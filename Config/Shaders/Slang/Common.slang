#pragma once

enum VertexOptionalFlag : uint32_t
{
    HasNone = 0,
    HasUV = 1u << 0,
    HasNormal = 1u << 1,
    HasTangent = 1u << 2,
    HasBitangent = 1u << 3,
    HasColor = 1u << 4,
    HasBones = 1u << 5
};

struct InVertex
{
    uint32_t4 data0; // position(12) , flags(4)
    uint32_t4 data1; // uv(8) , normal(6) , pad(2)
    uint32_t4 data2; // tangent(6) , bitangent(6) , color(4)
    uint32_t4 data3; // bonesindex(8), bonesweight(8)
};

struct CalculatedVertex
{
    float3 position;
    float3 uv;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float4 color;
    uint32_t4x2 bonesindex; // 8 个骨骼索引
    float4x2 bonesweight;   // 8 个骨骼权重
};

enum IMaterialTextureHasFlag : uint
{
    HasBitAllNotHas = 0,
    HasBitDeffuse = 1 << 0,
    HasBitNormal = 1 << 1,
    HasBitSpecular = 1 << 2,
    HasBitEmission = 1 << 3,
    HasBitAo = 1 << 4,
    HasBitOpacity = 1 << 5,
    HasBitRoughness = 1 << 6,
    HasBitClearCoat = 1 << 7
};
enum IMaterialAttributeFlag : uint
{
    UnDefined = 0,
    Metal = 1,
    NonMetal = 2,
};

// Material 辅助访问器（替代 uint16_t 字段，内存布局不变）
uint GetMaterialAttribute(Material m)     { return m.packed0 & 0xFFFFu; }
uint GetTextureReadyFlags(Material m)     { return m.packed0 >> 16; }
uint GetDiffuseTextureId(Material m)      { return m.packed1 & 0xFFFFu; }
uint GetNormalTextureId(Material m)       { return m.packed1 >> 16; }
uint GetSpecularTextureId(Material m)     { return m.packed2 & 0xFFFFu; }
uint GetEmissionTextureId(Material m)     { return m.packed2 >> 16; }
uint GetAoTextureId(Material m)           { return m.packed3 & 0xFFFFu; }
uint GetOpacityTextureId(Material m)      { return m.packed3 >> 16; }
uint GetRoughnessTextureId(Material m)    { return m.packed4 & 0xFFFFu; }
uint GetClearCoatTextureId(Material m)    { return m.packed4 >> 16; }

void SetMaterialAttribute(inout Material m, uint v) { m.packed0 = (m.packed0 & 0xFFFF0000u) | (v & 0xFFFFu); }
void SetTextureReadyFlags(inout Material m, uint v) { m.packed0 = (m.packed0 & 0xFFFFu) | ((v & 0xFFFFu) << 16); }
void SetDiffuseTextureId(inout Material m, uint v)  { m.packed1 = (m.packed1 & 0xFFFF0000u) | (v & 0xFFFFu); }
void SetNormalTextureId(inout Material m, uint v)   { m.packed1 = (m.packed1 & 0xFFFFu) | ((v & 0xFFFFu) << 16); }
void SetSpecularTextureId(inout Material m, uint v) { m.packed2 = (m.packed2 & 0xFFFF0000u) | (v & 0xFFFFu); }
void SetEmissionTextureId(inout Material m, uint v) { m.packed2 = (m.packed2 & 0xFFFFu) | ((v & 0xFFFFu) << 16); }
void SetAoTextureId(inout Material m, uint v)       { m.packed3 = (m.packed3 & 0xFFFF0000u) | (v & 0xFFFFu); }
void SetOpacityTextureId(inout Material m, uint v)  { m.packed3 = (m.packed3 & 0xFFFFu) | ((v & 0xFFFFu) << 16); }
void SetRoughnessTextureId(inout Material m, uint v){ m.packed4 = (m.packed4 & 0xFFFF0000u) | (v & 0xFFFFu); }
void SetClearCoatTextureId(inout Material m, uint v){ m.packed4 = (m.packed4 & 0xFFFFu) | ((v & 0xFFFFu) << 16); }

struct Material
{
    // 材质属性（替代原来的 uint16_t 字段，两两打包为 uint，内存布局不变）
    // packed0: [15:0] = MaterialAttribute, [31:16] = textureReadyFlags
    // packed1: [15:0] = DiffuseTextureId,  [31:16] = NormalTextureId
    // packed2: [15:0] = SpecularTextureId, [31:16] = EmissionTextureId
    // packed3: [15:0] = AoTextureId,       [31:16] = OpacityTextureId
    // packed4: [15:0] = RoughnessTextureId,[31:16] = ClearCoatTextureId
    uint packed0;
    uint packed1;
    uint packed2;
    uint packed3;
    uint packed4;

    // 8 个 uint8 权重，打包为 2 个 uint（每个 uint 存 4 个 byte），内存布局不变
    uint packedWeights0; // weight[0..3]
    uint packedWeights1; // weight[4..7]

    uint _padding;       // 4 bytes 对齐填充（原 uint8_t padding[4]）
};

// 材质 diffuse 权重访问器（替代 uint8_t 数组）
uint GetDiffuseTextureWeight(Material m, uint index)
{
    uint packed = (index < 4u) ? m.packedWeights0 : m.packedWeights1;
    uint shift = (index & 3u) * 8u;
    return (packed >> shift) & 0xFFu;
}

enum InstanceOptionFlag : uint
{
    NoUse = 0,
    UseUV = 1 << 0, // 仅限 2d 精灵使用 instanceData.uv 而不是 vertex.uv
    ShowInstance = 1 << 1, // 显示实例
    AllMarerialTexIsDeffuse = 1 << 2, // 所有材质贴图都是漫反射贴图
    Use32BitIndex = 1 << 3             // 索引用 uint32_t；未设置则按 uint16_t 读取
};

// 骨骼占位结构体（暂时不使用）
struct BoneSkeletonPlaceHolder { uint _placeholder; };

struct InstanceData
{
    float4x4 model;
    InVertex* vertexAddress;
    Material* materialAddress;
    uint* indexAddress; // 索引缓冲区地址，通过 InstanceOptionFlag::Use32BitIndex 判断格式
    BoneSkeletonPlaceHolder* boneAddress;  // 骨骼占位，暂时不支持
    float4 uv; // 2d 精灵使用的大图 uv
    float4 color; // 颜色
    float4 AABBExtent; // 实例 AABB box [xyz] 为半径 [w] 预留(包围球半径，目前未使用)
    uint32_t indexCount; // 索引数量
    uint32_t boneCount; // 骨骼数量
    uint32_t objectID; // 实例ID 为0时无效 计算着色器剔除该实例
    // [15:0] = optionFlags, [31:16] = levelID（原 uint16_t x2 打包为 uint）
    uint packedOption_Level;
};

// InstanceData 辅助访问器
uint GetOptionFlags(InstanceData d) { return d.packedOption_Level & 0xFFFFu; }
uint GetLevelID(InstanceData d)     { return d.packedOption_Level >> 16; }

enum LightFlag : uint32_t
{
    UseNone = 0,
    UseParallelLight = 1 << 0,
    UsePointLight = 1 << 1
};

struct UBO
{
    float4x4 vp; // 视图投影矩阵
    float4 cameraDatas; // 相机数据 [xyz]: 相机位置 [w]: LOD 系数
    float4 parallelLightDatas; // 灯光数据 [xyz]: 平行光方向 [w]: 平行光强度
    float4 parallelLightColor; // 平行光颜色
    uint32_t pointLightCount; // 点光源数量
    uint32_t UseLightFlags; // 是否使用光源 LightFlag
};

// 使用 ScalarDataLayout 优化内存对齐，float3 不会对齐到 16 字节
struct PointLight
{
    float4 position_radius; // [xyz] 位置 [w] 半径
    float4 color;
    float4 attenuation; // [x] 衰减系数 [yzw] 预留(目前未使用)
};

struct SpotLight
{
    float3 position;
    float range;
    float4 color;
    float3 direction;      // ← 必加：聚光灯朝向
    float spotInnerCutoff; // ← 建议加：内锥角 cos
    float spotOuterCutoff; // spotCutoff 改名，外层截止 cos
    float spotExponent;
    float attenuation;
    uint32_t TextureSlot; // cookie/gobo 投影纹理
};

struct PushConstants
{
    uint32_t DrawObjectIndex; // 本次绘制对象索引
    uint32_t LevelID;         // 实例所属关卡 ID
    uint32_t BatchID;         // 实例所属批次 ID
};
[[vk_push_constant]] PushConstants gPushConstants;

// vulkan 实例绘制命令
struct VkDrawIndirectCommand {
    uint32_t vertexCount;   // 填入你要繪製的頂點數量 (例如 3 代表三角形)
    uint32_t instanceCount; // 實例數量 (通常為 1)
    uint32_t firstVertex;   // 起始頂點偏移
    uint32_t firstInstance; // 起始實例偏移
};
