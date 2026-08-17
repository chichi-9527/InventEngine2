#pragma once

#include "EngineAPI.h"

namespace INVENT
{

	class IEngine
	{
		IEngine() = default;
	public:
		~IEngine() = default;

		static INVENT_API IEngine& Instatnce();
		INVENT_API void Init();
		INVENT_API void Shutdown();
		
		INVENT_API const char* GetRunPath() const noexcept;

	private:


	};

}

#ifdef __cplusplus
extern "C"
{
#endif // _cplusplus
	INVENT_API const char* INVENT_DLL InventEngineGetRunPath();
#ifdef __cplusplus
}
#endif // _cplusplus
