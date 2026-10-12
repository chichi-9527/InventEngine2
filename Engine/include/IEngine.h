#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <atomic>

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

		INVENT_API void EngineExit();
		INVENT_API void ChangeWindowSize(std::uint32_t width, std::uint32_t height);
		INVENT_API std::uint32_t GetWindowWidth() const { return _window_width; }
		INVENT_API std::uint32_t GetWindowHeight() const { return _window_height; }

		std::atomic_bool& GetWindowSizeResized();
		std::atomic_bool& GetEngineShouldExit();
	private:
		std::uint32_t _window_width{ 0 };
		std::uint32_t _window_height{ 0 };
		std::atomic_bool _window_size_resized = false;
		std::atomic_bool _engine_should_exit = false;
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
