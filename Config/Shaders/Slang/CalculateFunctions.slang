#pragma once

#include "Common.slang"

// ===== uint16_t 兼容层：通过 uint + 位运算模拟 =====

// 提取 uint 低 16 位
uint lo16(uint packed) { return packed & 0xFFFFu; }

// 提取 uint 高 16 位
uint hi16(uint packed) { return packed >> 16; }

// 将两个 16-bit 值打包到一个 uint：[15:0]=lo, [31:16]=hi
uint pack_u16x2(uint lo, uint hi) { return (hi << 16) | (lo & 0xFFFFu); }

// ===== float16_t 兼容层：IEEE 754 half → float 软件解码 =====

// 将 IEEE 754 半精度浮点数（uint 低 16 位）转换为 float
float half_to_float(uint h)
{
    h = h & 0xFFFFu;
    uint sign = (h >> 15) & 1u;
    uint exp = (h >> 10) & 0x1Fu;
    uint mant = h & 0x3FFu;
    uint fbits;

    if (exp == 0u)
    {
        // 零 或 非规格化数（简化:统一映射为带符号零,对法线精度影响可忽略）
        fbits = sign << 31;
    }
    else if (exp == 0x1Fu)
    {
        // 无穷大 或 NaN
        fbits = (sign << 31) | (0xFFu << 23) | (mant << 13);
    }
    else
    {
        // 规格化数: 指数偏移 -15 + 127 = +112
        fbits = (sign << 31) | ((exp + 112u) << 23) | (mant << 13);
    }
    return asfloat(fbits);
}

// 从一个 uint 解包两个 half → float2（替代 bit_cast<float16_t2>）
float2 unpack_half2(uint packed)
{
    return float2(half_to_float(packed), half_to_float(packed >> 16));
}

// 从两个 uint 解包四个 half → float4（替代 bit_cast<float16_t4>）
float4 unpack_half4(uint lo, uint hi)
{
    return float4(
        half_to_float(lo & 0xFFFFu),
        half_to_float(lo >> 16),
        half_to_float(hi & 0xFFFFu),
        half_to_float(hi >> 16));
}

// 从索引缓冲区读取顶点索引（兼容 uint16_t 和 uint32_t 格式，不依赖 16 位类型）
uint ReadIndex(uint* indexAddress, uint vertexId, bool use32Bit)
{
    uint* indexBuffer = indexAddress;
    if (use32Bit)
    {
        return indexBuffer[vertexId];
    }
    else
    {
        // uint16_t: 按字节偏移读取，兼容 2 字节对齐
        uint byteOffset = vertexId * 2;
        uint word = indexBuffer[byteOffset >> 2];
        uint shift = (byteOffset & 3u) * 8;
        return (word >> shift) & 0xFFFFu;
    }
}

// 从一个 uint 解包四个 uint8 → uint4（替代 bit_cast<uint8_t4>）
uint4 unpack_u8x4(uint packed)
{
    return uint4(
        packed & 0xFFu,
        (packed >> 8) & 0xFFu,
        (packed >> 16) & 0xFFu,
        (packed >> 24) & 0xFFu);
}

CalculatedVertex CalculateVertex(InVertex vertex)
{
    CalculatedVertex out;

    out.uv = float3(0.0);
    out.normal = float3(0.0, 0.0, 1.0); // 預設法線朝上
    out.tangent = float3(1.0, 0.0, 0.0);
    out.bitangent = float3(0.0, 1.0, 0.0);
    out.color = float4(1.0); // 預設白模
    out.bonesindex = uint32_t4x2(0);
    out.bonesweight = float4x2(0.0);

    out.position = bit_cast<float3>(vertex.data0.xyz);
    uint32_t flags = vertex.data0.w;

    if ((flags & VertexOptionalFlag::HasUV) != 0)
    {
        out.uv = float3(bit_cast<float32_t>(vertex.data1.x), bit_cast<float32_t>(vertex.data1.y), 0.0);
    }
    if ((flags & VertexOptionalFlag::HasNormal) != 0)
    {
        // 使用软件半精度解码替代 bit_cast<float16_t4>（GPU 不支持 Float16 时）
        float4 in_normal = unpack_half4(vertex.data1.z, vertex.data1.w);
        out.normal = float3(in_normal.xyz);
    }
    if ((flags & VertexOptionalFlag::HasTangent) != 0 || (flags & VertexOptionalFlag::HasBitangent) != 0)
    {
        // 將 3 個 uint32 分別精確解釋為 3 組 float16_t2
        // 每個 uint32 剛好容納兩個半精度浮點數（使用软件解码替代 bit_cast）
        float2 p0 = unpack_half2(vertex.data2.x); // [Tangent.x,   Tangent.y]
        float2 p1 = unpack_half2(vertex.data2.y); // [Tangent.z,   Bitangent.x]
        float2 p2 = unpack_half2(vertex.data2.z); // [Bitangent.y, Bitangent.z]

        if ((flags & VertexOptionalFlag::HasTangent) != 0)
        {
            out.tangent = float3(p0.x, p0.y, p1.x);
        }
        if ((flags & VertexOptionalFlag::HasBitangent) != 0)
        {
            out.bitangent = float3(p1.y, p2.x, p2.y);
        }
    }
    if ((flags & VertexOptionalFlag::HasColor) != 0)
    {
        uint4 in_color = unpack_u8x4(vertex.data2.w);
        out.color = float4(in_color) / 255.0;
    }
    if ((flags & VertexOptionalFlag::HasBones) != 0)
    {
        uint4 idx_part0 = unpack_u8x4(vertex.data3.x); // 前 4 個骨骼
        uint4 idx_part1 = unpack_u8x4(vertex.data3.y); // 後 4 個骨骼
        out.bonesindex._m00 = uint32_t(idx_part0.x);
        out.bonesindex._m01 = uint32_t(idx_part0.y);
        out.bonesindex._m10 = uint32_t(idx_part0.z);
        out.bonesindex._m11 = uint32_t(idx_part0.w);
        out.bonesindex._m20 = uint32_t(idx_part1.x);
        out.bonesindex._m21 = uint32_t(idx_part1.y);
        out.bonesindex._m30 = uint32_t(idx_part1.z);
        out.bonesindex._m31 = uint32_t(idx_part1.w);

        uint4 weight_part0 = unpack_u8x4(vertex.data3.z); // 前 4 個權重
        uint4 weight_part1 = unpack_u8x4(vertex.data3.w); // 後 4 個權重
        out.bonesweight._m00 = float(weight_part0.x) / 255.0;
        out.bonesweight._m01 = float(weight_part0.y) / 255.0;
        out.bonesweight._m10 = float(weight_part0.z) / 255.0;
        out.bonesweight._m11 = float(weight_part0.w) / 255.0;
        out.bonesweight._m20 = float(weight_part1.x) / 255.0;
        out.bonesweight._m21 = float(weight_part1.y) / 255.0;
        out.bonesweight._m30 = float(weight_part1.z) / 255.0;
        out.bonesweight._m31 = float(weight_part1.w) / 255.0;
    }

    return out;
}

// Slang 标准库未提供 inverse(), 这里手写列主序 (column-major) 4x4 逆矩阵
float4x4 mat4_inverse(float4x4 m)
{
    float Coef00 = m[2].z * m[3].w - m[2].w * m[3].z;
    float Coef02 = m[1].z * m[3].w - m[1].w * m[3].z;
    float Coef03 = m[1].z * m[2].w - m[1].w * m[2].z;
    float Coef05 = m[2].y * m[3].w - m[2].w * m[3].y;
    float Coef06 = m[1].y * m[3].w - m[1].w * m[3].y;
    float Coef07 = m[1].y * m[2].w - m[1].w * m[2].y;
    float Coef08 = m[2].y * m[3].z - m[2].z * m[3].y;
    float Coef10 = m[1].y * m[3].z - m[1].z * m[3].y;
    float Coef11 = m[1].y * m[2].z - m[1].z * m[2].y;
    float Coef12 = m[2].x * m[3].w - m[2].w * m[3].x;
    float Coef14 = m[1].x * m[3].w - m[1].w * m[3].x;
    float Coef15 = m[1].x * m[2].w - m[1].w * m[2].x;
    float Coef16 = m[2].x * m[3].z - m[2].z * m[3].x;
    float Coef18 = m[1].x * m[3].z - m[1].z * m[3].x;
    float Coef19 = m[1].x * m[2].z - m[1].z * m[2].x;
    float Coef20 = m[2].x * m[3].y - m[2].y * m[3].x;
    float Coef22 = m[1].x * m[3].y - m[1].y * m[3].x;
    float Coef23 = m[1].x * m[2].y - m[1].y * m[2].x;

    float4 Fac0 = float4(Coef00, Coef00, Coef02, Coef03);
    float4 Fac1 = float4(Coef05, Coef05, Coef06, Coef07);
    float4 Fac2 = float4(Coef08, Coef08, Coef10, Coef11);
    float4 Fac3 = float4(Coef12, Coef12, Coef14, Coef15);
    float4 Fac4 = float4(Coef16, Coef16, Coef18, Coef19);
    float4 Fac5 = float4(Coef20, Coef20, Coef22, Coef23);

    float4 Vec0 = float4(m[1].x, m[0].x, m[0].x, m[0].x);
    float4 Vec1 = float4(m[1].y, m[0].y, m[0].y, m[0].y);
    float4 Vec2 = float4(m[1].z, m[0].z, m[0].z, m[0].z);
    float4 Vec3 = float4(m[1].w, m[0].w, m[0].w, m[0].w);

    float4 Inv0 = float4(Vec1.x * Fac0.x - Vec2.x * Fac1.x + Vec3.x * Fac2.x,
                         Vec1.y * Fac0.x - Vec2.y * Fac1.x + Vec3.y * Fac2.x,
                         Vec1.z * Fac0.x - Vec2.z * Fac1.x + Vec3.z * Fac2.x,
                         Vec1.w * Fac0.x - Vec2.w * Fac1.x + Vec3.w * Fac2.x);

    float4 Inv1 = float4(Vec0.x * Fac0.y - Vec2.x * Fac3.x + Vec3.x * Fac4.x,
                         Vec0.y * Fac0.y - Vec2.y * Fac3.x + Vec3.y * Fac4.x,
                         Vec0.z * Fac0.y - Vec2.z * Fac3.x + Vec3.z * Fac4.x,
                         Vec0.w * Fac0.y - Vec2.w * Fac3.x + Vec3.w * Fac4.x);

    float4 Inv2 = float4(Vec0.x * Fac1.y - Vec1.x * Fac3.y + Vec3.x * Fac5.x,
                         Vec0.y * Fac1.y - Vec1.y * Fac3.y + Vec3.y * Fac5.x,
                         Vec0.z * Fac1.y - Vec1.z * Fac3.y + Vec3.z * Fac5.x,
                         Vec0.w * Fac1.y - Vec1.w * Fac3.y + Vec3.w * Fac5.x);

    float4 Inv3 = float4(Vec0.x * Fac2.y - Vec1.x * Fac4.y + Vec2.x * Fac5.y,
                         Vec0.y * Fac2.y - Vec1.y * Fac4.y + Vec2.y * Fac5.y,
                         Vec0.z * Fac2.y - Vec1.z * Fac4.y + Vec2.z * Fac5.y,
                         Vec0.w * Fac2.y - Vec1.w * Fac4.y + Vec2.w * Fac5.y);

    float4 SignA = float4(1.0, -1.0, 1.0, -1.0);
    float4 SignB = float4(-1.0, 1.0, -1.0, 1.0);
    float4x4 Inverse = float4x4(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);

    float4 Row0 = float4(m[0].x, m[1].x, m[2].x, m[3].x);
    float Determinant = dot(Row0, Inverse[0]);
    return Inverse / Determinant;
}