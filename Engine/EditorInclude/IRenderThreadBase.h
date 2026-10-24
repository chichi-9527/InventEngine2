#pragma once

#include "EngineAPI.h"
#include "ImguiConfig.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace INVENT
{
	class INVENT_API IRenderThreadBase
	{
	public:
		using CreateSurfaceFunc = VkResult(*)(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*);
		struct ImguiVkInitInfo 
		{
			VkInstance instance = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
			VkQueue graphicsQueue = VK_NULL_HANDLE;
			std::uint32_t queueFamilyIndex = 0; // 官方後端需要佇列家族索引

			VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE; // imgui 专用的 pool
			std::uint32_t minImageCount = 2;                       // 通常對應 Swapchain 最少影像數
			std::uint32_t imageCount = 3;                          // 通常對應 Swapchain 當前影像數

			VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED;  // 你的 Swapchain 影像格式 (例如 VK_FORMAT_B8G8R8A8_UNORM)
			VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;  // 如果 UI 需要獨立 Depth，可填；否則填 VK_FORMAT_UNDEFINED
			VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;// 通常填 VK_FORMAT_UNDEFINED

		};
		using ImguiVkInitFunc = bool(*)(const ImguiVkInitInfo&);
		using ImguiVkShutdownFunc = void(*)();

		virtual ~IRenderThreadBase() = default;

		static IRenderThreadBase& Instance();

		virtual void SetCreateSurfaceFunction(CreateSurfaceFunc func) = 0;
		virtual void SetImguiVkInitFunction(ImguiVkInitFunc func) = 0;
		virtual void SetImguiVkShutdownFunction(ImguiVkShutdownFunc func) = 0;
		virtual void SetImguiNewFrameFunc(IImgui::ImguiNewFrameFunc func) = 0;
		virtual void SetImguiEndFrameAndFetchDataFunc(IImgui::ImguiEndFrameAndFetchDataFunc func) = 0;
		virtual void SetImguiRenderDrawDataFunc(IImgui::ImguiRenderDrawDataFunc func) = 0;

		virtual bool Init() = 0;
		virtual bool Start() = 0;
		virtual void Shutdown() = 0;

		void SetVulkanInstanceExtension(const char* extension_name);

		std::uint32_t MaxFrameInFlight() const noexcept;
		bool IsImguiInitSuccss() const { return _is_imgui_init_success; }
	protected:
		bool _is_imgui_init_success{ false };

	};
}
