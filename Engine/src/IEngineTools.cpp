#include "IEngineTools.h"

#include <filesystem>


namespace INVENT
{
	static auto RunPath = std::filesystem::current_path().string();



	const std::string& IEngineTools::GetRunPath()
	{
		return RunPath;
	}

	IEngineTools& IEngineTools::Instance()
	{
		static IEngineTools t;
		return t;
	}

}