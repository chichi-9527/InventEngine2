#include "IVulkan/IVulkanGlobalVertices.h"

#include "ILog.h"

namespace INVENT
{
	IVulkanGlobalVertices& IVulkanGlobalVertices::Instance()
	{
		IVulkanGlobalVertices v;
		return v;
	}

	bool IVulkanGlobalVertices::Init()
	{
		_default_buffer = new IVertexBuffer(128);


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
}
