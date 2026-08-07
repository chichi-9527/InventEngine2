#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif // !GLM_FORCE_RADIANS
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif // !GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace IVulkan
{
	constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2U;
	constexpr std::uint32_t MAX_BINDLESS_TEXTURES = 500000U;
	constexpr std::uint32_t DEF_BINDLESS_TEXTURES = 100000U;

	struct PushConstants 
	{
		VkDeviceAddress VertexAddress{}; // 顶点起始指针
		VkDeviceAddress MaterualAddress{}; // 材质起始指针
		glm::mat4 MVP;
	};

}