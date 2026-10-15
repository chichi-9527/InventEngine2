
#include "Common.slang"

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
        float16_t4 in_normal = bit_cast<float16_t4>(vertex.data1.zw);
        out.normal = float3(in_normal.xyz);
    }
    if ((flags & VertexOptionalFlag::HasTangent) != 0 || (flags & VertexOptionalFlag::HasBitangent) != 0)
    {
        // 將 3 個 uint32 分別精確解釋為 3 個 float16_t2
        // 每個 uint32 剛好容納兩個半精度浮點數
        float16_t2 p0 = bit_cast<float16_t2>(vertex.data2.x); // [Tangent.x,   Tangent.y]
        float16_t2 p1 = bit_cast<float16_t2>(vertex.data2.y); // [Tangent.z,   Bitangent.x]
        float16_t2 p2 = bit_cast<float16_t2>(vertex.data2.z); // [Bitangent.y, Bitangent.z]

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
        uint8_t4 in_color = bit_cast<uint8_t4>(vertex.data2.w);
        out.color = float4(in_color) / 255.0;
    }
    if ((flags & VertexOptionalFlag::HasBones) != 0)
    {
        uint8_t4 idx_part0 = bit_cast<uint8_t4>(vertex.data3.x); // 前 4 個骨骼
        uint8_t4 idx_part1 = bit_cast<uint8_t4>(vertex.data3.y); // 後 4 個骨骼
        out.bonesindex._m00 = uint32_t(idx_part0.x);
        out.bonesindex._m01 = uint32_t(idx_part0.y);
        out.bonesindex._m10 = uint32_t(idx_part0.z);
        out.bonesindex._m11 = uint32_t(idx_part0.w);
        out.bonesindex._m20 = uint32_t(idx_part1.x);
        out.bonesindex._m21 = uint32_t(idx_part1.y);
        out.bonesindex._m30 = uint32_t(idx_part1.z);
        out.bonesindex._m31 = uint32_t(idx_part1.w);

        uint8_t4 weight_part0 = bit_cast<uint8_t4>(vertex.data3.z); // 前 4 個權重
        uint8_t4 weight_part1 = bit_cast<uint8_t4>(vertex.data3.w); // 後 4 個權重
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
