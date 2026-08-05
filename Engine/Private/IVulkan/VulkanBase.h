#pragma once

#include "VulkanConfig.h"
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace INVENT
{
	class IVulkanBase
	{
	
		IVulkanBase() = default;

	public:
		~IVulkanBase();

		static void AddValidationLayer(const char* layerName);
		static void AddInstanceExtension(const char* extensionName);
		static void AddDeviceExtension(const char* extensionName);

		static IVulkanBase& Base();

		bool CreateVulkanInstance();


		const std::uint32_t GetAPIVersion() const noexcept { return _api_version; }
		bool Version_1_3_OrHigher() const noexcept { return _api_version >= VK_API_VERSION_1_3; }
	private:
		const std::uint32_t _use_lastest_api_version();

#ifdef ENGINE_DEVELOPMENT_MODE
		bool _check_validation_layers();
		bool _setup_debug_messenger();
#endif // !ENGINE_DEVELOPMENT_MODE
	private:
		VkInstance _instance = VK_NULL_HANDLE;
		VkDevice _device = VK_NULL_HANDLE;
		VkSurfaceKHR _surface = VK_NULL_HANDLE;
		VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
		VkQueue _graphics_queue = VK_NULL_HANDLE;
		VkQueue _present_queue = VK_NULL_HANDLE;
		VkSwapchainKHR _swap_chain = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
		VkDescriptorPool _bindless_descriptor_pool = VK_NULL_HANDLE;
		VkPipelineLayout _global_pipeline_layout = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> _descriptor_set_layouts;
		VkDescriptorSet  _global_bindless_descriptor_set = VK_NULL_HANDLE;
		VkCommandPool _command_pool = VK_NULL_HANDLE;

		std::uint32_t _api_version{ VK_API_VERSION_1_0 };

	};

	
}
