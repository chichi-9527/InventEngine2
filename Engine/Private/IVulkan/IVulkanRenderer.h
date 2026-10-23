#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>
#include <chrono>

namespace INVENT
{
	/*
	* 渲染流程 (渲染线程独占, 每帧三段式):
	*
	*   BeginRender(f) : 等 frameFence[f] -> 流式三连 -> acquire -> BindMipFeedback -> 开始录制
	*   RenderFrame(f) : 录制 acquire屏障 -> 反馈CS -> quad 绘制 -> present 转换
	*   EndRender()    : 结束录制 -> 提交 -> present
	*
	* 测试阶段: 硬编码 quad + 单实例(纹理 slot) + 反馈CS替身
	* 之后: RenderFrame 里换成 裁剪CS + GPU驱动间接绘制, 帧结构不变
	*/
	class IVulkanRenderer
	{
		struct U_SS_BO 
		{
			VkBuffer buffer = VK_NULL_HANDLE;
			void* mapped = nullptr;
		};

	public:
		static bool Init();
		static void Shutdown();

		static void BeginRender(std::uint32_t frameindex);
		static void RenderFrame(std::uint32_t frameindex);
		static void EndRender();

		static void SetClearColor(VkClearColorValue v) { _vk_clear.color = v; }
	private:
		static bool _init_buffers();			// ubo(预留) + 测试实例 + 占位 ssbo
		static bool _init_descriptor_sets();	// set0 x 帧数 (一个 set 含全部 binding!)
		static bool _init_frames();			// fence/semaphore/命令缓冲
		static bool _init_pipelines();		// 测试图形管线 + 反馈计算管线
		static bool _init_test_resources();	// 全局采样器 + set1 + 测试纹理
		static void _record_swapchain_barrier(VkCommandBuffer cmd, VkImage image, bool to_present);
		static void _record_depth_barrier(VkCommandBuffer cmd, VkImage depth_image);
		static void _write_buffer_binding(VkDescriptorSet set, std::uint32_t binding, VkBuffer buffer, bool is_ubo);
		static void _upload_per_frame_data(std::uint32_t frame_index);
		static bool _recreate_swapchain();
		static void _clear();

	private:
		// compute (反馈替身, 之后 = 视锥裁剪 CS)
		inline static VkPipeline _compute_pipeline = VK_NULL_HANDLE;
		// main (测试 quad)
		inline static VkPipeline _main_opaque_pipeline = VK_NULL_HANDLE;

		inline static std::vector<VkFence> _frame_fences;
		inline static std::vector<VkSemaphore> _acquire_semaphores;	// 每帧槽
		inline static std::vector<VkSemaphore> _submit_semaphores;	// 每交换链图像
		inline static std::vector<VkCommandBuffer> _command_buffers;

		// set0: 每帧一份 (一个 set 包含 binding 0~4 全部)
		inline static std::vector<VkDescriptorSet> _set0;

		// 每帧 ubo;
		inline static std::vector<U_SS_BO> _ubo_buffers;
		// 未使用 binding (1灯光/3间接命令) 的占位, 防未更新告警
		inline static std::vector<U_SS_BO> _point_light_buffers;
		// binding 3 间接命令 (含每个批次的 drawCount)
		inline static std::vector<U_SS_BO> _draw_count_and_indirect_buffers;

		// 测试实例数据: [0] = 测试纹理 slot; 接真实渲染时换成 InstanceBuffer 的 ssbo
		inline static VkBuffer _test_instance_buffer = VK_NULL_HANDLE;
		inline static void* _test_instance_mapped = nullptr;
		

		inline static VkClearValue _vk_clear{};

		inline static VkSampler _global_sampler = VK_NULL_HANDLE;
		inline static std::uint32_t _test_texture_slot = UINT32_MAX;

		inline static std::chrono::steady_clock::time_point _recreate_cooldown_until;

		inline static std::uint32_t _image_index{ 0 };
		inline static std::uint32_t _current_frame{ 0 };	// EndRender 用
		inline static bool _frame_skipped{ false };
		inline static bool _swapchain_dirty{ false };
		inline static std::uint64_t _debug_frame{ 0 };
	};
}
