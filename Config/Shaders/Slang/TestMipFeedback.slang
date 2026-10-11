// 视锥裁剪 CS 的最小替身: 真实版本在可见实例分支按屏幕覆盖率算 mip 并 InterlockedMin;
// 这里只把实例 0 的纹理需求写成 mip0, 验证 [CS -> 反馈 -> readback -> 升级] 整条回路
[[vk::binding(2, 0)]]
StructuredBuffer<uint> g_instanceTextureSlots;

[[vk::binding(4, 0)]]
RWStructuredBuffer<uint> g_textureDesiredMip;

[shader("compute")]
[numthreads(1, 1, 1)]
void TestMipFeedbackCS(uint3 tid : SV_DispatchThreadID)
{
	uint slot = g_instanceTextureSlots[0];
	uint original;
	InterlockedMin(g_textureDesiredMip[slot], 0u, original);
}
