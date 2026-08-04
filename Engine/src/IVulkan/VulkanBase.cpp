#include "IVulkan/VulkanBase.h"

namespace INVENT
{
	static std::vector<const char*> validationLayers;
	static std::vector<const char*> instanceExtensions;
	static std::vector<const char*> deviceExtensions;

	IVulkanBase::~IVulkanBase()
	{}

	void IVulkanBase::AddValidationLayer(const char* layerName)
	{}

	void IVulkanBase::AddInstanceExtension(const char* extensionName)
	{}

	void IVulkanBase::AddDeviceExtension(const char* extensionName)
	{}

	IVulkanBase& IVulkanBase::Base()
	{
		static IVulkanBase base;
		return base;
	}
}