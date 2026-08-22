#include "IEngineTools.h"

#include "ILog.h"
#include "IThread/IThreadPool.h"
#include "IMemPool/IMemPool.h"

#include <fstream>

namespace INVENT
{
	static auto RunPath = std::filesystem::current_path().string();
	static auto RunStdPath = std::filesystem::current_path();


	bool IEngineTools::ReadFile(const std::string& path, std::vector<char>& out)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			INVENT_LOG_ERROR(std::format("[IEngineTools] failed to open file : {}.", path));
			return false;
		}

		size_t fileSize = (size_t)file.tellg();
		out.resize(fileSize + 1);
		file.seekg(0);
		file.read(out.data(), fileSize);
		file.close();
		out[fileSize] = '\0';
		return true;
	}

	const std::string& IEngineTools::GetRunPath()
	{
		return RunPath;
	}

	const std::filesystem::path& IEngineTools::GetRunStdPath()
	{
		return RunStdPath;
	}

	IEngineTools& IEngineTools::Instance()
	{
		static IEngineTools t;
		return t;
	}

	void IEngineTools::Init()
	{
		_init_threadpools();
		_init_mem_pools();
	}
	void IEngineTools::Clear()
	{
		_clear_threadpools();
		_clear_mem_pools();
	}

	void IEngineTools::_init_threadpools()
	{
		_work_thread_pool = new IThreadPool(2, 1);
		_work_thread_pool->Start();
		_allocator_thread_pool = new IThreadPool();
		_allocator_thread_pool->Start();
	}

	void IEngineTools::_clear_threadpools()
	{
		if (_work_thread_pool)
		{
			_work_thread_pool->Shutdown();
			delete _work_thread_pool;
			_work_thread_pool = nullptr;
		}
		if (_allocator_thread_pool)
		{
			_allocator_thread_pool->Shutdown();
			delete _allocator_thread_pool;
			_allocator_thread_pool = nullptr;
		}
	}

	void IEngineTools::_init_mem_pools()
	{
		_gobal_memory_pool = IMemPool::CreatePool();
	}

	void IEngineTools::_clear_mem_pools()
	{
		IMemPool::DestroyPool(_gobal_memory_pool);
	}

}