#pragma once

#include "EngineAPI.h"

#include <vector>
#include <queue>

#include <thread>
#include <mutex>
#include <future>
#include <atomic>

namespace INVENT
{
	class IThreadWork;

	class IThreadPool
	{
		friend class IThreadWork;
	public:
		IThreadPool(const IThreadPool&) = delete;
		IThreadPool(IThreadPool&&) = delete;
		IThreadPool& operator=(const IThreadPool&) = delete;
		IThreadPool& operator=(IThreadPool&&) = delete;

		// 当指定 priority_num 大于1时。将创建指定数量的 任务队列，任务优先级为 0 ~ priority_num - 1
		// 当 priority_num == 1 时，添加任务时 优先级失效
		INVENT_API IThreadPool(unsigned int thread_num = 1, unsigned int priority_num = 1);
		INVENT_API ~IThreadPool();

		INVENT_API void Start();

		// if submit some functions before start you can call this
		void NotifyOne() { _lock.notify_one(); }
		// if submit some functions before start you can call this
		void NotifyAll() { _lock.notify_all(); }

		// 线程数量只能在未 Start 时设置
		INVENT_API void SetThreadNum(unsigned int thread_num);
		unsigned int GetThreadNum() const { return _thread_num; }
		// 优先级数量只能在未 Start 时设置, 设置前指定的任务顺序不会改变
		INVENT_API void SetPriorityNum(unsigned int priority_num);
		unsigned int GetPriorityNum() const { return _priority_num; }
		// 线程数量和优先级数量只能在未 Start 时设置
		INVENT_API void SetThreadPriorityNum(unsigned int thread_num, unsigned int priority_num);

		bool IsRunning() const { return _is_start; }
		INVENT_API void Shutdown();

		// 任务优先级为 0 ~priority_num - 1
		// 当 priority_num == 1 时，添加任务时 优先级失效
		template<typename Func, typename... Args>
		auto Submit(unsigned int priority, Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
		{
			priority = priority > _priority_num - 1 ? _priority_num - 1 : priority;
			std::function<decltype(func(args...))()> function = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
			auto task_ptr = std::make_shared<std::packaged_task<decltype(func(args...))()>>(function);
			auto task_future = task_ptr->get_future();
			std::function<void()> warpper_func = [task_ptr]()
				{
					(*task_ptr)();
				};
			_func_queues[priority]->Emplace(warpper_func);
			_lock.notify_one();
			return task_future;
		}

	protected:

		const std::atomic_bool& IsShutdown() const { return _is_shutdown; }

		template<typename T>
		class IFunctionQueue
		{
		public:
			IFunctionQueue() = default;
			~IFunctionQueue() = default;

			bool Empty() { std::lock_guard<std::mutex> lock(_mutex); return _queue.empty(); }
			int Size() { std::lock_guard<std::mutex> lock(_mutex); return _queue.size(); }
			void Emplace(const T& t) { std::lock_guard<std::mutex> lock(_mutex); _queue.emplace(t); }

			void Emplace(IFunctionQueue<T>* func_queue)
			{
				while (!func_queue->_queue.empty())
				{
					this->_queue.push(func_queue->_queue.front());
					func_queue->_queue.pop();
				}
			}

			bool Front(T& t)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				if (_queue.empty()) { return false; }
				t = std::move(_queue.front());
				_queue.pop();
				return true;
			}

		private:
			std::queue<T> _queue;
			std::mutex _mutex;
		};

	private:
		std::vector<std::thread*> _threads;
		std::vector<IFunctionQueue<std::function<void()>>*> _func_queues;

		std::mutex _mutex;
		std::condition_variable _lock;

		std::atomic_bool _is_shutdown;

		unsigned int _thread_num;
		unsigned int _priority_num;

		bool _is_start;
	};

	class INVENT_API IThreadWork
	{
	public:
		IThreadWork(IThreadPool* pool);
		IThreadWork(const IThreadWork&) = delete;

		void Start()
		{
			std::function<void()> func;
			bool take_func_suc = false;

			while (!_pool->IsShutdown())
			{
				{
					std::unique_lock<std::mutex> lock(_pool->_mutex);
					auto funcs = _get_first_have_task_queue();
					if (funcs == nullptr)
					{
						_pool->_lock.wait(lock);
						continue;
					}
					take_func_suc = funcs->Front(func);
				}
				if (take_func_suc)
				{
					func();
					take_func_suc = false;
				}

			}
		}

		~IThreadWork();

	private:
		IThreadPool::IFunctionQueue<std::function<void()>>* _get_first_have_task_queue()
		{
			for (auto func_queue : _pool->_func_queues)
				if (!func_queue->Empty()) return func_queue;
			return nullptr;
		}

	private:
		IThreadPool* _pool;
	};
}
