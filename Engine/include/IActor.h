#pragma once

#include "EngineAPI.h"
#include "IBase/IBaseObject.h"

#include "IMath/IMathematics.h"

namespace INVENT
{
	/*
	* Enum 宏 Test
	enum class EWeaponType : uint8_t {
		Sword,
		Bow = 2,
		Staff
	};
	INVENT_ENUM_BEGIN(EWeaponType, INVENT::EEnumFlags::BlueprintType)
		INVENT_ENUM_VALUE(EWeaponType, Sword)
		INVENT_ENUM_VALUE(EWeaponType, Bow)
		INVENT_ENUM_VALUE(EWeaponType, Staff)
		INVENT_ENUM_END(EWeaponType)
	*/

	
	class INVENT_API IActor : public IBaseObject
	{
		INVENT_CLASS(IActor, IBaseObject)
	public:
		virtual ~IActor();
		IActor();

		// 非重载：只给函数名 + flags
		INVENT_FUNC(TakeDamage, EFuncFlags::BlueprintCallable, "amount")
		void TakeDamage(float amount);

		// 重载：第三个可选参数给"完整函数类型"（返回类型 + 参数）消歧
		INVENT_FUNC(Add, EFuncFlags::BlueprintCallable, "a,b", int, int)
		int Add(int a, int b);

		INVENT_FUNC(Add, EFuncFlags::BlueprintCallable, "a,b", float, float)
		float Add(float a, float b);

		INVENT_FUNC(Test11, EFuncFlags::BlueprintCallable, "")
		void Test11() const;

		INVENT_PROPERTY(float, test, EPropertyFlags::EditAnywhere)
		float test{ 1.0f };

		INVENT_PROPERTY(DVec3, position, EPropertyFlags::EditAnywhere)
		DVec3 position{ 1.0 };
	};

}

