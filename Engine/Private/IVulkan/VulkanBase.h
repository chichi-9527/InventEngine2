#pragma once

#include "VulkanConfig.h"
#include <vulkan/vulkan.hpp>

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

	private:

	};

	
}
