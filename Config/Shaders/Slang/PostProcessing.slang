// ============================================================================
// 后处理 (Post Processing) —— 全屏三角形 pass
//
// 默认行为: 什么都不做, 直接把主场景离屏颜色原样输出 (passthrough).
// 需要加效果时, 在 FSFullscreen 里对 sceneColor 做处理即可 (bloom / tonemap / ...).
//
// 与 IVulkanBase::CreateGlobalPipelineLayout 严格对应 (与 MainMesh.slang 同一套 layout):
//   set0 binding 0 : ConstantBuffer<UBO>   (相机数据; 默认不做处理时未使用, 预留)
//   set1 binding 0 : 全局采样器
//   set1 binding 1 : Bindless 纹理数组
//
// 后处理源纹理: 主场景离屏颜色 RT (IVulkanRenderer::_mainRTs[frameindex]),
// 它以 "外部渲染目标槽位" 注册进 bindless (IVulkanTexture2DManagement::UpdateTexture2D),
// 槽位号通过 push constant 传入:
//   gPushConstants.DrawObjectIndex == 源纹理 bindless slot
// (后处理 pass 没有 "本次绘制对象索引" 的语义, 因此复用该字段传递源纹理槽位,
//  不额外新增 GPU 结构; 见 IVulkanRenderer::RenderFrame 中后处理部分)
//
// 编译 (输出路径与 IVulkanRenderer.cpp 中的常量保持一致):
//   slangc Config/Shaders/Slang/PostProcessing.slang -target spirv -entry VSFullscreen -stage vertex   -o Config/Shaders/SPV/PostProcessing.vert.spv
//   slangc Config/Shaders/Slang/PostProcessing.slang -target spirv -entry FSFullscreen -stage fragment -o Config/Shaders/SPV/PostProcessing.frag.spv
// ============================================================================

#include "Common.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> gUBO;
[[vk::binding(0, 1)]] SamplerState gSampler;
[[vk::binding(1, 1)]] Texture2D gTexture[];

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : UV;
};

// 全屏大三角形: 无需顶点缓冲, 由 vkCmdDraw(cmd, 3, 1, 0, 0) 直接绘制
[shader("vertex")]
VSOutput VSFullscreen(uint vertexId : SV_VertexID)
{
    // NDC 覆盖整个屏幕 (Vulkan 下 -1 为左上, uv 原点在左上, 与纹理 v 方向一致)
    static const float2 kPosition[3] =
    {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    static const float2 kUv[3] =
    {
        float2(0.0, 0.0),
        float2(2.0, 0.0),
        float2(0.0, 2.0)
    };

    VSOutput output;
    output.position = float4(kPosition[vertexId], 0.0, 1.0);
    output.uv = kUv[vertexId];
    return output;
}

[shader("fragment")]
float4 FSFullscreen(VSOutput input) : SV_Target
{
    // 主场景离屏颜色 (bindless 槽位由 push constant 传入)
    uint sourceSlot = gPushConstants.DrawObjectIndex;

    float4 sceneColor = gTexture[NonUniformResourceIndex(sourceSlot)].Sample(gSampler, input.uv);

    // 默认: 什么都不做, 原样输出
    return sceneColor;
}
