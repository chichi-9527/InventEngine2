#include "Memory/Memory.h"

#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"
#include "IThread/IThreadPool.h"
#include "ILog.h"

namespace INVENT
{
	bool EngineAllocator::Init()
	{
		if (IEngineTools::Instance().GetMemPoolPool() == nullptr) 
		{ 
			INVENT_LOG_ERROR("[EngineAllocator] memory pool is not ready."); 
			return false; 
		}
		if (IsReady())
		{
			INVENT_LOG_WARNING("[EngineAllocator] is ready."); return false;
		}
		_manager = new PtrDataMap(64,
			std::hash<uint64_t>(),
			std::equal_to<uint64_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const uint64_t, _ptr_data>>(IEngineTools::Instance().GetMemPoolPool()));
		return true;
	}
	void EngineAllocator::Clear()
	{
		if (_manager)
		{
			for (auto& ptr : *_manager)
			{
				
			}
			delete _manager;
			_manager = nullptr;
		}
	}

	bool EngineAllocator::IsReady()
	{
		return IEngineTools::Instance().GetAllocatorThreadPool() != nullptr &&
			_manager != nullptr;
	}
	void* EngineAllocator::Allocate(std::uint64_t byte_size, std::uint64_t alignment)
	{
		if (byte_size == 0)
			return nullptr;
		if (alignment == 0)
			alignment = alignof(std::max_align_t);
		std::uint64_t need = byte_size > alignment ? byte_size : alignment;
		void* ptr = nullptr;
		bool fromPool = false;
		std::uint64_t recordSize = need;
		if (need <= FIXED_POOL_MAX_BLOCK_SIZE)
		{
			ptr = IEngineTools::Instance().GetMemPoolPool()->AllocateOnlyFixedWithAlignment(static_cast<size_t>(need));
			if (ptr != nullptr) fromPool = true;
		}
		if (ptr == nullptr)
		{
			// > 2048 / 对齐 > 2048 / 池不可用 / 池分配失败 —— 对齐 new（保留对齐）
			ptr = ::operator new(static_cast<std::size_t>(byte_size),
				std::align_val_t{ static_cast<std::size_t>(alignment) });
			recordSize = byte_size;
		}
		IEngineTools::Instance().GetAllocatorThreadPool()->Submit(0, [ptr, recordSize, alignment, fromPool]() {
			_manager->insert({ reinterpret_cast<uint64_t>(ptr), _ptr_data{ recordSize, alignment, fromPool } });
			});
		
		return nullptr;
	}

	void EngineAllocator::Deallocate(void* ptr) 
	{
		if (ptr == nullptr)
			return;
		IEngineTools::Instance().GetAllocatorThreadPool()->Submit(0, [ptr]() {
			_deallocate_task(ptr);
			});
	}

	void EngineAllocator::_deallocate_task(void* ptr)
	{
		auto iter = _manager->find(reinterpret_cast<uint64_t>(ptr));
		if (iter != _manager->end())
		{
			_ptr_data data = iter->second;
			_manager->erase(iter);
			if (data.FromPool)
			{
				IEngineTools::Instance().GetMemPoolPool()->DestroyOnlyFixed(ptr, static_cast<size_t>(data.ByteSize));
			}
			else
			{
				::operator delete(ptr, std::align_val_t{ static_cast<size_t>(data.Alignment) });
			}
		}
	}
}

bool INVENT_DLL InventEngineAllocatorReady()
{
	return INVENT::EngineAllocator::IsReady();
}

void* INVENT_DLL InventEngineAllocate(std::uint64_t byte_size)
{
	return INVENT::EngineAllocator::Allocate(byte_size);
}

void INVENT_DLL InventEngineDeallocate(void* ptr)
{
	INVENT::EngineAllocator::Deallocate(ptr);
}
