#pragma once

#if 1

#include <RegisterTest.h>
#include <ILog.h>

#include <string>
#include <format>
#include <cstdint>
#include <string_view>

class RegisterUseTest
{
public:
    static void Test()
    {
        entt::meta_ctx& ctx = INVENT::GetEngineEnttMetaContext();

        entt::meta_type enumType = entt::resolve(ctx, entt::hashed_string{ "EWeaponType" });
        if (!enumType)
        {
            INVENT_LOG_DEBUG("[AUIWindow] 找不到 EWeaponType 的反射資訊！");
            return;
        }
        if (!enumType.is_enum())
        {
            INVENT_LOG_DEBUG("[AUIWindow] EWeaponType 不是 enum！");
            return;
        }
        INVENT_LOG_DEBUG(std::format("[AUIWindow] === 遍歷enum : EWeaponType ==="));
        for (auto&& [propId, propData] : enumType.data())
        {
            // 取得枚舉的實際數值
            // 因為枚舉項是靜態常量，get({}) 傳入空的 meta_any 即可取得值
            entt::meta_any valAny = propData.get({});
            int64_t numericValue = 0;
            if (valAny.allow_cast<int64_t>())
            {
                numericValue = valAny.cast<int64_t>();
            }
            else if (valAny.allow_cast<uint64_t>())
            {
                numericValue = static_cast<int64_t>(valAny.cast<uint64_t>());
            }
            INVENT_LOG_DEBUG(std::format("[AUIWindow] {} = {}.", propData.name(), numericValue));
        }

        entt::meta_type actorType = entt::resolve(ctx, entt::hashed_string{ "IRegister" });
        if (!actorType)
        {
            INVENT_LOG_DEBUG("[AUIWindow] 找不到 IActor 的反射資訊！");
            return;
        }

        INVENT_LOG_DEBUG("[AUIWindow] === 類型資訊 ===");
        INVENT_LOG_DEBUG(std::format("[AUIWindow] 類型名稱 (name): {}.", actorType.name()));
        INVENT_LOG_DEBUG(std::format("[AUIWindow] 類型 ID (id): {}.", (uint32_t)actorType.id()));

        for (auto&& [baseId, baseType] : actorType.base())
        {
            INVENT_LOG_DEBUG(std::format("[AUIWindow] 父類別 (Super): {}.", baseType.name()));
        }

        auto actor = INVENT::IRegister::CreateShared();
        actor->test = 42.5f;
        actor->position = INVENT::DVec3{ 10.0, 20.0, 30.0 };

        // actorType.from_void(actor.Get());
        entt::meta_any instance = entt::forward_as_meta(ctx, *actor);

        INVENT_LOG_DEBUG(std::format("[AUIWindow] === 遍歷屬性 (Properties) ==="));
        for (auto&& [propId, propData] : actorType.data())
        {
            
            // 取得屬性名稱與屬性類型名稱
            const char* propName = propData.name();
            entt::meta_type propType = propData.type();
            INVENT_LOG_DEBUG(std::format("[AUIWindow] 屬性名: {} | 類型: {}.", propName, propType.name()));

            // 檢查自訂 Flag (假設你在 INVENT_PROPERTY 裡有掛載 .custom<INVENT::EPropertyFlags>(Flags))
            if (const INVENT::EPropertyFlags* flags = propData.custom())
            {
                INVENT_LOG_DEBUG(std::format("[AUIWindow]  -> 擁有自訂 EPropertyFlags: {}.", static_cast<int>(*flags)));
            }

            // 取得變數的當前值 (meta_any)
            entt::meta_any value = propData.get(instance);
            if (!value) continue;

            // 情況 A：基礎類型 (float)
            if (propType == entt::resolve<float>(ctx))
            {
                INVENT_LOG_DEBUG(std::format("[AUIWindow]  propType name: {}.", propType.name()));
                float fVal = value.cast<float>();
                INVENT_LOG_DEBUG(std::format("[AUIWindow]  -> 當前值 (float): {}.", fVal));

                // 測試透過反射修改值
                propData.set(instance, 99.9f);
                INVENT_LOG_DEBUG(std::format("[AUIWindow]  -> 修改後 actor.test = {}.", actor->test));
            }
            // 情況 B：巢狀反射結構 (DVec3)
            else if (propType == entt::resolve<INVENT::DVec3>(ctx))
            {
                INVENT_LOG_DEBUG(std::format("[AUIWindow]  -> 展開結構體 DVec3 成員:"));
                for (auto&& [subId, subData] : propType.data())
                {
                    entt::meta_any subVal = subData.get(value);
                    if (subVal && subData.type() == entt::resolve<double>(ctx))
                    {
                        INVENT_LOG_DEBUG(std::format("[AUIWindow]     .{} = {}.", subData.name(), subVal.cast<double>()));
                    }
                }

                // 測試透過反射修改巢狀成員 (例如將 position.x 改為 777.0)
                if (entt::meta_data xData = propType.data(entt::hashed_string{ "x" }))
                {
                    xData.set(value, 777.0);
                    INVENT_LOG_DEBUG(std::format("[AUIWindow]  -> 修改後 actor.position.x = {}.", actor->position.x));
                }
            }
        }

        INVENT_LOG_DEBUG(std::format("[AUIWindow] === 遍歷函数 (Properties) ==="));
        for (auto&& [id, func] : actorType.func())
        {
            // 重載函數
            for (entt::meta_func currFunc = func; currFunc; currFunc = currFunc.next())
            {
                INVENT_LOG_DEBUG(std::format("[AUIWindow] 函数名：{}, is const: {}, is tatic: {}.",
                    currFunc.name(), currFunc.is_const(), currFunc.is_static()));
                entt::meta_type retType = currFunc.ret();
                INVENT_LOG_DEBUG(std::format("[AUIWindow] 函数返回类型str count：{}", retType.info().name().size()));
                INVENT_LOG_DEBUG(std::format("[AUIWindow] 函数返回类型：{}", retType.name()));
                
                std::size_t arity = currFunc.arity();
                for (std::size_t i = 0; i < arity; ++i)
                {
                    entt::meta_type argType = currFunc.arg(i);
                    // 這裡就能拿到第 i 個參數的型別資訊了！
                    INVENT_LOG_DEBUG(std::format("[AUIWindow] 函数参数类型 {}: {}", i, argType.name()));
                }

                // 3. 獲取我們存入的參數名稱與 Flags
                if (const INVENT::FuncMetaInfo* info = currFunc.custom(); info != nullptr)
                {
                    INVENT::EFuncFlags flags = info->flags;
                    std::string_view paramNames = info->paramNames; // 拿到 "a, b"
                    // 在這裡將 paramNames 依逗號 split，與上面的 argType 逐一配對，即可輸出 Lua EmmyLua 註解！
                    INVENT_LOG_DEBUG(std::format("[AUIWindow] 函数参数名 : {}", paramNames.data()));

                }

            }
        }

    }
};

/*
    test log 输出：

[2026-11-18 16:21:16.6107876][DEBUG]:[AUIWindow] === 遍歷enum : EWeaponType ===
[2026-11-18 16:21:16.6108478][DEBUG]:[AUIWindow] Sword = 0.
[2026-11-18 16:21:16.6108599][DEBUG]:[AUIWindow] Bow = 2.
[2026-11-18 16:21:16.6108684][DEBUG]:[AUIWindow] Staff = 3.
[2026-11-18 16:21:16.6108767][DEBUG]:[AUIWindow] === 類型資訊 ===
[2026-11-18 16:21:16.6108855][DEBUG]:[AUIWindow] 類型名稱 (name): IRegister.
[2026-11-18 16:21:16.6108939][DEBUG]:[AUIWindow] 類型 ID (id): 1690279213.
[2026-11-18 16:21:16.6109067][DEBUG]:[AUIWindow] 父類別 (Super): IBaseObject.
[2026-11-18 16:21:16.6109309][DEBUG]:[AUIWindow] === 遍歷屬性 (Properties) ===
[2026-11-18 16:21:16.6109444][DEBUG]:[AUIWindow] 屬性名: test | 類型: float.
[2026-11-18 16:21:16.6109532][DEBUG]:[AUIWindow]  -> 擁有自訂 EPropertyFlags: 1.
[2026-11-18 16:21:16.6109624][DEBUG]:[AUIWindow]  propType name: float.
[2026-11-18 16:21:16.6109801][DEBUG]:[AUIWindow]  -> 當前值 (float): 42.5.
[2026-11-18 16:21:16.6109900][DEBUG]:[AUIWindow]  -> 修改後 actor.test = 99.9.
[2026-11-18 16:21:16.6109998][DEBUG]:[AUIWindow] 屬性名: position | 類型: DVec3.
[2026-11-18 16:21:16.6110083][DEBUG]:[AUIWindow]  -> 擁有自訂 EPropertyFlags: 1.
[2026-11-18 16:21:16.6110171][DEBUG]:[AUIWindow]  -> 展開結構體 DVec3 成員:
[2026-11-18 16:21:16.6110360][DEBUG]:[AUIWindow]     .x = 10.
[2026-11-18 16:21:16.6110524][DEBUG]:[AUIWindow]     .y = 20.
[2026-11-18 16:21:16.6110609][DEBUG]:[AUIWindow]     .z = 30.
[2026-11-18 16:21:16.6110747][DEBUG]:[AUIWindow]  -> 修改後 actor.position.x = 10.
[2026-11-18 16:21:16.6110835][DEBUG]:[AUIWindow] === 遍歷函数 (Properties) ===
[2026-11-18 16:21:16.6110921][DEBUG]:[AUIWindow] 函数名：TakeDamage, is const: false, is tatic: false.
[2026-11-18 16:21:16.6111076][DEBUG]:[AUIWindow] 函数返回类型str count：4
[2026-11-18 16:21:16.6111164][DEBUG]:[AUIWindow] 函数返回类型：void
[2026-11-18 16:21:16.6111248][DEBUG]:[AUIWindow] 函数参数类型 0: float
[2026-11-18 16:21:16.6111333][DEBUG]:[AUIWindow] 函数参数名 : amount
[2026-11-18 16:21:16.6111416][DEBUG]:[AUIWindow] 函数名：Add, is const: false, is tatic: false.
[2026-11-18 16:21:16.6111507][DEBUG]:[AUIWindow] 函数返回类型str count：3
[2026-11-18 16:21:16.6111588][DEBUG]:[AUIWindow] 函数返回类型：std_int32
[2026-11-18 16:21:16.6111670][DEBUG]:[AUIWindow] 函数参数类型 0: std_int32
[2026-11-18 16:21:16.6111754][DEBUG]:[AUIWindow] 函数参数类型 1: std_int32
[2026-11-18 16:21:16.6111833][DEBUG]:[AUIWindow] 函数参数名 : a,b
[2026-11-18 16:21:16.6111915][DEBUG]:[AUIWindow] 函数名：Add, is const: false, is tatic: false.
[2026-11-18 16:21:16.6111998][DEBUG]:[AUIWindow] 函数返回类型str count：5
[2026-11-18 16:21:16.6112080][DEBUG]:[AUIWindow] 函数返回类型：float
[2026-11-18 16:21:16.6112164][DEBUG]:[AUIWindow] 函数参数类型 0: float
[2026-11-18 16:21:16.6112247][DEBUG]:[AUIWindow] 函数参数类型 1: float
[2026-11-18 16:21:16.6112328][DEBUG]:[AUIWindow] 函数参数名 : a,b
[2026-11-18 16:21:16.6112414][DEBUG]:[AUIWindow] 函数名：Test11, is const: true, is tatic: false.
[2026-11-18 16:21:16.6112505][DEBUG]:[AUIWindow] 函数返回类型str count：4
[2026-11-18 16:21:16.6112602][DEBUG]:[AUIWindow] 函数返回类型：void
[2026-11-18 16:21:16.6112690][DEBUG]:[AUIWindow] 函数参数名 :
[2026-11-18 16:21:16.6112780][DEBUG]:[AUIWindow] 函数名：StaticTestF, is const: false, is tatic: true.
[2026-11-18 16:21:16.6112871][DEBUG]:[AUIWindow] 函数返回类型str count：4
[2026-11-18 16:21:16.6112960][DEBUG]:[AUIWindow] 函数返回类型：void
[2026-11-18 16:21:16.6113046][DEBUG]:[AUIWindow] 函数参数名 :

*/

#endif //1
