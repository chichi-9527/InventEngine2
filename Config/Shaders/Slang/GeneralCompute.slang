#pragma once

// 通用計算著色器: 用於處理通用計算任務, 紋理 mip 反饋, 實例繪製命令.
#include "Common.slang"
#include "CalculateFunctions.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> gUBO;
[[vk::binding(2, 0)]] StructuredBuffer<InstanceData, ScalarDataLayout> gInstanceData; // 實例數據 SSBO

// 將計數器與命令陣列合併到同一個寫回 Buffer 中
// 實例繪製命令 Buffer (SSBO) 前 4*32 个字节為 drawCount(uint32_t) (与cpp端批次数量一致32个，修改时需要同步更新cpp端) 
// 後面為命令陣列(VkDrawIndirectCommand)
[[vk::binding(3, 0)]] RWByteAddressBuffer gIndirectArgsBuffer;

// 單純的 uint 陣列，長度等於 Bindless 紋理的最大數量
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> gTextureDesiredMip; // 紋理 mip 反饋 SSBO

// LevelID 通配值
static const uint kLevelID_All = 0xFFFFu;
// 最大 mip 等級 (對應 4096 分辨率)
static const uint32_t kMaxMipLevel = 12;
// 預留的最大批次數量上限（128 位元組計數器區）
static const uint32_t kMaxBatchCount = 32;

struct Frustum
{
    float4 planes[6]; 
};

// 提取 6 個視錐平面
Frustum ExtractFrustum(float4x4 vp)
{
    float4 row0 = vp[0];
    float4 row1 = vp[1];
    float4 row2 = vp[2];
    float4 row3 = vp[3];

    Frustum f;
    f.planes[0] = row3 + row0; // 左
    f.planes[1] = row3 - row0; // 右
    f.planes[2] = row3 + row1; // 下
    f.planes[3] = row3 - row1; // 上
    f.planes[4] = row2;        // 近
    f.planes[5] = row3 - row2; // 遠

    for (uint32_t i = 0; i < 6; i++)
    {
        f.planes[i] /= length(f.planes[i].xyz);
    }
    return f;
}

// 視錐裁剪測試
bool IsAABBCulled(Frustum frustum, float3 center, float3 halfExtent)
{
    for (uint32_t i = 0; i < 6; i++)
    {
        float3 n = frustum.planes[i].xyz;
        float dist = dot(n, center) + frustum.planes[i].w;
        float radius = dot(abs(n), halfExtent);
        if (dist + radius < 0.0)
            return true; 
    }
    return false;
}

// 局部 AABB 半邊長轉到世界空間
float3 TransformHalfExtent(float4x4 model, float3 localHalfExtent)
{
    float3 result;
    result.x = abs(model[0][0]) * localHalfExtent.x + abs(model[0][1]) * localHalfExtent.y + abs(model[0][2]) * localHalfExtent.z;
    result.y = abs(model[1][0]) * localHalfExtent.x + abs(model[1][1]) * localHalfExtent.y + abs(model[1][2]) * localHalfExtent.z;
    result.z = abs(model[2][0]) * localHalfExtent.x + abs(model[2][1]) * localHalfExtent.y + abs(model[2][2]) * localHalfExtent.z;
    return result;
}

// LevelID 過濾
bool PassesLevelFilter(uint instanceLevelID, uint filterLevelID)
{
    if (filterLevelID == kLevelID_All)
        return true;
    return instanceLevelID == filterLevelID;
}

// 計算流式紋理最精細 mip 等級
uint32_t ComputeDesiredMip(float3 worldCenter, float boundingRadius)
{
    float dist = length(gUBO.cameraDatas.xyz - worldCenter);

    // 使用 UBO.cameraDatas.w (LodFactor)
    float screenDiameterPixels = (2.0 * boundingRadius) * gUBO.cameraDatas.w / max(dist, 0.0001);

    // mip 下降级数（分辨率无关）：screenPixels 越大，需要越少下降 = 纹理越高清
    // C++ 端换算: actualMip = textureMaxMip - clampedDrop
    float drop = log2(max(screenDiameterPixels, 1.0));
    return uint32_t(clamp(floor(drop), 0.0, float(kMaxMipLevel)));
}

[numthreads(64, 1, 1)]
[shader("compute")]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t instanceIndex = gPushConstants.DrawObjectIndex + dispatchThreadId.x;
    InstanceData instance = gInstanceData[instanceIndex];

    // 1. 有效性檢查
    if (instance.objectID == 0)
        return;

    // 2. 關卡過濾
    if (!PassesLevelFilter(GetLevelID(instance), gPushConstants.LevelID))
        return;

    // 3. 視錐裁剪
    Frustum frustum = ExtractFrustum(gUBO.vp);

    float3 worldCenter = instance.model[3].xyz; // 直接取矩陣第四列平移
    float3 worldHalfExtent = TransformHalfExtent(instance.model, instance.AABBExtent.xyz);

    if (IsAABBCulled(frustum, worldCenter, worldHalfExtent))
        return;

    // 4. 計算 Mip 等級
    float boundingRadius = length(worldHalfExtent); 
    uint32_t desiredMip = ComputeDesiredMip(worldCenter, boundingRadius);

    // 利用物體材質中的 DiffuseTextureId 作為 Bindless 陣列的索引
    // 注意：這裡需要通過材質地址取得 DiffuseTextureId
    uint textureId = GetDiffuseTextureId(instance.materialAddress[0]);

    // 透過原子操作取最小值，保證 C++ 端拿到的是所有看見該紋理的物件中，最渴望高清（Mip 最低）的等級
    InterlockedMin(gTextureDesiredMip[textureId], desiredMip);

    // 当前批次的 drawCount 變數器偏移量
    uint32_t counterByteOffset = uint32_t(gPushConstants.BatchID) * 4;

    // 5. 原子追加繪製命令
    uint32_t slot = 0;
    gIndirectArgsBuffer.InterlockedAdd(counterByteOffset, 1, slot); // 增加 drawCount 變數器

    uint32_t absoluteSlot = gPushConstants.DrawObjectIndex + slot; // 复用 DrawObjectIndex 作為 BaseCommandSlot

    // 6. 計算該命令在陣列中的字節偏移量并寫回
    uint32_t allCountersSize = kMaxBatchCount * 4; // 128 字節
    uint32_t cmdByteOffset = allCountersSize + absoluteSlot * 16; // 16 字節/命令 (VkDrawIndirectCommand)

    VkDrawIndirectCommand cmd;
    cmd.vertexCount    = instance.indexCount;
    cmd.instanceCount  = 1;
    cmd.firstVertex    = 0;
    cmd.firstInstance  = instanceIndex;
    gIndirectArgsBuffer.Store<VkDrawIndirectCommand>(cmdByteOffset, cmd); 
}
