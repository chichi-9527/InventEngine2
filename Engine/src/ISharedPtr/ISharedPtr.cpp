#include "ISharedPtr/ISharedPtr.h"

#include "Memory/Memory.h"

#include <utility>

namespace INVENT
{
    namespace ITools
    {

        IPtrManager::~IPtrManager()
        {
            Terminate();
        }

        IPtrManager& IPtrManager::GetInstance()
        {
            static IPtrManager instance;
            return instance;
        }

        bool IPtrManager::Init()
        {
            std::lock_guard<std::mutex> lock(Mutex);
            terminated = false;
            initialized = true;
            return true;
        }

        void IPtrManager::Terminate()
        {
            std::vector<ControlBlock*> blocks;

            {
                std::lock_guard<std::mutex> lock(Mutex);

                if (terminated)
                    return;

                terminated = true;
                initialized = false;

                blocks.swap(Blocks);

                std::queue<ControlBlock*> empty;
                FreeBlocks.swap(empty);
            }

            for (ControlBlock* cb : blocks)
            {
                // 仍存活的对象：执行 deleter
                if (cb->strong.load(std::memory_order_acquire) > 0)
                {
                    if (cb->deleter && cb->object)
                        cb->deleter(cb->object, cb->deleterState);
                }

                if (EngineAllocator::IsReady())
                {
                    EngineAllocator::Deallocate(cb);
                }
                else
                {
                    delete cb;
                }
            }
        }

        ControlBlock* IPtrManager::AllocBlock(void* object, IPtrManager::DeleterFunc deleter,
            void* deleterState, size_t count,
            ITools::PtrType origin)
        {
            std::lock_guard<std::mutex> lock(Mutex);

            if (terminated)
                throw std::runtime_error("IPtrManager::AllocBlock called after Terminate");

            initialized = true;

            ControlBlock* cb = nullptr;

            if (!FreeBlocks.empty())
            {
                cb = FreeBlocks.front();
                FreeBlocks.pop();
            }
            else
            {
                if (EngineAllocator::IsReady())
                {
                    void* ptr = EngineAllocator::Allocate(static_cast<std::uint64_t>(sizeof(ControlBlock)), static_cast<std::uint64_t>(alignof(ControlBlock)));
                    cb = ::new(ptr) ControlBlock();
                }
                else
                {
                    cb = new ControlBlock();
                }
                Blocks.push_back(cb);
            }

            cb->strong.store(1, std::memory_order_relaxed);
            cb->weak.store(1, std::memory_order_relaxed);
            cb->object = object;
            cb->deleter = deleter;
            cb->deleterState = deleterState;
            cb->count = count;
            cb->origin = origin;

            return cb;
        }

        void IPtrManager::ReleaseBlock(ControlBlock* cb)
        {
            if (!cb)
                return;

            std::lock_guard<std::mutex> lock(Mutex);

            if (!terminated)
                FreeBlocks.push(cb);
        }

    }
}
