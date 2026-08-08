#include "IEngine.h"

#include "IEngineTools.h"

namespace INVENT
{

	IEngine& IEngine::Instatnce()
	{
		static IEngine e;
		return e;
	}

	const char* IEngine::GetRunPath() const noexcept
	{
		return IEngineTools::GetRunPath().c_str();
	}


}

const char* INVENT_DLL InventEngineGetRunPath()
{
	return INVENT::IEngine::Instatnce().GetRunPath();
}
