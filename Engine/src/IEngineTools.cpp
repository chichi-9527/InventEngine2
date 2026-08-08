#include "IEngineTools.h"

#include "ILog.h"

#include <fstream>

namespace INVENT
{
	static auto RunPath = std::filesystem::current_path().string();
	static auto RunStdPath = std::filesystem::current_path();


	bool IEngineTools::ReadFile(const std::string& path, std::vector<char>& out)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			INVENT_LOG_ERROR(std::format("[IEngineTools] failed to open file : {}.", path));
			return false;
		}

		size_t fileSize = (size_t)file.tellg();
		out.resize(fileSize + 1);
		file.seekg(0);
		file.read(out.data(), fileSize);
		file.close();
		out[fileSize] = '\0';
		return true;
	}

	const std::string& IEngineTools::GetRunPath()
	{
		return RunPath;
	}

	const std::filesystem::path& IEngineTools::GetRunStdPath()
	{
		return RunStdPath;
	}

	IEngineTools& IEngineTools::Instance()
	{
		static IEngineTools t;
		return t;
	}

}