
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

enum IMaterialTextureHasFlag : uint16_t
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
enum IMaterialAttributeFlag : uint16_t
{
    UnDefined = 0,
    Metal = 1,
    NonMetal = 2,
};

struct Material
{
    // 材质属性 IMaterialAttributeFlag
    uint16_t MaterialAttribute;
    // 贴图标记（是否准备好）
    uint16_t textureReadyFlags;

    //
    uint16_t DiffuseTextureId;
    // 法线贴图
    uint16_t NormalTextureId;
    // 镜面反射贴图
    uint16_t SpecularTextureId;
    // 自发光
    uint16_t EmissionTextureId;
    // 环境遮挡
    uint16_t AoTextureId;
    // 不透明度
    uint16_t OpacityTextureId;
    // 粗糙度
    uint16_t RoughnessTextureId;
    // 透明涂层
    uint16_t ClearCoatTextureId;

    //
    uint8_t padding[12];
};

enum InstanceOptionFlag : uint16_t 
{
    NoUse = 0,
    UseUV = 1 << 0, // 仅限 2d 精灵使用 instanceData.uv 而不是 vertex.uv
    ShowInstance = 1 << 1, // 显示实例
    AllMarerialTexIsDeffuse = 1 << 2 // 所有材质贴图都是漫反射贴图
};

struct InstanceData
{
    float4x4 model;
    InVertex* vertexAddress;
    Material* materialAddress;
    void* indexAddress; // 索引地址 数量大于等于 65536 时使用 uint32_t 索引 否则使用 uint16_t 索引
    void* boneAddress;  // 骨骼地址 ,暂时不支持
    float4 uv; // 2d 精灵使用的大图 uv
    float4 color; // 颜色
    uint32_t indexCount; // 索引数量
    uint32_t boneCount; // 骨骼数量
    uint32_t objectID; // 实例ID 为0时无效 计算着色器剔除该实例
    uint16_t optionFlags; // 实例选项
    uint16_t levelID;     // 实例所属关卡 ID
};

// vulkan 实例绘制命令
struct VkDrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};
