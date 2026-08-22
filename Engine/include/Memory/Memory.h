#pragma once

#include "EngineAPI.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace INVENT
{
	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class EngineAllocator
	{
		struct _ptr_data
		{
			std::uint64_t ByteSize{ 0 };
			std::uint64_t Alignment{ 0 };
			bool		  FromPool{ false };
			bool IsValisd() const noexcept
			{
				return ByteSize != 0 && Alignment != 0;
			}
		};

		//using PtrDataMap = std::unordered_map<void*, _ptr_data>;
		using PtrDataMap = std::unordered_map<uint64_t,
			_ptr_data,
			std::hash<uint64_t>,
			std::equal_to<uint64_t>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const uint64_t, _ptr_data>>>;
	public:
		INVENT_API static bool Init();
		INVENT_API static void Clear();
		INVENT_API static bool IsReady();
		[[nodiscard]] INVENT_API static void* Allocate(std::uint64_t byte_size, std::uint64_t alignment = alignof(std::max_align_t));
		INVENT_API static void Deallocate(void* ptr);
	private:
		static void _deallocate_task(void* ptr);
	private:
		inline static PtrDataMap* _manager = nullptr;
	};
}

#ifdef __cplusplus
extern "C"
{
#endif // _cplusplus
	INVENT_API bool INVENT_DLL InventEngineAllocatorReady();
	INVENT_API void* INVENT_DLL InventEngineAllocate(std::uint64_t byte_size);
	INVENT_API void INVENT_DLL InventEngineDeallocate(void* ptr);
	
#ifdef __cplusplus
}
#endif // _cplusplus
