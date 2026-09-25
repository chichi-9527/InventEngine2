#include "IBuffer.h"

#include "VulkanConfig.h"
#include "VulkanBase.h"
#include "ILog.h"

namespace INVENT
{
	
	template<typename T>
	inline IBuffer<T>::IBuffer(std::uint32_t vertex_count)
	{
		_vertex_count = vertex_count ? vertex_count : IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
		VkDeviceSize size = sizeof(pointType) * _vertex_count;

		// 設置 Vulkan Buffer Usage Flags
		VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT        // 傳統渲染管線使用
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT       // 允許 Shader 以 Bindless SSBO 方式讀取
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT // 開啟 BDA 支援
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT         // 碎片整理時作為 Copy 源
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT;        // 關卡加載或碎片整理時作為 Copy 目的

		VmaAllocationCreateFlags vma_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

#if 0
		auto allocator = IVulkanBase::Base().GetVmaAllocator();

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo vmaAllocCreateInfo{};
		vmaAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		vmaAllocCreateInfo.flags = vma_flags;
		std::uint32_t typeIndex{ 0 };
		vmaFindMemoryTypeIndexForBufferInfo(allocator, &bufferInfo, &vmaAllocCreateInfo, &typeIndex);

		const VkPhysicalDeviceMemoryProperties* memProps;
		vmaGetMemoryProperties(allocator, &memProps);
		uint32_t heapIndex = memProps->memoryTypes[typeIndex].heapIndex;
		INVENT_LOG_DEBUG(std::format("[IVertexBuffer] Find memory heap index for vertex buffer: {}.", heapIndex));
#endif

		if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(size,
			usage,
			vma_flags,
			_buffer))
		{
			INVENT_LOG_FATAL(std::format("[IVertexBuffer] Failed to allocate buffer. VkResult: {}.", static_cast<int32_t>(result)));
			throw std::runtime_error("Failed to create IBuffer");
		}

		_device_address = IVulkanBase::Base().GetBufferDeviceAddress(_buffer);
	}

	template<typename T>
	inline IBuffer<T>::~IBuffer()
	{
		if (_buffer != VK_NULL_HANDLE)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(_buffer);
			_buffer = VK_NULL_HANDLE;
		}
		_device_address = 0;
	}

	template<typename T>
	inline IBaseBuffer::Bhandle IBuffer<T>::AddDatas(const pointType* points, std::uint32_t count)
	{
		if (points == nullptr || count == 0)
			return INVALID_VHANDLE;

		std::lock_guard<std::mutex> lock(_s_mutex);

		if (GetCanAllocateCount() < count)
			return INVALID_VHANDLE;
		const std::uint32_t offset = _offset;
		const VkDeviceSize bytes = static_cast<VkDeviceSize>(count) * sizeof(pointType);

		VkBuffer staging = VK_NULL_HANDLE;
		void* mapped = nullptr;
		if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(bytes,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging,
			&mapped))
		{
			INVENT_LOG_ERROR(std::format("[IVertexBuffer] Failed to create staging. VkResult: {}.",
				static_cast<std::int32_t>(result)));
			return INVALID_VHANDLE;
		}

		std::memcpy(mapped, points, static_cast<std::size_t>(bytes));

		VkCommandBuffer cmd = _begin_one_time_cmd();
		if (cmd == VK_NULL_HANDLE)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(staging);
			INVENT_LOG_ERROR("[IVertexBuffer] 命令緩衝分配失敗!");
			return INVALID_VHANDLE;
		}

		VkBufferCopy copy{};
		copy.srcOffset = 0;
		copy.dstOffset = static_cast<VkDeviceSize>(offset) * sizeof(pointType);
		copy.size = bytes;
		vkCmdCopyBuffer(cmd, staging, _buffer, 1, &copy);
		// 末尾屏障: 拷貝寫入 -> 後續頂點讀取. 同隊列提交序使該依賴
		// 覆蓋之後提交的所有幀 —— 渲染循環無需任何接入
		_record_visibility_barrier(cmd, _buffer);
		if (!_end_one_time_cmd(cmd))
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(staging);
			return INVALID_VHANDLE;		// 上傳失敗: 不落賬, 空間未推進
		}

		IVulkanBase::Base().UseVmaDestroyBuffer(staging);

		return _commit_range(count);
	}

	template<typename T>
	inline void IBuffer<T>::DestroyDatas(IBaseBuffer::Bhandle handle)
	{
		std::lock_guard<std::mutex> lock(_s_mutex);

		auto iter = _used_handles.find(handle);
		if (iter == _used_handles.end()) return;

		const auto& vData = _datas[handle];
		_used_count -= vData.Count;
		_used_handles.erase(iter);
	}

	template<typename T>
	inline void IBuffer<T>::Reset()
	{
		std::lock_guard<std::mutex> lock(_s_mutex);

		_datas.clear();
		_used_handles.clear();
		_used_count = 0;
		_offset = 0;
	}

	template<typename T>
	inline bool IBuffer<T>::CheckDefragment() const
	{
		if (_vertex_count == 0) return false;
		return static_cast<double>(_used_count) / _vertex_count < 0.5 &&
			static_cast<double>(_offset) / _vertex_count > 0.7;
	}

	template<typename T>
	inline IBaseBuffer::Bhandle IBuffer<T>::_add_datas_range(std::uint32_t count)
	{
		std::lock_guard<std::mutex> lock(_s_mutex);

		if (count == 0 || GetCanAllocateCount() < count)
			return INVALID_VHANDLE;
		return _commit_range(count);
	}

	template<typename T>
	inline IBaseBuffer::Bhandle IBuffer<T>::_commit_range(std::uint32_t count)
	{
		const std::uint32_t offset = _offset;
		const Bhandle h = static_cast<Bhandle>(_datas.size());
		auto& vData = _datas.emplace_back();
		vData.Offset = offset;
		vData.BaseAddress = _device_address + static_cast<VkDeviceSize>(offset) * sizeof(pointType);
		vData.Count = count;
		_offset = offset + count;
		_used_count += count;
		_used_handles.insert(h);
		return h;
	}

}
