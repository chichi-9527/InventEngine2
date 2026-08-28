#include "IVulkan/IRenderThread.h"

#include "ILog.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/IVulkanRenderer.h"

namespace INVENT
{
	IRenderThreadBase& IRenderThreadBase::Instance()
	{
		static IRenderThread r;
		return r;
	}

	bool IRenderThread::Start()
	{
		_running = true;
		if (!_init_vulkan())
		{
			return false;
		}
		if (!IVulkanRenderer::Init())
		{
			INVENT_LOG_ERROR("[IRenderThread] Init Vulkan renderer error.");
			return false;
		}

		return true;
	}

	void IRenderThread::Shutdown()
	{
		IVulkanRenderer::Shutdown();
		INVENT_LOG_INFO("[IRenderThread] shutdown.");
	}

	bool IRenderThread::_init_vulkan()
	{
		if (this->_create_surface == nullptr)
		{
			INVENT_LOG_ERROR("you need set create surface function before start!");
			return false;
		}

		if (!IVulkanBase::InitVmaAllocationCache()) return false;

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
			!IVulkanBase::Base().InitDescriptorCounts() ||
			!IVulkanBase::Base().CreateGlobalPipelineLayout() ||
			!IVulkanBase::Base().CreateBindlessDescriptorPool() ||
			!IVulkanBase::Base().CreateOtherDsecriptorPools() ||
			!IVulkanBase::Base().AllocaGlobalBindlessDescriptorSet() ||
			!IVulkanBase::Base().CreateCommandPool())
		{
			return false;
		}

		return true;
	}

	void IRenderThreadBase::SetVulkanInstanceExtension(const char* extension_name)
	{
		IVulkanBase::AddInstanceExtension(extension_name);
	}

	void IRenderThreadBase::SetVulkanWaitForWindowEventsFunction(void(*func)())
	{
		IVulkanBase::Base().SetWaitForWindowEventsFunction(func);
	}
}
