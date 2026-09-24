#include "IVulkan/IVertexBuffer.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "ILog.h"

namespace INVENT
{
	bool IVertexBuffer::InitUploadContext()
	{
		if (_s_ctx.IsValid()) return true;
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE) return false;

		VkCommandPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		pci.queueFamilyIndex = base.GetQueueFamilyIndices().GraphicsFamily;
		if (vkCreateCommandPool(base.GetDevice(), &pci, nullptr, &_s_ctx.CmdPool))
		{
			INVENT_LOG_ERROR("[IVertexBuffer] failed to create upload command pool!");
			return false;
		}
		VkFenceCreateInfo fci{};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(base.GetDevice(), &fci, nullptr, &_s_ctx.Fence))
		{
			INVENT_LOG_ERROR("[IVertexBuffer] failed to create upload fence!");
			vkDestroyCommandPool(base.GetDevice(), _s_ctx.CmdPool, nullptr);
			_s_ctx.CmdPool = VK_NULL_HANDLE;
			return false;
		}
		_s_ctx.Queue = base.GetGraphicsQueue();
		INVENT_LOG_INFO("[IVertexBuffer] upload context ready (graphics queue).");
		return true;
	}

	void IVertexBuffer::DestroyUploadContext()
	{
		if (!_s_ctx.IsValid()) return;
		auto device = IVulkanBase::Base().GetDevice();
		if (_s_ctx.Fence != VK_NULL_HANDLE) { vkDestroyFence(device, _s_ctx.Fence, nullptr); _s_ctx.Fence = VK_NULL_HANDLE; }
		if (_s_ctx.CmdPool != VK_NULL_HANDLE) { vkDestroyCommandPool(device, _s_ctx.CmdPool, nullptr); _s_ctx.CmdPool = VK_NULL_HANDLE; }
		_s_ctx.Queue = VK_NULL_HANDLE;
	}

	IVertexBuffer::IVertexBuffer(std::uint32_t vertex_count)
	{
		_vertex_count = vertex_count ? vertex_count : IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
		VkDeviceSize size = sizeof(IVertex) * _vertex_count;

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
		_device_address = 0;
	}

	IVertexBuffer::Vhandle IVertexBuffer::AddVertices(const IVertex* vertices, std::uint32_t count)
	{
		if (vertices == nullptr || count == 0)
			return INVALID_VHANDLE;
		if (GetCanAllocateCount() < count)
			return INVALID_VHANDLE;
		const std::uint32_t offset = _offset;
		const VkDeviceSize bytes = static_cast<VkDeviceSize>(count) * sizeof(IVertex);

		std::lock_guard<std::mutex> lock(_s_mutex);

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

		std::memcpy(mapped, vertices, static_cast<std::size_t>(bytes));

		VkCommandBuffer cmd = _begin_one_time_cmd();
		if (cmd == VK_NULL_HANDLE)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(staging);
			INVENT_LOG_ERROR("[IVertexBuffer] 命令緩衝分配失敗!");
			return INVALID_VHANDLE;
		}

		VkBufferCopy copy{};
		copy.srcOffset = 0;
		copy.dstOffset = static_cast<VkDeviceSize>(offset) * sizeof(IVertex);
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

	void IVertexBuffer::DestroyVertices(Vhandle handle)
	{
		std::lock_guard<std::mutex> lock(_s_mutex);

		auto iter = _used_handles.find(handle);
		if (iter == _used_handles.end()) return;

		const auto& vData = _datas[handle];
		_used_count -= vData.Count;
		_used_handles.erase(iter);
	}

	void IVertexBuffer::Reset()
	{
		std::lock_guard<std::mutex> lock(_s_mutex);

		_datas.clear();
		_used_handles.clear();
		_used_count = 0;
		_offset = 0;
	}

	bool IVertexBuffer::CheckDefragment() const
	{
		if (_vertex_count == 0) return false;
		return static_cast<double>(_used_count) / _vertex_count < 0.5 &&
			static_cast<double>(_offset) / _vertex_count > 0.7;
	}

	VkCommandBuffer IVertexBuffer::_begin_one_time_cmd()
	{
		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandPool = _s_ctx.CmdPool;
		ai.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(IVulkanBase::Base().GetDevice(), &ai, &cmd))
			return VK_NULL_HANDLE;
		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &bi);
		return cmd;
	}

	bool IVertexBuffer::_end_one_time_cmd(VkCommandBuffer cmd)
	{
		if (vkEndCommandBuffer(cmd))
		{
			vkFreeCommandBuffers(IVulkanBase::Base().GetDevice(), _s_ctx.CmdPool, 1, &cmd);
			return false;
		}
		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;
		auto device = IVulkanBase::Base().GetDevice();
		vkResetFences(device, 1, &_s_ctx.Fence);
		if (VkResult r = vkQueueSubmit(_s_ctx.Queue, 1, &si, _s_ctx.Fence))
		{
			INVENT_LOG_ERROR(std::format("[IVertexBuffer] 提交失敗! VkResult: {}.", static_cast<std::int32_t>(r)));
		}
		else if (VkResult r = vkWaitForFences(device, 1, &_s_ctx.Fence, VK_TRUE, UINT64_MAX))
		{
			INVENT_LOG_ERROR(std::format("[IVertexBuffer] fence 等待失敗! VkResult: {}.", static_cast<std::int32_t>(r)));
		}
		else
		{
			vkFreeCommandBuffers(device, _s_ctx.CmdPool, 1, &cmd);
			return true;		// 拷貝完成
		}
		vkFreeCommandBuffers(device, _s_ctx.CmdPool, 1, &cmd);
		return false;
	}

	void IVertexBuffer::_record_visibility_barrier(VkCommandBuffer cmd, VkBuffer buffer)
	{
		VkBufferMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		// 消費: BDA 頂點拉取 (vertex/compute shader) + 傳統管線綁定 (vertex input)
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
			| VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
			| VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = buffer;
		barrier.size = VK_WHOLE_SIZE;
		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.bufferMemoryBarrierCount = 1;
		di.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmd, &di);
	}

	IVertexBuffer::Vhandle IVertexBuffer::_commit_range(std::uint32_t count)
	{
		const std::uint32_t offset = _offset;
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

}
