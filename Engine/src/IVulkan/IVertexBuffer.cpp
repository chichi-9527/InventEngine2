#include "IVulkan/IVertexBuffer.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "ILog.h"

namespace INVENT
{
	IVertexBuffer::IVertexBuffer(std::uint32_t vertex_count)
	{
		_vertex_count = vertex_count ? vertex_count : IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
		VkDeviceSize size = sizeof(IVertex) * _vertex_count;

		// 設置 Vulkan Buffer Usage Flags
		VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT        // 傳統渲染管線使用
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT       // 允許 Shader 以 Bindless SSBO 方式讀取（推薦）
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT // 開啟 BDA 支援
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT         // 碎片整理時作為 Copy 源
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT;        // 關卡加載或碎片整理時作為 Copy 目的

		// 設置 VMA Allocation Flags
		// 提示 VMA 該記憶體主要由 CPU 寫入，且是順序寫入（適合加載時 memcpy）
		VmaAllocationCreateFlags vma_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
			| VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(size,
			usage,
			vma_flags,
			_buffer,
			&_mapped_data))
		{
			INVENT_LOG_FATAL(std::format("[IVertexBuffer] Failed to allocate vertex buffer. VkResult: {}.", static_cast<int32_t>(result)));
			throw std::runtime_error("Failed to create IVertexBuffer");
		}

		_device_address = IVulkanBase::Base().GetBufferDeviceAddress(_buffer);
	}

	IVertexBuffer::~IVertexBuffer()
	{
		if (_buffer != VK_NULL_HANDLE)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(_buffer);
			_buffer = VK_NULL_HANDLE;
		}
		_mapped_data = nullptr;
		_device_address = 0;
	}
}
