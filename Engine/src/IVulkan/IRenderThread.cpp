#include "IVulkan/IRenderThread.h"

#include "ILog.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/IVulkanRenderer.h"
#include "IVulkan/VulkanConfig.h"

namespace INVENT
{
	IRenderThreadBase& IRenderThreadBase::Instance()
	{
		static IRenderThread r;
		return r;
	}

	bool IRenderThread::Start()
	{
		if (_running.exchange(true)) return false;

		if (!_init_vulkan())
		{
			_running = false;
			return false;
		}
		if (!IVulkanRenderer::Init())
		{
			INVENT_LOG_ERROR("[IRenderThread] Init Vulkan renderer error.");
			_running = false;
			return false;
		}
		_thread = std::thread(&IRenderThread::_render_loop, this);
		return true;
	}

	void IRenderThread::Shutdown()
	{
		_running.store(false);
		if (_thread.joinable())
			_thread.join();		// 渲染线程退出后才能销毁 Vulkan 资源
		IVulkanRenderer::Shutdown();
		INVENT_LOG_INFO("[IRenderThread] shutdown.");
		_shutdown_vulkan();
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

	void IRenderThread::_shutdown_vulkan()
	{
		IVulkanBase::Base().Terminate();
	}

	void IRenderThread::_render_loop()
	{
		std::uint32_t frame = 0;
		while (_running.load(std::memory_order_relaxed))
		{
			
			IVulkanRenderer::BeginRender(frame);
			IVulkanRenderer::RenderFrame(frame);
			IVulkanRenderer::EndRender();
			frame = (frame + 1) % IVulkan::MAX_FRAMES_IN_FLIGHT;
		}
	}

	void IRenderThreadBase::SetVulkanInstanceExtension(const char* extension_name)
	{
		IVulkanBase::AddInstanceExtension(extension_name);
	}

}
