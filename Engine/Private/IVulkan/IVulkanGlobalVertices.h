#pragma once

#include "VulkanConfig.h"
#include "IVulkan/IVertexBuffer.h"

namespace INVENT
{
	class IVulkanGlobalVertices
	{
		IVulkanGlobalVertices() = default;
	public:
		static IVulkanGlobalVertices& Instance();

		bool Init();
		void Destroy();


	private:
		IVertexBuffer* _default_buffer = nullptr;
	};
}
