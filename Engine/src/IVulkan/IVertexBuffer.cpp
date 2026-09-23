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

		VkMemoryPropertyFlags mem_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(size,
			usage,
			vma_flags,
			mem_flags,
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

	IVertexBuffer::Vhandle IVertexBuffer::AddVertices(const IVertex* vertices, std::uint32_t count)
	{
		if (vertices == nullptr || count == 0)
			return INVALID_VHANDLE;
		if (GetCanAllocateCount() < count)
			return INVALID_VHANDLE;
		const std::uint32_t offset = _offset;

		std::memcpy(static_cast<std::byte*>(_mapped_data) + static_cast<std::size_t>(offset) * sizeof(IVertex),
			vertices,
			static_cast<std::size_t>(count) * sizeof(IVertex));

		const Vhandle h = static_cast<Vhandle>(_datas.size());
		auto& vData = _datas.emplace_back();
		vData.Offset = offset;
		vData.BaseAddress = _device_address + static_cast<VkDeviceSize>(offset) * sizeof(IVertex);
		vData.Count = count;
		_offset = offset + count;
		_used_count += count;
		_used_handles.insert(h);
		return h;
	}

	void IVertexBuffer::DestroyVertices(Vhandle handle)
	{
		auto iter = _used_handles.find(handle);
		if (iter == _used_handles.end()) return;

		const auto& vData = _datas[handle];
		_used_count -= vData.Count;
		_used_handles.erase(iter);
	}

	void IVertexBuffer::Reset()
	{
		_datas.clear();
		_used_handles.clear();
		_used_count = 0;
		_offset = 0;
	}

	bool IVertexBuffer::CheckDefragment() const
	{
		if (static_cast<double>(_used_count) / _vertex_count < 0.5 &&
			static_cast<double>(_offset) / _vertex_count > 0.7)
		{
			return true;
		}
		return false;
	}

}
