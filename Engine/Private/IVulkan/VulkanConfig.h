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
	constexpr std::uint32_t EXPECTED_SSBO_SIZE = 128 * 1024 * 1024; // 预期的ssbo大小 单位字节 128MB
	constexpr std::uint32_t EXPECTED_UBO_SIZE = 128U;				// 预期的ubo大小 单位字节 128B
	constexpr std::uint32_t MAX_POINT_LIGHT_COUNT = 100U;
	constexpr std::uint32_t DEF_ONE_BUFFER_SIZE = 32 * 1024 * 1024; // 默认管理类中单个 VkBuffer 大小 单位字节 32MB

	static VkDeviceSize TotalVRAM{ 0 };
	// 计算出的各种 buffer 的总大小(B) ,总显存(TotalVRAM)的 70%
	static VkDeviceSize MaxBufferSize{ 0 };
	// 单个 SSBO 描述符绑定的最大字节数
	static std::uint32_t MaxSSBORange{ 0 };
	// 单个 UBO 描述符绑定的最大字节数
	static std::uint32_t MaxUBORange{ 0 };

	struct UBO 
	{
		glm::mat4 VP{ 1.0f };
	};

	struct PointLight
	{

	};

	struct InstanceData 
	{
		glm::mat4 Model{ 1.0f };
		VkDeviceAddress VertexAddress{ 0 };		// 顶点起始指针
		VkDeviceAddress MaterualAddress{ 0 };	// 材质起始指针
		VkDeviceAddress IndexAddress{ 0 };		// 顶点索引起始指针
		VkDeviceAddress BoneBufferAddress{ 0 };	// 骨骼起始指针
		std::uint32_t BoneCount{ 0 };
		std::uint32_t ObjectID{ 0 };
		std::uint64_t padding{ 0 };
	};

	struct PushConstants 
	{
		std::uint32_t DrawObjectIndexOffset{ 0 };
	};

}