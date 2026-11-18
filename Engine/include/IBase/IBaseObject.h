#pragma once

#include "EngineAPI.h"
#include "Memory/Memory.h"
#include "ISharedPtr/ISharedPtr.h"
#include "ILog.h"

#include <entt/meta/meta.hpp>
#include <entt/meta/factory.hpp>

#include <unordered_map>
#include <string>
#include <functional>
#include <type_traits>
#include <concepts>
#include <utility>
#include <cstdint>


namespace INVENT
{
	INVENT_API entt::meta_ctx& GetEngineEnttMetaContext();

	enum class EPropertyFlags : uint32_t
	{
		None = 0,
		EditAnywhere = 1u << 0,   // 编辑器里可编辑（实例级）
		ReadOnly = 1u << 1,   // 只读（可见不可改）
		Serialize = 1u << 2,   // 参与序列化
		HideInEditor = 1u << 3,   // 编辑器里隐藏
	};

	enum class EFuncFlags : std::uint32_t
	{
		None = 0,
		BlueprintCallable = 1u << 0,   // 可被脚本调用
		EditorCallable = 1u << 1,
	};

	constexpr EPropertyFlags operator|(EPropertyFlags a, EPropertyFlags b) noexcept
	{
		return static_cast<EPropertyFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
	}
	constexpr EPropertyFlags operator&(EPropertyFlags a, EPropertyFlags b) noexcept
	{
		return static_cast<EPropertyFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
	}
	constexpr EFuncFlags operator|(EFuncFlags a, EFuncFlags b) noexcept
	{
		return static_cast<EFuncFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
	}
	constexpr EFuncFlags operator&(EFuncFlags a, EFuncFlags b) noexcept
	{
		return static_cast<EFuncFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
	}

	constexpr bool HasFlag(EPropertyFlags value, EPropertyFlags flag) noexcept
	{
		return (value & flag) != EPropertyFlags::None;
	}
	constexpr bool HasFlag(EFuncFlags value, EFuncFlags flag) noexcept
	{
		return (value & flag) != EFuncFlags::None;
	}

	template <typename... Args>
	struct FuncResolver 
	{
		// 匹配非 const 成員函數
		template <typename ClassType, typename RetType>
		static constexpr auto get(RetType(ClassType::* ptr)(Args...))
		{
			return ptr;
		}
		// 匹配 const 成員函數
		template <typename ClassType, typename RetType>
		static constexpr auto get(RetType(ClassType::* ptr)(Args...) const)
		{
			return ptr;
		}
		// 匹配類別 static 成員函數（或自由函數）
		template <typename RetType>
		static constexpr auto get(RetType(*ptr)(Args...))
		{
			return ptr;
		}
	};

	template <>
	struct FuncResolver<void> {
		template <typename PtrType>
		static constexpr auto get(PtrType ptr)
		{
			return ptr;
		}
	};

	struct FuncMetaInfo {
		EFuncFlags flags;
		std::string_view paramNames; // 儲存參數名稱
	};

	enum class EEnumFlags : uint32_t {
		None = 0,
		BlueprintType = 1 << 0,
		Bitflags = 1 << 1, // 若為位元遮罩枚舉，生成 Lua 時可加上特殊標記
	};

	// 整個枚舉類型的元資料
	struct EnumMetaInfo {
		EEnumFlags flags;
	};

	template <typename T>
	struct AutoMetaRegistrar {
		template <typename RegFunc>
		AutoMetaRegistrar(RegFunc&& func)
		{
			// DLL 載入時（靜態初始化階段）：執行註冊
			func();
		}
		~AutoMetaRegistrar()
		{
			// DLL 卸載時（靜態變數銷毀階段）：精準從引擎 Context 中移除型別 T
			entt::meta_reset<T>(GetEngineEnttMetaContext());
		}
	};

	class IBaseObject;
	using ObjectCreatorFunc = IUniquePtr<IBaseObject>(*)();

	struct ClassTypeInfo
	{
		const char* Name;
		const ClassTypeInfo* SuperClass;
	};

	// 约束1 必须是派生自 IBaseObject 的类
	template <typename T>
	concept CInventObject = std::derived_from<std::remove_cv_t<T>, IBaseObject>;
	// 约束2 必须是合法的反射对象,没有漏写 INVENT_CLASS 宏
	template <typename T>
	concept CReflectableObject = CInventObject<T> && requires {
		typename std::remove_cv_t<T>::ThisClass;
		requires std::same_as<typename std::remove_cv_t<T>::ThisClass, std::remove_cv_t<T>>;
	};

	template<typename T>
	void DefaultEngineDeleter(void* obj, void* state)
	{
		if (obj != nullptr)
		{
			static_cast<T*>(obj)->~T();
			EngineAllocator::Deallocate(obj);
		}
	}

	template <CReflectableObject T, typename... Args>
	IUniquePtr<T> EngineCreateUnique(Args&& ...args)
	{
		if (!EngineAllocator::IsReady())
		{
			INVENT_LOG_ERROR("[AllocateIUnique] Engine allocator is not ready!");
			return nullptr;
		}
		void* rawMem = EngineAllocator::Allocate(
			static_cast<std::uint64_t>(sizeof(T)),
			static_cast<std::uint64_t>(alignof(T))
		);
		if (rawMem == nullptr)
		{
			return nullptr;
		}
		T* obj;
		try
		{
			obj = ::new(rawMem) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			EngineAllocator::Deallocate(rawMem);
			throw;
		}
		IUniquePtr<T> ptr(obj, &DefaultEngineDeleter<T>, nullptr, 1);
		obj->_weakSelf = IWeakPtr<IBaseObject>(ptr.GetControlBlock(), static_cast<IBaseObject*>(obj));
		return ptr;
	}
	template <CReflectableObject T, typename... Args>
	[[nodiscard]] ISharedPtr<T> EngineCreateShared(Args&&... args)
	{
		if (!EngineAllocator::IsReady())
		{
			INVENT_LOG_ERROR("[EngineCreateShared] Engine allocator is not ready!");
			return nullptr;
		}
		void* rawMem = EngineAllocator::Allocate(
			static_cast<std::uint64_t>(sizeof(T)),
			static_cast<std::uint64_t>(alignof(T))
		);
		if (rawMem == nullptr) return nullptr;

		T* obj;
		try
		{
			obj = ::new(rawMem) T(std::forward<Args>(args)...);
		}
		catch (...)
		{
			EngineAllocator::Deallocate(rawMem);
			throw;
		}
		ISharedPtr<T> ptr(obj, &DefaultEngineDeleter<T>, nullptr, 1);
		obj->_weakSelf = IWeakPtr<IBaseObject>(ptr.GetControlBlock(), static_cast<IBaseObject*>(obj));
		return ptr;
	}


#pragma warning(push)
#pragma warning(disable: 4251)
	class INVENT_API IBaseObject
	{
	public:
		using ThisClass = IBaseObject;
		virtual ~IBaseObject();
		IBaseObject();

		static void _reg_MetaClass()
		{
			entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()}.type("IBaseObject");
		}
		inline static int _autoReg_MetaClass = (ThisClass::_reg_MetaClass(), 0); 

		IBaseObject(const IBaseObject&) = delete;
		IBaseObject& operator=(const IBaseObject&) = delete;

		static const ClassTypeInfo* StaticClass()
		{
			static ClassTypeInfo info{ "IBaseObject", nullptr };
			return &info;
		}

		virtual const ClassTypeInfo* GetClass() const
		{
			return StaticClass();
		}

		bool IsA(const ClassTypeInfo* TargetClass) const
		{
			if (!TargetClass) return false;
			for (const ClassTypeInfo* Current = GetClass(); Current != nullptr; Current = Current->SuperClass)
			{
				if (Current == TargetClass)
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] IWeakPtr<IBaseObject> AsWeakBase() const noexcept
		{
			return _weakSelf;
		}

	private:
		template <CReflectableObject T, typename... Args>
		friend IUniquePtr<T> EngineCreateUnique(Args&&... args);
		template <CReflectableObject T, typename... Args>
		friend ISharedPtr<T> EngineCreateShared(Args&&... args);

		mutable IWeakPtr<IBaseObject> _weakSelf;

	};
#pragma warning(pop)

	// 裸指针
	template <CReflectableObject To, CInventObject From>
		requires (!std::is_const_v<From>)
	[[nodiscard]] To* EngineCast(From* Obj) noexcept
	{
		if constexpr (std::derived_from<From, To>)
		{
			return static_cast<To*>(Obj);
		}
		else
		{
			// 运行时安全向下转型（Downcast）或侧向转型
			if (Obj != nullptr && Obj->IsA(To::StaticClass()))
			{
				return static_cast<To*>(Obj);
			}
			return nullptr;
		}
	}
	template <CReflectableObject To, CInventObject From>
	[[nodiscard]] const To* EngineCast(const From* Obj) noexcept
	{
		if constexpr (std::derived_from<From, To>)
		{
			return static_cast<const To*>(Obj);
		}
		else
		{
			if (Obj != nullptr && Obj->IsA(To::StaticClass()))
			{
				return static_cast<const To*>(Obj);
			}
			return nullptr;
		}
	}
	// 智能指针
	template <CReflectableObject To, CInventObject From>
	[[nodiscard]] ISharedPtr<To> Cast(const ISharedPtr<From>& p)
	{
		if (!p)
		{
			return ISharedPtr<To>();
		}

		if constexpr (std::derived_from<From, To>)
		{
			return ISharedPtr<To>(p);
		}
		else
		{
			To* casted = EngineCast<To>(p.Get());
			if (!casted)
			{
				return ISharedPtr<To>();
			}
			// Aliasing 构造：共享同一个 ControlBlock，只更换类型与偏移地址
			return ISharedPtr<To>(p.GetControlBlock(), casted);
		}
	}
	template <CReflectableObject To, CInventObject From>
	[[nodiscard]] IWeakPtr<To> Cast(const IWeakPtr<From>& p)
	{
		if (!p.IsValid())
		{
			return IWeakPtr<To>();
		}

		// 情况 A：向上转型（如 IWeakPtr<APlayer> -> IWeakPtr<IBaseObject>）
		if constexpr (std::derived_from<From, To>)
		{
			return IWeakPtr<To>(p.GetControlBlock(), static_cast<To*>(p.Get()));
		}
		else
		{
			// 情况 B：向下转型（如 IWeakPtr<IBaseObject> -> IWeakPtr<APlayer>）
			// 必须先 Lock() 锁定生命周期并校验真实类型！
			if (auto locked = p.Lock())
			{
				if (To* casted = EngineCast<To>(locked.Get()))
				{
					return IWeakPtr<To>(p.GetControlBlock(), casted);
				}
			}
			return IWeakPtr<To>();
		}
	}
	template <CReflectableObject To, CInventObject From>
	[[nodiscard]] IWeakPtr<To> CastToWeak(const IUniquePtr<From>& p)
	{
		if (!p)
		{
			return IWeakPtr<To>();
		}

		To* casted = EngineCast<To>(p.Get());
		if (!casted)
		{
			return IWeakPtr<To>();
		}

		// 利用 UniquePtr 转 WeakPtr 的机制，同时绑定正确的 casted 指针地址
		// （注：若 IWeakPtr 构造函数为 IWeakPtr(ControlBlock*, To*)，可直接传入 casted）
		return Cast<To>(p.ToWeakPtr());
	}

	template <CReflectableObject To, CInventObject From>
		requires (!std::is_const_v<From>)
	[[nodiscard]] To* Cast(From* Obj) noexcept { return EngineCast<To>(Obj); }
	template <CReflectableObject To, CInventObject From>
	[[nodiscard]] const To* Cast(const From* Obj) noexcept { return EngineCast<To>(Obj); }


	class ObjectRegistry
	{
		ObjectRegistry() = default;
	public:
		INVENT_API static ObjectRegistry& Instance();

		INVENT_API void RegisterClass(const char* name, ObjectCreatorFunc creator);
		INVENT_API IUniquePtr<IBaseObject> Create(const char* name) const;
		// 卸载 Game.dll 前清空注册表
		INVENT_API void Clear();

	private:
		std::unordered_map<std::string, ObjectCreatorFunc> _registries;

	};
	// 辅助模板类，在构造时自动向 Registry 注册
	template <typename T>
	class AutoRegister 
	{
	public:
		explicit AutoRegister(const char* name)
		{
			if constexpr (!std::is_abstract_v<T> && std::is_default_constructible_v<T>)
			{
				ObjectRegistry::Instance().RegisterClass(name, []() -> IUniquePtr<IBaseObject> {
					return EngineCreateUnique<T>();
					});
			}
		}

	};


	enum class ESpawnOwnership
	{
		Unique, // 交由 Level 託管，返回 IWeakPtr
		Shared  // 交由呼叫者共享，返回 ISharedPtr
	};
}


#define INVENT_STRUCT(StructName) \
    using ThisClass = StructName; \
    static void _reg_MetaStruct() { \
		static INVENT::AutoMetaRegistrar<ThisClass> _meta_class_guard{[](){entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()}.type(#StructName);}};\
    } \
    inline static int _autoReg_MetaStruct = (ThisClass::_reg_MetaStruct(), 0);

#define INVENT_CLASS(CLASS_NAME, SUPER_CLASS_NAME) \
public: \
    using Super = SUPER_CLASS_NAME; \
    using ThisClass = CLASS_NAME; \
    \
    /* 靜態帶參建立工廠 */ \
    template <typename... Args> \
    [[nodiscard]] static INVENT::IUniquePtr<CLASS_NAME> CreateUnique(Args&&... args) \
    { \
        return INVENT::EngineCreateUnique<CLASS_NAME>(std::forward<Args>(args)...); \
    } \
    \
    template <typename... Args> \
    [[nodiscard]] static INVENT::ISharedPtr<CLASS_NAME> CreateShared(Args&&... args) \
    { \
        return INVENT::EngineCreateShared<CLASS_NAME>(std::forward<Args>(args)...); \
    } \
    \
    /* 取得型別精確的自身弱引用 */ \
    [[nodiscard]] INVENT::IWeakPtr<CLASS_NAME> AsWeak() noexcept \
    { \
        auto baseWeak = AsWeakBase(); \
        return INVENT::IWeakPtr<CLASS_NAME>(baseWeak.GetControlBlock(), this); \
    } \
    \
    [[nodiscard]] INVENT::IWeakPtr<const CLASS_NAME> AsWeak() const noexcept \
    { \
        auto baseWeak = AsWeakBase(); \
        return INVENT::IWeakPtr<const CLASS_NAME>(baseWeak.GetControlBlock(), this); \
    } \
    \
    /* 反射型別元資料 */ \
    static const INVENT::ClassTypeInfo* StaticClass() \
    { \
        static INVENT::ClassTypeInfo info{ \
            #CLASS_NAME, \
            SUPER_CLASS_NAME::StaticClass() \
        }; \
        return &info; \
    } \
    \
    virtual const INVENT::ClassTypeInfo* GetClass() const override \
    { \
        return StaticClass(); \
    } \
    \
private: \
	static void _reg_autoRegister(){static INVENT::AutoRegister<CLASS_NAME> _autoRegister##CLASS_NAME{ #CLASS_NAME };}\
    inline static int _auto_register = (ThisClass::_reg_autoRegister(), 0); \
	\
	static void _reg_MetaClass(){\
		static INVENT::AutoMetaRegistrar<ThisClass> _meta_class_guard{[](){\
			entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()}.type(#CLASS_NAME) \
				.base<Super>(); }};\
	} \
	inline static int _autoReg_MetaClass = (ThisClass::_reg_MetaClass(), 0); \


#define INVENT_PROPERTY(Type, Name, Flags) \
    static void _reg_##Name() { \
        static_assert(std::is_same_v<Type, decltype(ThisClass::Name)>); \
        entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()} \
            .data<&ThisClass::Name>(#Name) \
            .custom<INVENT::EPropertyFlags>(Flags); \
    } \
    inline static int _autoReg_##Name = (ThisClass::_reg_##Name(), 0);


// —— 基礎工具宏 ——
#define INVENT_EXPAND(x) x
#define INVENT_CAT_IMPL(a, b) a##b
#define INVENT_CAT(a, b) INVENT_CAT_IMPL(a, b)

// —— 1. 無簽名版本（無重載時使用：固定 3 個參數：FuncName, Flags, ParamNames） ——
#define INVENT_FUNC_NO_SIG(ID, FuncName, Flags, ParamNames) \
    static void INVENT_CAT(_reg_func_, ID)() { \
        constexpr auto ptr = INVENT::FuncResolver<void>::get(&ThisClass::FuncName); \
        entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()} \
            .func<ptr>(#FuncName) \
            .custom<INVENT::FuncMetaInfo>(INVENT::FuncMetaInfo{Flags, ParamNames}); \
    } \
    inline static int INVENT_CAT(_autoReg_func_, ID) = (ThisClass::INVENT_CAT(_reg_func_, ID)(), 0);

// —— 2. 有簽名版本（重載或無參重載時使用：4 個及以上參數：FuncName, Flags, ParamNames, Args...） ——
#define INVENT_FUNC_WITH_SIG(ID, FuncName, Flags, ParamNames, ...) \
    static void INVENT_CAT(_reg_func_, ID)() { \
        constexpr auto ptr = INVENT::FuncResolver<__VA_ARGS__>::get(&ThisClass::FuncName); \
        entt::meta_factory<ThisClass>{INVENT::GetEngineEnttMetaContext()} \
            .func<ptr>(#FuncName) \
            .custom<INVENT::FuncMetaInfo>(INVENT::FuncMetaInfo{Flags, ParamNames}); \
    } \
    inline static int INVENT_CAT(_autoReg_func_, ID) = (ThisClass::INVENT_CAT(_reg_func_, ID)(), 0);

// —— 3. 參數個數計數器（前 3 個固定為 FuncName, Flags, ParamNames，第 4 個起為 Args...） ——
#define INVENT_FUNC_SELECT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, NAME, ...) NAME
#define INVENT_FUNC_SELECT(...) \
    INVENT_EXPAND(INVENT_FUNC_SELECT_IMPL(__VA_ARGS__, \
        INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, \
        INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, \
        INVENT_FUNC_WITH_SIG, INVENT_FUNC_WITH_SIG, INVENT_FUNC_NO_SIG, ERROR, ERROR))

// —— 統一入口 ——
#define INVENT_FUNC(...) \
    INVENT_EXPAND(INVENT_FUNC_SELECT(__VA_ARGS__)(INVENT_CAT(Func_, __COUNTER__), __VA_ARGS__))



// —— 開始註冊枚舉 ——
#define INVENT_ENUM_BEGIN(EnumType, Flags) \
    namespace { \
        static void INVENT_CAT(_reg_enum_, EnumType)() { \
            entt::meta_factory<EnumType>{INVENT::GetEngineEnttMetaContext()} \
                .type(#EnumType) \
                .custom<INVENT::EnumMetaInfo>(INVENT::EnumMetaInfo{Flags})

// —— 註冊單個枚舉值 ——
#define INVENT_ENUM_VALUE(EnumType, ValueName) \
                .data<EnumType::ValueName>(#ValueName)

// —— 結束註冊枚舉（觸發靜態初始化自動執行） ——
#define INVENT_ENUM_END(EnumType) \
            ; \
        } \
          inline static INVENT::AutoMetaRegistrar<EnumType> INVENT_CAT(_autoReg_enum_, EnumType){ \
            &INVENT_CAT(_reg_enum_, EnumType) \
        }; \
    }
