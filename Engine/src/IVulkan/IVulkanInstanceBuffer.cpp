#include "IVulkan/IVulkanInstanceBuffer.h"

#include "IMemPool/IMemPool.h"
#include "IEngineTools.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/VulkanConfig.h"
#include "ILog.h"

#include <cmath>
#include <numeric>
#include <new>

namespace INVENT
{
	static IMemPoolAllocatorOnlyFixedBlock<IVulkan::Batch> Allocator;

	bool InstanceBuffer::Init(std::uint32_t default_batch_num)
	{
		if (_is_valid) return false;

		Allocator = IMemPoolAllocatorOnlyFixedBlock<IVulkan::Batch>{ IEngineTools::Instance().GetMemPoolPool() };
		// calculate buffer size
		std::uint32_t maxInstanceNum = static_cast<uint32_t>(uint64_t{ IVulkan::MaxSSBORange } / sizeof(InstanceData));
		_current_instance_num = std::min(IVulkan::EXPECTED_INSTANCE_SIZE, maxInstanceNum);
		INVENT_LOG_INFO(std::format("[InstanceBuffer] Max instance num: {}.", _current_instance_num));

		if (default_batch_num > _current_instance_num)
		{
			INVENT_LOG_ERROR("[InstanceBuffer] default batches too big."); 
			return false;
		}

		// init vkbuffer
		if (VkResult res = IVulkanBase::Base().UseVmaCreateBuffer(static_cast<VkDeviceSize>(_current_instance_num) * sizeof(InstanceData),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			_ssbo,
			&_mapped_data))
		{
			throw std::runtime_error("failed to create instance buffer!");
		}
		
		// default
		auto defPtr = Allocator.allocate(1);
		if (defPtr == nullptr)
		{
			INVENT_LOG_ERROR("[InstanceBuffer] Allocator allocate error!");
			return false;
		}
		auto defaultBatch = ::new (defPtr) IVulkan::Batch();
		defaultBatch->Offset = 0u;
		defaultBatch->Count = default_batch_num;
		defaultBatch->_valids.ResizeBitCount(default_batch_num);
		_batches.push_back(defaultBatch);

		_offset = default_batch_num;
		_no_team_instance_num = _current_instance_num - default_batch_num;

		_is_valid = true;
		return true;
	}

	void InstanceBuffer::Destroy()
	{
		for (auto batch : _batches)
		{
			if (batch)
			{
				Allocator.deallocate(batch, 1);
			}
		}
		_batches.clear();
		if (_ssbo != VK_NULL_HANDLE)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(_ssbo);
			_ssbo = VK_NULL_HANDLE;
		}
		_mapped_data = nullptr;
		_current_instance_num = 0;
		_is_valid = false;
	}

	InstanceBuffer::BatchHandle InstanceBuffer::AddBatch(std::uint32_t instance_num)
	{
		if (_batches.size() + 1 >= static_cast<size_t>(IVulkan::MAX_INSTANCE_BATCH_COUNT))
		{
			INVENT_LOG_ERROR(std::format("[InstanceBuffer] batch count(includes default) greater than MAX_INSTANCE_BATCH_COUNT: {}.", IVulkan::MAX_INSTANCE_BATCH_COUNT));
			INVENT_LOG_WARNING("[InstanceBuffer] if you want to increase IVulkan::MAX_INSTANCE_BATCH_COUNT,"
				"you must to change shader file drawCount count and IVulkanRanderer::_draw_count_and_indirect_buffer size.");
			return IIBHANDLE_INVALID;
		}
		if (instance_num > _no_team_instance_num)
		{
			INVENT_LOG_ERROR(std::format("[InstanceBuffer] New batch needed instance num > no team instance num. need cout: {}. no team count: {}.",
				instance_num, _no_team_instance_num));
			return IIBHANDLE_INVALID;
		}
		
		auto ptr = Allocator.allocate(1);
		if (ptr == nullptr)
		{
			INVENT_LOG_ERROR("[InstanceBuffer] Allocator allocate error!");
			return false;
		}
		IVulkan::Batch* batch = ::new (ptr) IVulkan::Batch();
		batch->Count = instance_num;
		batch->_valids.ResizeBitCount(instance_num);
		batch->Offset = _offset;
		_batches.push_back(batch);

		_offset += instance_num;
		_no_team_instance_num -= instance_num;

		return static_cast<BatchHandle>(_batches.size());
	}

	uint32_t InstanceBuffer::GetOffset(size_t batch_index)
	{
		if (batch_index >= _batches.size()) return UINT32_MAX;
		auto batch = _batches[batch_index];
		std::shared_lock<std::shared_mutex> lock(batch->_rw_mutex);

		return batch->Offset;
	}

	uint32_t InstanceBuffer::GetUsedHighWaterMark(size_t batch_index)
	{
		if (batch_index >= _batches.size()) return UINT32_MAX;
		auto batch = _batches[batch_index];
		std::shared_lock<std::shared_mutex> lock(batch->_rw_mutex);

		return batch->UsedHighWaterMark;
	}

	InstanceBuffer::Handle InstanceBuffer::AddInstanceData(const InstanceData& data, BatchHandle batch_index)
	{
		if (batch_index >= _batches.size()) return Handle{};
		auto batch = _batches[batch_index];

		std::unique_lock<std::shared_mutex> lock(batch->_rw_mutex);
		IHandle ihandle = batch->_valids.FindFirstZero();
		if (!ihandle.IsValid())
		{
			INVENT_LOG_ERROR(std::format("[InstanceBuffer] NO.{} batch allocate instance error, because batch memory is full!", batch_index));
			return Handle{};
		}
		batch->_valids.SetValue<true>(ihandle);
		auto localIndex = static_cast<uint32_t>(ihandle.GetRealIndex());
		if (localIndex >= batch->UsedHighWaterMark)
		{
			batch->UsedHighWaterMark = localIndex + 1;
		}
		// write
		auto gIndex = batch->Offset + localIndex;
		if (_mapped_data)
		{
			auto dstPtr = reinterpret_cast<InstanceData*>(_mapped_data) + gIndex;
			memcpy(dstPtr, &data, sizeof(InstanceData));
		}

		Handle handle = { batch_index, ihandle };
		return handle;
	}

	void InstanceBuffer::UpdateInstanceData(const InstanceData& data, Handle handle)
	{
		if (!handle.IsValid()) return;
		if (handle.batchIndex >= _batches.size())return;
		auto batch = _batches[handle.batchIndex];

		std::shared_lock<std::shared_mutex> lock(batch->_rw_mutex);
		if(!batch->_valids.Get(handle.ihandle)) return;
		uint32_t gIndex = batch->Offset + handle.GetInstanceIndex();
		if (_mapped_data)
		{
			auto dstPtr = reinterpret_cast<InstanceData*>(_mapped_data) + gIndex;
			memcpy(dstPtr, &data, sizeof(InstanceData));
		}
	}

	void InstanceBuffer::FreeInstanceData(Handle handle)
	{
		if (!handle.IsValid()) return;
		if (handle.batchIndex >= _batches.size())return;
		auto batch = _batches[handle.batchIndex];

		std::unique_lock<std::shared_mutex> lock(batch->_rw_mutex);
		if (!batch->_valids.Get(handle.ihandle)) return;
		uint32_t gIndex = batch->Offset + handle.GetInstanceIndex();
		if (_mapped_data)
		{
			reinterpret_cast<InstanceData*>(_mapped_data)[gIndex].ObjectID = 0;
		}
		batch->_valids.SetValue<false>(handle.ihandle);
	}

	void InstanceBuffer::FlushBuffer()
	{
		if (!IVulkanBase::Base().UseVmaFlushAllocationBuffer(_ssbo))
		{
			throw std::runtime_error("failed to flush instance buffer!");
		}
	}
}
