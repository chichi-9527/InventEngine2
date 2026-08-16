#pragma once

#include "IBitArray.h"

#include <cstdint>
#include <vulkan/vulkan.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif // !GLM_FORCE_RADIANS
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif // !GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace INVENT
{


	namespace IVulkan
	{
		constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2U;
		constexpr std::uint32_t MAX_BINDLESS_TEXTURES = 500000U;
		constexpr std::uint32_t DEF_BINDLESS_TEXTURES = 100000U;
		constexpr std::uint32_t MAX_ALLOCATED_SETS = MAX_FRAMES_IN_FLIGHT;

		struct PushConstants
		{
			VkDeviceAddress VertexAddress{ 0 };		// 顶点起始指针
			VkDeviceAddress MaterualAddress{ 0 };	// 材质起始指针
			VkDeviceAddress IndexAddress{ 0 };		// 顶点索引起始指针
			glm::mat4 MVP{ 1.0f };
		};

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

		struct Texture2DHandle
		{
			IHandle handle{};
			std::uint64_t version{ 0 };

			Texture2DHandle() = default;
			constexpr Texture2DHandle(size_t h)
				: handle(h)
			{}
			constexpr Texture2DHandle(size_t h, std::uint32_t v)
				: handle(h), version(v)
			{}
			Texture2DHandle(const Texture2DHandle&) = default;
			Texture2DHandle(Texture2DHandle&&) noexcept = default;

			Texture2DHandle& operator=(const Texture2DHandle&) = default;
			Texture2DHandle& operator=(Texture2DHandle&&) noexcept = default;

			friend bool operator==(const Texture2DHandle& handle1, const Texture2DHandle& handle2)
			{
				return handle1.handle == handle2.handle &&
					handle1.version == handle2.version;
			}

			bool IsValid() const noexcept { return handle.IsValid(); }
		};



	} // namespace IVulkan

} // namespcae INVENT
