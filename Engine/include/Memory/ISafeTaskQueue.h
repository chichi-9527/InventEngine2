#pragma once

#include <array>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <future>
#include <type_traits>
#include <memory>

namespace INVENT
{
    class alignas(64) ISafeTaskQueue {
    private:
        // 使用限制對齊，防止偽共享 (False Sharing)
        struct alignas(64) PaddedVector {
            std::vector<std::function<void()>> tasks;
        };

        std::array<PaddedVector, 2> _call_backs;
        std::atomic<uint32_t> _call_backs_frame{ 0 };
        std::mutex _write_mutex;

    public:
        explicit ISafeTaskQueue(size_t initial_capacity = 64)
        {
            _call_backs[0].tasks.reserve(initial_capacity);
            _call_backs[1].tasks.reserve(initial_capacity);
        }

        ~ISafeTaskQueue() = default;

        // 禁用拷貝與移動
        ISafeTaskQueue(const ISafeTaskQueue&) = delete;
        ISafeTaskQueue& operator=(const ISafeTaskQueue&) = delete;

        /**
         * 提交任務，支持返回值與 std::future
         */
        template<typename Func, typename... Args>
        auto Submit(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
        {
            using ReturnType = std::invoke_result_t<Func, Args...>;

            // 1. 使用 C++20 std::bind_front 替代 std::bind（性能更好，完美轉發參數）
            // 如果編譯器不支持，也可以用拉姆達表達式：[f = std::forward<Func>(func), ...args = std::forward<Args>(args)]() mutable { return std::invoke(f, args...); }
            auto bound_task = std::bind_front(std::forward<Func>(func), std::forward<Args>(args)...);

            // 2. 創建 packaged_task
            auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound_task));
            auto task_future = task_ptr->get_future();

            // 3. 包裝成 std::function。
            // 核心技巧：雖然 packaged_task 不能拷貝，但 shared_ptr 可以安全地被 std::function 拷貝捕獲
            std::function<void()> wrapper_func = [task_ptr]() {
                if (task_ptr)
                {
                    (*task_ptr)();
                }
                };

            // 4. 投遞到當前寫入緩衝區（內部加輕量鎖）
            {
                uint32_t write_index = _call_backs_frame.load(std::memory_order_acquire) % 2;
                std::lock_guard<std::mutex> lock(_write_mutex);
                _call_backs[write_index].tasks.push_back(std::move(wrapper_func));
            }

            return task_future;
        }

        /**
         * 專用線程每幀調用一次：切換緩衝區並無鎖執行所有任務
         */
        void FlushAndExecute()
        {
            uint32_t read_index = _call_backs_frame.load(std::memory_order_relaxed) % 2;
            uint32_t next_write_index = (read_index + 1) % 2;

            // 1. 切換雙緩衝指針（在鎖內切換，保證生產者和消費者完全安全分離）
            {
                std::lock_guard<std::mutex> lock(_write_mutex);
                _call_backs_frame.store(next_write_index, std::memory_order_release);
            }

            // 2. 此時生產者已經安全地寫入下一個 vector 了
            // 當前線程可以【完全無鎖】安全地消費舊 vector 裡的所有任務
            auto& current_tasks = _call_backs[read_index].tasks;

            for (const auto& task : current_tasks)
            {
                if (task)
                {
                    task();
                }
            }

            // 3. 清空任務，保持 capacity 空間，避免下次寫入時重複分配內存
            current_tasks.clear();
        }
    };
}
