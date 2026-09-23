#include "IVulkan/IVulkanGlobalVertices.h"

#include "ILog.h"
#include "IVulkan/VulkanConfig.h"

namespace INVENT
{
	IVulkanGlobalVertices& IVulkanGlobalVertices::Instance()
	{
		IVulkanGlobalVertices v;
		return v;
	}

	bool IVulkanGlobalVertices::Init()
	{
		_default_buffer = new IVertexBuffer(2u * 1024);


		INVENT_LOG_DEBUG("[IVulkanGlobalVertices] Init().");
		return true;
	}

	void IVulkanGlobalVertices::Destroy()
	{
		if (_default_buffer != nullptr)
		{
			delete _default_buffer;
			_default_buffer = nullptr;
		}
	}

	IVulkanGlobalVertices::VertexDataInfo IVulkanGlobalVertices::AddVertexData(const std::string& name, const IVertex* vertices, std::uint32_t count)
	{
		return VertexDataInfo();
	}
}
