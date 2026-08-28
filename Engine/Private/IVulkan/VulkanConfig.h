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
	constexpr std::uint32_t MAX_BINDLESS_TEXTURES = 100000U;	// 最大纹理数量 100,000 
	constexpr std::uint32_t DEF_BINDLESS_TEXTURES = 10000U;		// 默认纹理数量 10,000
	constexpr std::uint32_t MAX_ALLOCATED_SETS = MAX_FRAMES_IN_FLIGHT;
	constexpr std::uint32_t EXPECTED_INSTANCE_SIZE = 1 * 1024 * 1024; // 预期的实例数量 1M (个) 
	constexpr std::uint32_t EXPECTED_UBO_SIZE = 128U;				// 预期的ubo大小 128 (B)
	constexpr std::uint32_t MAX_POINT_LIGHT_COUNT = 100U;
	constexpr std::uint32_t DEF_ONE_BUFFER_SIZE = 32 * 1024 * 1024; // 默认管理类中单个 VkBuffer 大小  32 (MB)
	constexpr std::uint64_t CREATE_VKIMAGE_LIMIT = 1 * 1024 * 1024; // 当创建小于 1(MB) 字节的 VkImage 时用 vmaPool,否则強制开启 Dedicated 独立分配
	constexpr double MAX_TEXTURE_BUDGET_RATIO = 0.50; // "纹理"占可用显存最大比例 50%
	constexpr std::uint64_t ABSOLUTE_SAFETY_MARGIN = 64 * 1024 * 1024; // 64(MB) 全域余量
	constexpr std::uint64_t DEF_IMAGE_POOL_BLOCK_SIZE = 64 * 1024 * 1024; // 默认 vmaPool 块大小 64(MB)
	constexpr std::uint32_t MAX_IMAGE_POOL_BLOCK_COUNT = 5; // 默认 vmaPool 块数最多 5(个)

	inline VkDeviceSize TotalVRAM{ 0 };
	// 计算出的各种 buffer 的总大小(B) ,总显存(TotalVRAM)的 70%
	inline VkDeviceSize MaxBufferSize{ 0 };
	// 单个 SSBO 描述符绑定的最大字节数
	inline std::uint32_t MaxSSBORange{ 0 };
	// 单个 UBO 描述符绑定的最大字节数
	inline std::uint32_t MaxUBORange{ 0 };

	struct UBO 
	{
		glm::mat4 VP{ 1.0f };
	};

	struct PointLight
	{
		VkDrawIndexedIndirectCommand command{};
		
	};

	struct PushConstants 
	{
		std::uint32_t DrawObjectIndexOffset{ 0 };
	};

	/*typedef struct VkDrawIndexedIndirectCommand {
		uint32_t    indexCount;
		uint32_t    instanceCount;
		uint32_t    firstIndex;
		int32_t     vertexOffset;
		uint32_t    firstInstance;
	} VkDrawIndexedIndirectCommand;*/

}