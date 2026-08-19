#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>

namespace INVENT
{
	class IVulkanRenderer 
	{
	public:
		static bool Init();
		static void Shutdown();

		static void BeginRender();
		static void EndRender();

		static void RenderFrame(std::uint32_t frameindex);

	private:
		static bool _init_pipelines();
		static void _clear();

	private:

		// compute
		inline static VkPipeline _compute_pipeline = VK_NULL_HANDLE;
		// main
		inline static VkPipeline _main_opaque_pipeline = VK_NULL_HANDLE;

		inline static std::vector<VkFence> _frame_fences;
		inline static std::vector<VkSemaphore> _acquire_semaphores;
		inline static std::vector<VkSemaphore> _submit_semaphores;
		inline static std::vector<VkCommandBuffer> _command_buffers;

		inline static std::vector<VkDescriptorSet> _ubo;
		inline static std::vector<VkDescriptorSet> _point_light_ssbo;
		inline static std::vector<VkDescriptorSet> _instance_ssbo;
		inline static std::vector<VkDescriptorSet> _out_instance_ssbo;

		inline static std::uint32_t _image_index{ 0 };
	};
}
