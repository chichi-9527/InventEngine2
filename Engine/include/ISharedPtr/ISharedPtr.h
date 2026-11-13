#pragma once

/*
// 若提供非 RTTI cast（自行实现）
template<typename To, typename From>
To* EngineCast(From* ptr);
// 1) shared -> shared（向下转型）
template<typename To, typename From>
ISharedPtr<To> CastIShared(const ISharedPtr<From>& p)
{
    if (!p)
        return ISharedPtr<To>();
    To* casted = EngineCast<To>(p.Get());
    if (!casted)
        return ISharedPtr<To>();
    // aliasing：共享同一个 ControlBlock，只换访问地址
    return ISharedPtr<To>(p.GetControlBlock(), casted);
}
// 2) weak -> 使用（先 Lock 再 cast）
IWeakPtr<Base> weak = uniquePtr.ToWeakPtr();
if (auto locked = weak.Lock())
{
    if (Derived* d = EngineCast<Derived>(locked.Get()))
    {
        d->DoSomething();
    }
}   // guard 析构自动释放 strong
// 3) weak -> weak（同地址或偏移 0 的基类；否则先 Lock 再 cast）
IWeakPtr<To> CastIWeak(const IWeakPtr<From>& p)
{
    if (!p.IsValid())
        return IWeakPtr<To>();
    // 注意：只适用于 To 与 From 地址相同的向上转型。
    // 多重继承下偏移非 0 时，请先 Lock() 再 cast。
    return IWeakPtr<To>(p.GetControlBlock());
}
*/

#include "EngineAPI.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <string_view>
#include <type_traits>
#include <stdexcept>
#include <initializer_list>
#include <new>
#include <vector>
#include <queue>
#include <mutex>
#include <concepts>

#ifndef INVENT_API
#define INVENT_API
#endif // !INVENT_API

namespace INVENT
{

    namespace ITools
    {
        enum class PtrType : uint32_t
        {
            UNDEFINED = 0,
            SHARED = 1,
            UNIQUE = 2
        };

        // ---------------------------------------------------------------------------
        // 控制块
        //   strong = 强引用数（拥有者 + 活动中的 weak lock）
        //   weak   = 弱引用数（weak 实例数 + (strong > 0 ? 1 : 0)）
        // ---------------------------------------------------------------------------
        struct ControlBlock
        {
            std::atomic<size_t> strong{ 1 };
            std::atomic<size_t> weak{ 1 };

            void* object = nullptr;                             // 对象原始地址（删除 / weak 解析用）
            void (*deleter)(void* object, void* state) = nullptr;
            void* deleterState = nullptr;

            size_t count = 1;                                   // 数组长度，operator[] 用
            ITools::PtrType origin = ITools::PtrType::UNDEFINED; // UNIQUE / SHARED
        };

        // ---------------------------------------------------------------------------
        // 控制块池管理器
        // ---------------------------------------------------------------------------
        class IPtrManager
        {
        public:
            using DeleterFunc = void (*)(void* object, void* state);

            INVENT_API static IPtrManager& GetInstance();

            INVENT_API bool Init();
            INVENT_API void Terminate();

            INVENT_API ControlBlock* AllocBlock(void* object, DeleterFunc deleter, void* deleterState,
                size_t count, ITools::PtrType origin);
            INVENT_API void ReleaseBlock(ControlBlock* cb);

        private:
            IPtrManager() = default;
            ~IPtrManager();

            IPtrManager(const IPtrManager&) = delete;
            IPtrManager& operator=(const IPtrManager&) = delete;

            std::mutex Mutex;
            std::vector<ControlBlock*> Blocks;     // 所有已分配控制块（Terminate 统一销毁）
            std::queue<ControlBlock*> FreeBlocks;  // 空闲控制块池

            bool initialized = false;
            bool terminated = false;
        };

    }

    namespace detail
    {
        template<typename T>
        void DefaultDeleter(void* object, void*)
        {
            delete static_cast<T*>(object);
        }

        // 默认数组 deleter：new CType[n] -> delete[]
        template<typename T>
        void DefaultArrayDeleter(void* object, void*)
        {
            delete[] static_cast<T*>(object);
        }

        // 自定义分配器状态。分配器必须返回 T* 原始指针：
        //   T* raw = alloc.allocate(n);
        //   alloc.deallocate(raw, n);
        template<typename T, typename Alloc>
        struct AllocatorState
        {
            Alloc allocator;
            uint32_t count = 1;
        };

        template<typename T, typename Alloc>
        void AllocatorDeleter(void* object, void* state)
        {
            auto* s = static_cast<AllocatorState<T, Alloc>*>(state);
            T* ptr = static_cast<T*>(object);

            if (ptr)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    for (uint32_t i = 0; i < s->count; ++i)
                        ptr[i].~T();
                }

                s->allocator.deallocate(ptr, s->count);
            }

            delete s;
        }

        // -----------------------------------------------------------------------
        // 无锁引用计数（热路径，内联）
        // -----------------------------------------------------------------------
        inline void IncStrong(ITools::ControlBlock* cb) noexcept
        {
            cb->strong.fetch_add(1, std::memory_order_relaxed);
        }

        inline void IncWeak(ITools::ControlBlock* cb) noexcept
        {
            cb->weak.fetch_add(1, std::memory_order_relaxed);
        }

        // weak -> strong 提升：CAS 防止 0 -> 1
        inline bool LockStrong(ITools::ControlBlock* cb, void*& ptr) noexcept
        {
            size_t cur = cb->strong.load(std::memory_order_relaxed);

            for (;;)
            {
                if (cur == 0)
                    return false;

                if (cb->strong.compare_exchange_weak(
                    cur, cur + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed))
                {
                    ptr = cb->object;
                    return true;
                }
            }
        }

        inline void ReleaseStrong(ITools::ControlBlock* cb) noexcept
        {
            if (cb->strong.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                // 最后一个强引用：析构对象
                if (cb->deleter && cb->object)
                    cb->deleter(cb->object, cb->deleterState);

                // 释放隐式 weak 槽位
                if (cb->weak.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    ITools::IPtrManager::GetInstance().ReleaseBlock(cb);
            }
        }

        inline void ReleaseWeak(ITools::ControlBlock* cb) noexcept
        {
            if (cb->weak.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                if (cb->strong.load(std::memory_order_acquire) == 0)
                    ITools::IPtrManager::GetInstance().ReleaseBlock(cb);
            }
        }
    } // namespace detail

// ---------------------------------------------------------------------------
// weak lock guard
// ---------------------------------------------------------------------------
template<typename CType>
class IWeakPtrLock
{
public:
    IWeakPtrLock() noexcept = default;

    IWeakPtrLock(ITools::ControlBlock* cb, CType* ptr) noexcept
        : _cb(cb)
        , _ptr(ptr)
    {}

    ~IWeakPtrLock()
    {
        Unlock();
    }

    IWeakPtrLock(const IWeakPtrLock&) = delete;
    IWeakPtrLock& operator=(const IWeakPtrLock&) = delete;

    IWeakPtrLock(IWeakPtrLock&& other) noexcept
        : _cb(other._cb)
        , _ptr(other._ptr)
    {
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    IWeakPtrLock& operator=(IWeakPtrLock&& other) noexcept
    {
        if (this != &other)
        {
            Unlock();
            _cb = other._cb;
            _ptr = other._ptr;
            other._cb = nullptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    void Unlock() noexcept
    {
        if (_cb)
        {
            detail::ReleaseStrong(_cb);
            _cb = nullptr;
            _ptr = nullptr;
        }
    }

    CType* Get() const noexcept { return _ptr; }
    CType* operator->() const noexcept { return _ptr; }
    CType& operator*() const { return *_ptr; }
    explicit operator bool() const noexcept { return _ptr != nullptr; }

private:
    ITools::ControlBlock* _cb = nullptr;
    CType* _ptr = nullptr;
};

// ---------------------------------------------------------------------------
// ISharedPtr
// ---------------------------------------------------------------------------
template<typename CType>
class ISharedPtr
{
public:
    ~ISharedPtr()
    {
        if (_cb)
            detail::ReleaseStrong(_cb);
    }

    constexpr ISharedPtr() noexcept = default;

    ISharedPtr(CType* ptr)
    {
        if (ptr)
        {
            ITools::ControlBlock* cb = nullptr;
            try
            {
                cb = ITools::IPtrManager::GetInstance().AllocBlock(
                    ptr,
                    &detail::DefaultDeleter<CType>,
                    nullptr,
                    1,
                    ITools::PtrType::SHARED);
            }
            catch (...)
            {
                delete ptr;
                throw;
            }
            _cb = cb;
            _ptr = ptr;
        }
    }

    ISharedPtr(CType* ptr, ITools::IPtrManager::DeleterFunc deleter, void* deleterState,
        std::uint32_t count = 1)
    {
        if (ptr)
        {
            ITools::ControlBlock* cb = nullptr;
            try
            {
                cb = ITools::IPtrManager::GetInstance().AllocBlock(
                    ptr, deleter, deleterState, count, ITools::PtrType::SHARED);
            }
            catch (...)
            {
                if (deleter)
                    deleter(ptr, deleterState);
                throw;
            }
            _cb = cb;
            _ptr = ptr;
        }
    }

    // aliasing 构造：与 cb 共享所有权，用 ptr 作为访问地址（引擎 cast 使用）
    ISharedPtr(ITools::ControlBlock* cb, CType* ptr) noexcept
    {
        void* dummy = nullptr;
        if (cb && detail::LockStrong(cb, dummy))
        {
            _cb = cb;
            _ptr = ptr;
        }
    }

    ISharedPtr(const ISharedPtr& other) noexcept
    {
        _cb = other._cb;
        _ptr = other._ptr;
        if (_cb)
            detail::IncStrong(_cb);
    }

    ISharedPtr(ISharedPtr&& other) noexcept
    {
        _cb = other._cb;
        _ptr = other._ptr;
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    // 重绑定转换构造（仅允许 U* -> CType* 的隐式向上转型
    template<typename U>
        requires (!std::same_as<std::remove_cv_t<U>, std::remove_cv_t<CType>>
    && std::convertible_to<U*, CType*>)
        ISharedPtr(const ISharedPtr<U>& other) noexcept
        : _cb(other._cb)
        , _ptr(static_cast<CType*>(other._ptr))
    {
        if (_cb)
            detail::IncStrong(_cb);
    }
    // 重绑定转换构造（仅允许 U* -> CType* 的隐式向上转型
    template<typename U>
        requires (!std::same_as<std::remove_cv_t<U>, std::remove_cv_t<CType>>
    && std::convertible_to<U*, CType*>)
        ISharedPtr(ISharedPtr<U>&& other) noexcept
        : _cb(other._cb)
        , _ptr(static_cast<CType*>(other._ptr))
    {
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    ISharedPtr& operator=(const ISharedPtr& other) noexcept
    {
        if (this != &other)
        {
            if (_cb)
                detail::ReleaseStrong(_cb);

            _cb = other._cb;
            _ptr = other._ptr;

            if (_cb)
                detail::IncStrong(_cb);
        }
        return *this;
    }

    ISharedPtr& operator=(ISharedPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (_cb)
                detail::ReleaseStrong(_cb);

            _cb = other._cb;
            _ptr = other._ptr;

            other._cb = nullptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    ISharedPtr& operator=(CType* ptr)
    {
        if (_cb)
            detail::ReleaseStrong(_cb);

        _cb = nullptr;
        _ptr = nullptr;

        if (ptr)
        {
            ITools::ControlBlock* cb = nullptr;
            try
            {
                cb = ITools::IPtrManager::GetInstance().AllocBlock(
                    ptr,
                    &detail::DefaultDeleter<CType>,
                    nullptr,
                    1,
                    ITools::PtrType::SHARED);
            }
            catch (...)
            {
                delete ptr;
                throw;
            }
            _cb = cb;
            _ptr = ptr;
        }
        return *this;
    }

    // ------------------------------------------------------------------
    // 自定义分配器工厂
    // ------------------------------------------------------------------
    template<typename Alloc, typename... Args>
    static ISharedPtr<CType> Allocate(Alloc& alloc, Args&&... args)
    {
        CType* raw = alloc.allocate(1);
        if (!raw)
            return ISharedPtr<CType>();

        try
        {
            ::new (raw) CType(std::forward<Args>(args)...);
        }
        catch (...)
        {
            alloc.deallocate(raw, 1);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, 1 };
        }
        catch (...)
        {
            if constexpr (!std::is_trivially_destructible_v<CType>)
                raw->~CType();
            alloc.deallocate(raw, 1);
            throw;
        }

        return ISharedPtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            1);
    }

    template<typename Alloc>
    static ISharedPtr<CType> AllocateArray(Alloc& alloc, std::uint32_t n)
    {
        if (n == 0)
            return ISharedPtr<CType>();

        CType* raw = alloc.allocate(n);
        if (!raw)
            return ISharedPtr<CType>();

        std::uint32_t constructed = 0;

        try
        {
            for (std::uint32_t i = 0; i < n; ++i)
            {
                ::new (&raw[i]) CType();
                ++constructed;
            }
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < constructed; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, n };
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < n; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        return ISharedPtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            n);
    }

    template<typename Alloc, typename TInit>
    static ISharedPtr<CType> AllocateArray(Alloc& alloc, std::initializer_list<TInit> init)
    {
        std::uint32_t n = static_cast<std::uint32_t>(init.size());
        if (n == 0)
            return ISharedPtr<CType>();

        CType* raw = alloc.allocate(n);
        if (!raw)
            return ISharedPtr<CType>();

        std::uint32_t constructed = 0;

        try
        {
            auto it = init.begin();
            for (std::uint32_t i = 0; i < n; ++i, ++it)
            {
                ::new (&raw[i]) CType(*it);
                ++constructed;
            }
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < constructed; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, n };
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < n; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        return ISharedPtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            n);
    }

    CType& operator[](std::size_t index) const
    {
        if (_cb && index < _cb->count)
            return _ptr[index];
        throw std::out_of_range("");
    }

    CType* Get() const noexcept { return _ptr; }
    CType* operator->() const noexcept { return _ptr; }
    CType& operator*() const { return *_ptr; }

    std::uint32_t GetCount() const noexcept
    {
        return _cb ? static_cast<std::uint32_t>(_cb->count) : 0;
    }

    ITools::PtrType GetPtrType() const noexcept
    {
        return _cb ? _cb->origin : ITools::PtrType::UNDEFINED;
    }

    ITools::ControlBlock* GetControlBlock() const noexcept { return _cb; }

    bool IsValid() const noexcept { return _cb != nullptr; }
    explicit operator bool() const noexcept { return _cb != nullptr; }

    bool operator==(const ISharedPtr& other) const noexcept { return _cb == other._cb; }
    bool operator!=(const ISharedPtr& other) const noexcept { return _cb != other._cb; }
    bool operator==(const CType* ptr) const noexcept { return _ptr == ptr; }
    bool operator!=(const CType* ptr) const noexcept { return _ptr != ptr; }

private:
    ITools::ControlBlock* _cb = nullptr;
    CType* _ptr = nullptr;
    template<typename> friend class ISharedPtr;
};

// ---------------------------------------------------------------------------
// IUniquePtr 前置声明（供 IWeakPtr 交叉构造/赋值）
// ---------------------------------------------------------------------------
template<typename CType>
class IUniquePtr;

// ---------------------------------------------------------------------------
// IWeakPtr
// ---------------------------------------------------------------------------
template<typename CType>
class IWeakPtr
{
public:
    ~IWeakPtr()
    {
        if (_cb)
            detail::ReleaseWeak(_cb);
    }

    constexpr IWeakPtr() noexcept = default;

    IWeakPtr(const IWeakPtr& other) noexcept
        : _cb(other._cb)
        , _ptr(other._ptr)
    {
        if (_cb)
            detail::IncWeak(_cb);
    }

    IWeakPtr(IWeakPtr&& other) noexcept
        : _cb(other._cb)
        , _ptr(other._ptr)
    {
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    IWeakPtr(const ISharedPtr<CType>& other) noexcept
        : _cb(other.GetControlBlock())
        , _ptr(other.Get())
    {
        if (_cb)
            detail::IncWeak(_cb);
    }

    // 依赖 IUniquePtr 的交叉构造 / 赋值，在 IUniquePtr 定义后实现
    IWeakPtr(const IUniquePtr<CType>& other) noexcept;
    IWeakPtr& operator=(const IUniquePtr<CType>& other) noexcept;

    // aliasing weak 构造：与 cb 共享弱引用，并缓存元素地址（引擎 cast weak 使用）
    IWeakPtr(ITools::ControlBlock* cb, CType* ptr) noexcept
        : _cb(cb)
        , _ptr(ptr)
    {
        if (_cb)
            detail::IncWeak(_cb);
    }

    IWeakPtr& operator=(const IWeakPtr& other) noexcept
    {
        if (this != &other)
        {
            if (_cb)
                detail::ReleaseWeak(_cb);

            _cb = other._cb;
            _ptr = other._ptr;

            if (_cb)
                detail::IncWeak(_cb);
        }
        return *this;
    }

    IWeakPtr& operator=(IWeakPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (_cb)
                detail::ReleaseWeak(_cb);

            _cb = other._cb;
            _ptr = other._ptr;

            other._cb = nullptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    IWeakPtr& operator=(const ISharedPtr<CType>& other) noexcept
    {
        if (_cb)
            detail::ReleaseWeak(_cb);

        _cb = other.GetControlBlock();
        _ptr = other.Get();

        if (_cb)
            detail::IncWeak(_cb);

        return *this;
    }

    // ------------------------------------------------------------------
    // std::weak_ptr::lock() 的对应物
    //   Lock()       : 临时 pin，两种来源通用，返回 move-only guard，自动释放
    //   LockShared() : 永久升格为 ISharedPtr，仅 SHARED 来源有效
    // ------------------------------------------------------------------
    IWeakPtrLock<CType> Lock() const
    {
        if (_cb)
        {
            void* dummy = nullptr;
            if (detail::LockStrong(_cb, dummy))
                return IWeakPtrLock<CType>(_cb, _ptr);
        }
        return IWeakPtrLock<CType>();
    }

    ISharedPtr<CType> LockShared() const
    {
        return ToSharedPtr();
    }

    ISharedPtr<CType> ToSharedPtr() const
    {
        if (_cb
            && _cb->origin == ITools::PtrType::SHARED
            && _cb->strong.load(std::memory_order_acquire) > 0)
        {
            // aliasing 构造内部做 CAS，防止 strong 在检查后归零
            return ISharedPtr<CType>(_cb, _ptr);
        }
        return ISharedPtr<CType>();
    }

    // 不 pin 对象，仅短暂获取原始指针。
    // 对 UniquePtr 派生的 weak_ptr，调用后若对象被释放仍可能悬空，谨慎使用。
    CType* Get() const
    {
        if (_cb && _cb->strong.load(std::memory_order_acquire) > 0)
            return _ptr;
        return nullptr;
    }

    bool IsValid() const noexcept
    {
        return _cb && _cb->strong.load(std::memory_order_acquire) > 0;
    }
    explicit operator bool() const noexcept { return IsValid(); }

    std::uint32_t GetCount() const noexcept
    {
        return _cb ? static_cast<std::uint32_t>(_cb->count) : 0;
    }

    ITools::PtrType GetPtrType() const noexcept
    {
        return _cb ? _cb->origin : ITools::PtrType::UNDEFINED;
    }

    ITools::ControlBlock* GetControlBlock() const noexcept { return _cb; }

    bool operator==(const IWeakPtr& other) const noexcept { return _cb == other._cb; }
    bool operator!=(const IWeakPtr& other) const noexcept { return _cb != other._cb; }
    bool operator==(const CType* ptr) const noexcept { return _ptr == ptr; }
    bool operator!=(const CType* ptr) const noexcept { return _ptr != ptr; }

private:
    ITools::ControlBlock* _cb = nullptr;
    CType* _ptr = nullptr;   // 缓存元素地址（可能因多重继承与 cb->object 不同）
};

// ---------------------------------------------------------------------------
// IWeakPtr 中依赖 IUniquePtr 的交叉构造 / 赋值（延迟定义）
// ---------------------------------------------------------------------------
template<typename CType>
IWeakPtr<CType>::IWeakPtr(const IUniquePtr<CType>& other) noexcept
    : _cb(other.GetControlBlock())
    , _ptr(other.Get())
{
    if (_cb)
        detail::IncWeak(_cb);
}

template<typename CType>
IWeakPtr<CType>& IWeakPtr<CType>::operator=(const IUniquePtr<CType>& other) noexcept
{
    if (_cb)
        detail::ReleaseWeak(_cb);

    _cb = other.GetControlBlock();
    _ptr = other.Get();

    if (_cb)
        detail::IncWeak(_cb);

    return *this;
}

// ---------------------------------------------------------------------------
// IUniquePtr
// ---------------------------------------------------------------------------
template<typename CType>
class IUniquePtr
{
public:
    ~IUniquePtr()
    {
        if (_cb)
            detail::ReleaseStrong(_cb);
    }

    constexpr IUniquePtr() noexcept = default;

    IUniquePtr(CType* ptr)
    {
        if (ptr)
        {
            ITools::ControlBlock* cb = nullptr;
            try
            {
                cb = ITools::IPtrManager::GetInstance().AllocBlock(
                    ptr,
                    &detail::DefaultDeleter<CType>,
                    nullptr,
                    1,
                    ITools::PtrType::UNIQUE);
            }
            catch (...)
            {
                delete ptr;
                throw;
            }
            _cb = cb;
            _ptr = ptr;
        }
    }

    IUniquePtr(CType* ptr, ITools::IPtrManager::DeleterFunc deleter, void* deleterState,
        std::uint32_t count = 1)
    {
        if (ptr)
        {
            ITools::ControlBlock* cb = nullptr;
            try
            {
                cb = ITools::IPtrManager::GetInstance().AllocBlock(
                    ptr, deleter, deleterState, count, ITools::PtrType::UNIQUE);
            }
            catch (...)
            {
                if (deleter)
                    deleter(ptr, deleterState);
                throw;
            }
            _cb = cb;
            _ptr = ptr;
        }
    }

    // aliasing 构造：与 cb 共享所有权，用 ptr 作为访问地址
    IUniquePtr(ITools::ControlBlock* cb, CType* ptr) noexcept
    {
        void* dummy = nullptr;
        if (cb && detail::LockStrong(cb, dummy))
        {
            _cb = cb;
            _ptr = ptr;
        }
    }

    IUniquePtr(const IUniquePtr&) = delete;
    IUniquePtr& operator=(const IUniquePtr&) = delete;

    IUniquePtr(IUniquePtr&& other) noexcept
    {
        _cb = other._cb;
        _ptr = other._ptr;
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    IUniquePtr& operator=(IUniquePtr&& other) noexcept
    {
        if (this != &other)
        {
            if (_cb)
                detail::ReleaseStrong(_cb);

            _cb = other._cb;
            _ptr = other._ptr;

            other._cb = nullptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    // 重绑定转换构造（仅允许 U* -> CType* 的隐式向上转型，移动语义）
    template<typename U>
        requires (!std::same_as<std::remove_cv_t<U>, std::remove_cv_t<CType>>
    && std::convertible_to<U*, CType*>)
        IUniquePtr(IUniquePtr<U>&& other) noexcept
        : _cb(other._cb)
        , _ptr(static_cast<CType*>(other._ptr))
    {
        other._cb = nullptr;
        other._ptr = nullptr;
    }

    // ------------------------------------------------------------------
    // 自定义分配器工厂
    // ------------------------------------------------------------------
    template<typename Alloc, typename... Args>
    static IUniquePtr<CType> Allocate(Alloc& alloc, Args&&... args)
    {
        CType* raw = alloc.allocate(1);
        if (!raw)
            return IUniquePtr<CType>();

        try
        {
            ::new (raw) CType(std::forward<Args>(args)...);
        }
        catch (...)
        {
            alloc.deallocate(raw, 1);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, 1 };
        }
        catch (...)
        {
            if constexpr (!std::is_trivially_destructible_v<CType>)
                raw->~CType();
            alloc.deallocate(raw, 1);
            throw;
        }

        return IUniquePtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            1);
    }

    template<typename Alloc>
    static IUniquePtr<CType> AllocateArray(Alloc& alloc, std::uint32_t n)
    {
        if (n == 0)
            return IUniquePtr<CType>();

        CType* raw = alloc.allocate(n);
        if (!raw)
            return IUniquePtr<CType>();

        std::uint32_t constructed = 0;

        try
        {
            for (std::uint32_t i = 0; i < n; ++i)
            {
                ::new (&raw[i]) CType();
                ++constructed;
            }
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < constructed; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, n };
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < n; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        return IUniquePtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            n);
    }

    template<typename Alloc, typename TInit>
    static IUniquePtr<CType> AllocateArray(Alloc& alloc, std::initializer_list<TInit> init)
    {
        std::uint32_t n = static_cast<std::uint32_t>(init.size());
        if (n == 0)
            return IUniquePtr<CType>();

        CType* raw = alloc.allocate(n);
        if (!raw)
            return IUniquePtr<CType>();

        std::uint32_t constructed = 0;

        try
        {
            auto it = init.begin();
            for (std::uint32_t i = 0; i < n; ++i, ++it)
            {
                ::new (&raw[i]) CType(*it);
                ++constructed;
            }
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < constructed; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        using State = detail::AllocatorState<CType, Alloc>;
        State* state = nullptr;

        try
        {
            state = new State{ alloc, n };
        }
        catch (...)
        {
            for (std::uint32_t i = 0; i < n; ++i)
                raw[i].~CType();
            alloc.deallocate(raw, n);
            throw;
        }

        return IUniquePtr<CType>(
            raw,
            &detail::AllocatorDeleter<CType, Alloc>,
            state,
            n);
    }

    CType& operator[](std::size_t index) const
    {
        if (_cb && index < _cb->count)
            return _ptr[index];
        throw std::out_of_range("");
    }

    CType* Get() const noexcept { return _ptr; }
    CType* operator->() const noexcept { return _ptr; }
    CType& operator*() const { return *_ptr; }

    std::uint32_t GetCount() const noexcept
    {
        return _cb ? static_cast<std::uint32_t>(_cb->count) : 0;
    }

    ITools::PtrType GetPtrType() const noexcept
    {
        return _cb ? _cb->origin : ITools::PtrType::UNDEFINED;
    }

    ITools::ControlBlock* GetControlBlock() const noexcept { return _cb; }

    bool IsValid() const noexcept { return _cb != nullptr; }
    explicit operator bool() const noexcept { return _cb != nullptr; }

    template<typename WeakType>
    bool operator==(const IWeakPtr<WeakType>& other) const noexcept
    {
        return _cb == other.GetControlBlock();
    }

    template<typename WeakType>
    bool operator!=(const IWeakPtr<WeakType>& other) const noexcept
    {
        return _cb != other.GetControlBlock();
    }

    bool operator==(const CType* ptr) const noexcept { return _ptr == ptr; }
    bool operator!=(const CType* ptr) const noexcept { return _ptr != ptr; }

    // UniquePtr -> WeakPtr。弱引用不会延长 UniquePtr 的生命周期。
    IWeakPtr<CType> ToWeakPtr() const
    {
        return IWeakPtr<CType>(_cb, _ptr);
    }

private:
    ITools::ControlBlock* _cb = nullptr;
    CType* _ptr = nullptr;
    template<typename> friend class IUniquePtr;
};

    template<typename CType, typename... Args>
    ISharedPtr<CType> MakeIShared(Args&&... args)
    {
        return ISharedPtr<CType>(new CType(std::forward<Args>(args)...));
    }

    // 默认分配器数组版本：new CType[n] / delete[]
    template<typename CType>
    ISharedPtr<CType> MakeISharedArray(std::uint32_t n)
    {
        if (n == 0)
            return ISharedPtr<CType>();
        CType* ptr = new CType[n];
        // 注意：必须显式传 DefaultArrayDeleter，否则会走 DefaultDeleter（delete ptr）
        return ISharedPtr<CType>(
            ptr,
            &detail::DefaultArrayDeleter<CType>,
            nullptr,
            n);
    }

    template<typename CType, typename Alloc, typename... Args>
    ISharedPtr<CType> AllocateIShared(Alloc& alloc, Args&&... args)
    {
        return ISharedPtr<CType>::template Allocate<Alloc>(
            alloc, std::forward<Args>(args)...);
    }

    template<typename CType, typename... Args>
    IUniquePtr<CType> MakeIUnique(Args&&... args)
    {
        return IUniquePtr<CType>(new CType(std::forward<Args>(args)...));
    }

    // 默认分配器数组版本：new CType[n] / delete[]
    template<typename CType>
    IUniquePtr<CType> MakeIUniqueArray(std::uint32_t n)
    {
        if (n == 0)
            return IUniquePtr<CType>();
        CType* ptr = new CType[n];
        return IUniquePtr<CType>(
            ptr,
            &detail::DefaultArrayDeleter<CType>,
            nullptr,
            n);
    }

    template<typename CType, typename Alloc, typename... Args>
    IUniquePtr<CType> AllocateIUnique(Alloc& alloc, Args&&... args)
    {
        return IUniquePtr<CType>::template Allocate<Alloc>(
            alloc, std::forward<Args>(args)...);
    }

}
