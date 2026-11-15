#pragma once

#include "EngineAPI.h"
#include "IBase/IBaseObject.h"

#include "IMath/IMathematics.h"

namespace INVENT
{
	class INVENT_API IActor : public IBaseObject
	{
		INVENT_CLASS(IActor, IBaseObject)
	public:
		virtual ~IActor();
		IActor();

		// 非重载：只给函数名 + flags
		INVENT_FUNC(TakeDamage, EFuncFlags::BlueprintCallable)
		void TakeDamage(float amount);

		// 重载：第三个可选参数给"完整函数类型"（返回类型 + 参数）消歧
		INVENT_FUNC(Add, EFuncFlags::BlueprintCallable, int, int, int)
		int Add(int a, int b);

		INVENT_FUNC(Add, EFuncFlags::BlueprintCallable, float, float, float)
		float Add(float a, float b);

		INVENT_FUNC_CONST(Test11, EFuncFlags::BlueprintCallable, void)
		void Test11() const;

		INVENT_PROPERTY(float, test, EPropertyFlags::EditAnywhere)
		float test{ 1.0f };

		INVENT_PROPERTY(DVec3, position, EPropertyFlags::EditAnywhere)
		DVec3 position{ 1.0 };
	};

}

