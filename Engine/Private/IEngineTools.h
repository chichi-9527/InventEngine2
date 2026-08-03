#pragma once

#include "IMemPool/IMemPool.h"

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