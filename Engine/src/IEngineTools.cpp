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

	bool IEngineTools::WriteFile(const std::string& path, const std::string& filename, const std::vector<char>& data)
	{
		return WriteFile(path, filename, data.data(), data.size());
	}

	bool IEngineTools::WriteFile(const std::string& path, const std::string& filename, const char* data, std::size_t size)
	{
		auto _current_path = std::filesystem::path{ path } / (filename);
		if (auto path = _current_path.parent_path(); !path.empty())
		{
			std::filesystem::create_directories(path);
		}
		std::ofstream out(_current_path, std::ios::out | std::ios::trunc | std::ios::binary);
		if (!out.is_open())
		{
			return false;
		}
		out.write(data, size);
		out.flush();
		return true;
	}

	bool IEngineTools::WriteFileAppend(const std::string& path, const std::string& filename, const std::vector<char>& data)
	{
		return WriteFileAppend(path, filename, data.data(), data.size());
	}

	bool IEngineTools::WriteFileAppend(const std::string& path, const std::string& filename, const char* data, std::size_t size)
	{
		auto _current_path = std::filesystem::path{ path } / (filename);
		if (auto path = _current_path.parent_path(); !path.empty())
		{
			std::filesystem::create_directories(path);
		}
		std::ofstream out(_current_path, std::ios::out | std::ios::app | std::ios::binary);
		if (!out.is_open())
		{
			return false;
		}
		out.write(data, size);
		out.flush();
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