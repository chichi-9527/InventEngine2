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

#endif //1
