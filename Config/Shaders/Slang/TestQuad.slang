// ===== 全局绑定, 与 IVulkanBase::CreateGlobalPipelineLayout 严格对应 =====

// set0 binding 2 : 实例数据 SSBO (测试最小版: [instanceIndex] = 该实例的 bindless texture slot)
// 接真实渲染时换成 StructuredBuffer<InstanceData>
[[vk::binding(2, 0)]]
StructuredBuffer<uint> g_instanceTextureSlots;

// set0 binding 4 : 纹理 mip 反馈 (由 TestMipFeedback CS 写)
[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> g_textureDesiredMip;

// set1 binding 0 : 全局采样器
[[vk::binding(0, 1)]]
SamplerState g_globalSampler;

// set1 binding 1 : bindless 贴图数组 (无界数组, 对应 variable descriptor count)
[[vk::binding(1, 1)]]
Texture2D<float4> g_texture2DArray[];

struct VSOut
{
	float4 position : SV_Position;
	float2 uv       : UV;
	nointerpolation uint slot : TEX_SLOT;
};

[shader("vertex")]
VSOut TestQuadVS(uint vertexId: SV_VertexID, uint instanceId: SV_InstanceID)
{
	// NDC 中心 quad (管线 CullMode = NONE, 无绕序烦恼)
	static const float2 kPos[6] = {
		float2(-0.5, -0.5), float2(0.5, -0.5), float2(0.5, 0.5),
		float2(-0.5, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
	static const float2 kUv[6] = {
		float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
		float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0) };

	VSOut o;
	o.position = float4(kPos[vertexId], 0.0, 1.0);
	o.uv       = kUv[vertexId];
	o.slot     = g_instanceTextureSlots[instanceId];
	return o;
}

[shader("fragment")]
float4 TestQuadFS(VSOut i) : SV_Target
{
	// 隐式导数采样: 流式窗口 image 自动适配 LOD, 无需 baseMip
	return g_texture2DArray[NonUniformResourceIndex(i.slot)].Sample(g_globalSampler, i.uv);
}
