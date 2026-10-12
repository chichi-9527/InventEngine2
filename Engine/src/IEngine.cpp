#include "IEngine.h"

#include "IEngineTools.h"

namespace INVENT
{

	IEngine& IEngine::Instatnce()
	{
		static IEngine e;
		return e;
	}

	void IEngine::Init()
	{
		IEngineTools::Instance().Init();
	}
	void IEngine::Shutdown()
	{
		IEngineTools::Instance().Clear();
	}

	const char* IEngine::GetRunPath() const noexcept
	{
		return IEngineTools::GetRunPath().c_str();
	}

	void IEngine::EngineExit()
	{
		_engine_should_exit.store(true, std::memory_order_release);
	}

	void IEngine::ChangeWindowSize(std::uint32_t width, std::uint32_t height)
	{
		_window_width = width;
		_window_height = height;
		_window_size_resized.store(true, std::memory_order_release);
	}

	std::atomic_bool& IEngine::GetWindowSizeResized()
	{
		return _window_size_resized;
	}
	std::atomic_bool& IEngine::GetEngineShouldExit()
	{
		return _engine_should_exit;
	}
}

const char* INVENT_DLL InventEngineGetRunPath()
{
	return INVENT::IEngine::Instatnce().GetRunPath();
}
