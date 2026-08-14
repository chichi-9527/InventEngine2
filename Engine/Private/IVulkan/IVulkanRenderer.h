#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace INVENT
{
	class IVulkanRenderer 
	{
	public:
		static bool Init();
		static void Shutdown();

	private:
		static bool _init_pipelines();

	private:

		// main
		inline static VkPipeline _main_opaque_pipeline = VK_NULL_HANDLE;

		inline static std::vector<VkFence> _frame_fences;
		inline static std::vector<VkSemaphore> _acquire_semaphores;
		inline static std::vector<VkSemaphore> _submit_semaphores;
		inline static std::vector<VkCommandBuffer> _command_buffers;

	};
}
