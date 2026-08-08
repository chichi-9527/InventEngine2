#pragma once

#include "IMemPool/IMemPool.h"

#include <string>
#include <vector>
#include <filesystem>

namespace INVENT
{
	class IEngineTools
	{
		IEngineTools() = default;
	public:
		~IEngineTools() = default;

		static bool ReadFile(const std::string& path, std::vector<char>& out);
		static const std::string& GetRunPath();
		static const std::filesystem::path& GetRunStdPath();

		static IEngineTools& Instance();

	private:

	};
}