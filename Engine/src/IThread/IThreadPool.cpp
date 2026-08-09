#include "IThread/IThreadPool.h"

namespace INVENT
{
	IThreadWork::IThreadWork(IThreadPool* pool)
		: _pool(pool)
	{}

	IThreadWork::~IThreadWork()
	{}

	////////////////////////////////////////////////////////////////////////////////////////////
	////////////// thread pool
	////////////////////////////////////////////////////////////////////////////////////////////

	IThreadPool::IThreadPool(unsigned int thread_num, unsigned int priority_num)
		: _thread_num(thread_num ? thread_num : 1)
		, _priority_num(priority_num ? priority_num : 1)
		, _is_shutdown(false)
		, _is_start(false)
	{
		for (unsigned int i = 0; i < priority_num; ++i)
		{
			_func_queues.push_back(new IFunctionQueue<std::function<void()>>);
		}
	}

	IThreadPool::~IThreadPool()
	{
		Shutdown();

		for (auto func_queue : _func_queues)
			if (func_queue) delete func_queue;
	}

	void IThreadPool::Start()
	{
		if (_is_start) return;
		_is_start = true;
		_is_shutdown = false;
		for (unsigned int i = 0; i < _thread_num; ++i)
		{
			_threads.push_back(new std::thread([this]() {IThreadWork work(this); work.Start(); }));
		}
	}

	void IThreadPool::SetThreadNum(unsigned int thread_num)
	{
		if (thread_num == 0 || _is_start) return;

		_thread_num = thread_num;
	}

	void IThreadPool::SetPriorityNum(unsigned int priority_num)
	{
		if (priority_num == 0 || _is_start) return;
		if (_priority_num < priority_num)
		{
			for (unsigned int i = 0; i < priority_num - _priority_num; ++i)
			{
				_func_queues.push_back(new IFunctionQueue<std::function<void()>>);
			}
		}
		else if (_priority_num > priority_num)
		{
			for (unsigned int i = priority_num; i < _priority_num; ++i)
			{
				_func_queues[size_t{ priority_num } - 1]->Emplace(_func_queues[i]);
			}
			for (unsigned int i = 0; i < _priority_num - priority_num; ++i)
			{
				_func_queues.pop_back();
			}
		}
		_priority_num = priority_num;
	}

	void IThreadPool::SetThreadPriorityNum(unsigned int thread_num, unsigned int priority_num)
	{
		SetThreadNum(thread_num);
		SetPriorityNum(priority_num);
	}

	void IThreadPool::Shutdown()
	{
		if (_is_start)
		{
			_is_shutdown = true;
			_lock.notify_all();

			for (auto thread : _threads)
				if (thread->joinable()) thread->join();

			for (auto thread : _threads)
				delete thread;

			_is_start = false;
		}

	}


}
