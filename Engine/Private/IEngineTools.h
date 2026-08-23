#pragma once


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
		static bool WriteFile(const std::string& path, const std::string& filename, const std::vector<char>& data);
		static bool WriteFile(const std::string& path, const std::string& filename, const char* data, std::size_t size);
		static bool WriteFileAppend(const std::string& path, const std::string& filename, const std::vector<char>& data);
		static bool WriteFileAppend(const std::string& path, const std::string& filename, const char* data, std::size_t size);
		
		static const std::string& GetRunPath();
		static const std::filesystem::path& GetRunStdPath();

		static IEngineTools& Instance();

		void Init();
		void Clear();

		IThreadPool* GetWorkThreadPool() const { return _work_thread_pool; }
		IThreadPool* GetAllocatorThreadPool() const { return _allocator_thread_pool; }
		IMemPool* GetMemPoolPool() const { return _gobal_memory_pool; }

	private:
		void _init_threadpools();
		void _clear_threadpools();
		void _init_mem_pools();
		void _clear_mem_pools();

	private:
		IThreadPool* _work_thread_pool = nullptr;
		IThreadPool* _allocator_thread_pool = nullptr;
		IMemPool* _gobal_memory_pool = nullptr;
	};
}