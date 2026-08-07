#include "IVulkan/IRenderThread.h"

#include "ILog.h"
#include "IVulkan/VulkanBase.h"

namespace INVENT
{
	IRenderThreadBase& IRenderThreadBase::Instance()
	{
		static IRenderThread r;
		return r;
	}

	bool IRenderThread::Start()
	{
		INVENT_LOG_INFO("[IRenderThread] start.");
		if (this->_create_surface == nullptr)
		{
			INVENT_LOG_ERROR("you need set create surface function before start!");
			return false;
		}

		IVulkanBase::AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		if (!IVulkanBase::Base().CreateVulkanInstance())
		{
			return false;
		}
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (VkResult result = _create_surface(IVulkanBase::Base().GetVkInstance(), nullptr, &surface))
		{
			INVENT_LOG_ERROR(std::format("[IRenderThread] Failed to create a window surface! VkResult: {}.", static_cast<std::uint32_t>(result)));
			return false;
		}
		IVulkanBase::Base().SetSurface(surface);
		// init vulkan
		if (!IVulkanBase::Base().PickPhysicalDevice() ||
			!IVulkanBase::Base().CreateLogicalDevice() ||
			!IVulkanBase::Base().CreateSwapChain() ||
			!IVulkanBase::Base().CreateSwapChainImageView() ||
			!IVulkanBase::Base().CreateVmaAllocator() ||
			!IVulkanBase::Base().FindDepthFormat() ||
			!IVulkanBase::Base().InitializeAllOffscreenPasses() ||
			!IVulkanBase::Base().CreateGlobalPipelineLayout() ||
			!IVulkanBase::Base().CreateBindlessDescriptorPool() ||
			!IVulkanBase::Base().AllocaGlobalBindlessDescriptorSet() ||
			!IVulkanBase::Base().CreateCommandPool())
		{
			return false;
		}

		return true;
	}

	void IRenderThread::Shutdown()
	{
		INVENT_LOG_INFO("[IRenderThread] shutdown.");
	}

	void IRenderThreadBase::SetVulkanInstanceExtension(const char* extension_name)
	{
		IVulkanBase::AddInstanceExtension(extension_name);
	}
}
