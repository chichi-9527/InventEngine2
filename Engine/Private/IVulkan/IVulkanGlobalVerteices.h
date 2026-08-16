#pragma once

#include "VulkanConfig.h"

namespace INVENT
{
	struct Vertex
	{
		glm::vec3 Position{ 0.0f };
		std::uint32_t padding1{ 0 };

		glm::vec2 TexCoords{ 0.0f };
		std::uint32_t HasTexCoords{ 0 };
		std::uint32_t padding2{ 0 };

		glm::vec4 Color{ 1.0f };

		glm::vec3 Normal{ 0.0f };
		std::uint32_t HasNormal{ 0 };

		glm::vec3 Tangent{ 0.0f };			// 切线
		std::uint32_t HasTangent{ 0 };

		glm::vec3 Bitangent{ 0.0f };		// 双切线
		std::uint32_t HasBitangent{ 0 };
	};
}
