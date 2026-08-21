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

	bool InstanceBuffer::Init(const std::vector<uint32_t>& other_batches)
	{
		InstanceBuffer::Destroy();
		Allocator = IMemPoolAllocatorOnlyFixedBlock<IVulkan::Batch>{ IEngineTools::Instance().GetMemPoolPool() };
		// calculate buffer size
		std::uint32_t maxInstanceNum = static_cast<uint32_t>(uint64_t{ IVulkan::MaxSSBORange } / sizeof(InstanceData));
		_current_instance_num = std::min(IVulkan::EXPECTED_INSTANCE_SIZE, maxInstanceNum);
		INVENT_LOG_INFO(std::format("[InstanceBuffer] Max instance num: {}.", _current_instance_num));
		auto allOthers = std::accumulate(other_batches.begin(), other_batches.end(), uint32_t{ 0 });
		if (allOthers > _current_instance_num)
		{
			INVENT_LOG_ERROR("[InstanceBuffer] other batches too big."); 
			return false;
		}
		if (_current_instance_num - allOthers < 10 * 1024)
		{
			INVENT_LOG_WARNING(std::format("[InstanceBuffer] 默认批次（普通不透明批次）的实例数量只有 {} 个.", _current_instance_num - allOthers));
		}
		// init vkbuffer
		if (!IVulkanBase::Base().UseVmaCreateBuffer(static_cast<VkDeviceSize>(_current_instance_num) * sizeof(InstanceData),
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
		_batches.push_back(defaultBatch);
		uint32_t offset{ 0 };
		for (auto num : other_batches)
		{
			auto ptr = Allocator.allocate(1);
			if (ptr == nullptr)
			{
				INVENT_LOG_ERROR("[InstanceBuffer] Allocator allocate error!");
				return false;
			}
			IVulkan::Batch* batch = ::new (ptr) IVulkan::Batch();
			batch->Count = num;
			batch->_valids.ResizeBitCount(num);
			batch->Offset = offset;
			_batches.push_back(batch);

			offset += num;
		}
		defaultBatch->Offset = offset;
		defaultBatch->Count = _current_instance_num - allOthers;
		defaultBatch->_valids.ResizeBitCount(_current_instance_num - allOthers);

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

	InstanceBuffer::Handle InstanceBuffer::AddInstanceData(const InstanceData& data, uint32_t batch_index)
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
