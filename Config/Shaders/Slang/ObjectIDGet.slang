#pragma once

#include "Common.slang"
#include "CalculateFunctions.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> gUBO;
[[vk::binding(5, 0)]] RWStructuredBuffer<ObjectIDGetBack> gObjectIDGetBack;

[[vk::binding(0, 1)]] SamplerState gSampler;
[[vk::binding(1, 1)]] Texture2D gTexture[];

[numthreads(1, 1, 1)]
[shader("compute")]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ObjectIDGetBack back = gObjectIDGetBack[0];

    if (back.inPacked.w == 0)
        return;

    // inPacked: [xy]=鼠标像素坐标(float位) [z]=depth<<16|objIdSlot [w]=是否执行
    float2 mouseCoord = asfloat(back.inPacked.xy);
    uint   objTexSlot = back.inPacked.z & 0xFFFFu;
    uint   depthSlot  = (back.inPacked.z >> 16) & 0xFFFFu;
    
    uint rtWidth, rtHeight;
    gTexture[NonUniformResourceIndex(objTexSlot)].GetDimensions(rtWidth, rtHeight);

    if (mouseCoord.x < 0.0 || mouseCoord.y < 0.0 ||
        mouseCoord.x >= float(rtWidth) || mouseCoord.y >= float(rtHeight))
    {
        back.outPacked = uint4(0, 0, 0, 0);
        gObjectIDGetBack[0] = back;
        return;
    }

    int2 pixelCoord = int2(int(mouseCoord.x), int(mouseCoord.y));

    // R32_UINT ObjectID RT 用 asuint 位重解释读取, 避免 float->uint 隐式转换
    uint objectID = asuint(gTexture[NonUniformResourceIndex(objTexSlot)].Load(int3(pixelCoord, 0)).x);

    float3 worldPosition = float3(0.0, 0.0, 0.0);

    if (objectID != 0)
    {
        float2 uv = float2(mouseCoord.x / float(rtWidth), mouseCoord.y / float(rtHeight));

        // 深度 RT 注册到 bindless 后
        float depth = gTexture[NonUniformResourceIndex(depthSlot)].Load(int3(pixelCoord, 0)).x;

        float4x4 invVP = transpose(mat4_inverse(gUBO.vp));
        float4 clipPos = float4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth, 1.0);
        float4 worldPos = mul(invVP, clipPos);
        worldPosition = worldPos.xyz / worldPos.w;
    }

    // outPacked: [xyz]=wPosition (float位) [w]=objectID
    back.outPacked = uint4(asuint(worldPosition.x), asuint(worldPosition.y), asuint(worldPosition.z), objectID);
    gObjectIDGetBack[0] = back;
}