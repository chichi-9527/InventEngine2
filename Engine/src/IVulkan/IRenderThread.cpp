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

		if (!IVulkanBase::Base().CreateVulkanInstance())
		{
			return false;
		}

		return true;
	}

	void IRenderThread::Shutdown()
	{
		INVENT_LOG_INFO("[IRenderThread] shutdown.");
	}
}
