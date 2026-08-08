#pragma once

#include "EngineAPI.h"

#include <vulkan/vulkan.h>

namespace INVENT
{
	class INVENT_API IRenderThreadBase
	{
	public:
		using CreateSurfaceFunc = VkResult(*)(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*);
		virtual ~IRenderThreadBase() = default;

		static IRenderThreadBase& Instance();

		virtual void SetCreateSurfaceFunction(CreateSurfaceFunc func) = 0;

		virtual bool Start() = 0;
		virtual void Shutdown() = 0;

		void SetVulkanInstanceExtension(const char* extension_name);
		void SetVulkanWaitForWindowEventsFunction(void(*func)());

	};
}
