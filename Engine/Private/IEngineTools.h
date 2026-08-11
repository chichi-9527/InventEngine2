#pragma once

#include "IMemPool/IMemPool.h"

#include <string>
#include <vector>
#include <filesystem>

namespace INVENT
{
	class IThreadPool;
	class IMemPool;

	class IEngineTools
	{
		IEngineTools() = default;
	public:
		~IEngineTools() = default;

		static bool ReadFile(const std::string& path, std::vector<char>& out);
		static const std::string& GetRunPath();
		static const std::filesystem::path& GetRunStdPath();

		static IEngineTools& Instance();

		void Init();
		void Clear();

		IThreadPool* GetWorkThreadPool() const { return _work_thread_pool; }
		IMemPool* GetMemPoolPool() const { return _gobal_memory_pool; }

	private:
		void _init_threadpools();
		void _clear_threadpools();
		void _init_mem_pools();
		void _clear_mem_pools();

	private:
		IThreadPool* _work_thread_pool = nullptr;
		IMemPool* _gobal_memory_pool = nullptr;
	};
}