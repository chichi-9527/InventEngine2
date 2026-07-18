#pragma once

#include <string>

namespace INVENT
{
	class IEngineTools
	{
		IEngineTools() = default;
	public:
		~IEngineTools() = default;

		static const std::string& GetRunPath();

		static IEngineTools& Instance();

	private:

	};
}