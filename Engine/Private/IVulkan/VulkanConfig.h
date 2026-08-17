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
	constexpr std::uint32_t MAX_ALLOCATED_SETS = MAX_FRAMES_IN_FLIGHT;

	static VkDeviceSize TotalVRAM{ 0 };
	// 计算出的各种 buffer 的总大小(B) ,总显存(TotalVRAM)的 70%
	static VkDeviceSize MaxBufferSize{ 0 };

	struct PushConstants 
	{
		VkDeviceAddress VertexAddress{ 0 };		// 顶点起始指针
		VkDeviceAddress MaterualAddress{ 0 };	// 材质起始指针
		VkDeviceAddress IndexAddress{ 0 };		// 顶点索引起始指针
		glm::mat4 MVP;
	};

}