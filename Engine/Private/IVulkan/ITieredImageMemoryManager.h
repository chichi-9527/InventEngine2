#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <utility>
#include <cstdint>

/*
* SRGB:  擴散貼圖 (Diffuse Map) 基礎顏色貼圖 (Albedo Map) 反射貼圖 (Specular Map)
* UNORM: 法線貼圖 (Normal Map) 粗糙度貼圖 (Roughness Map) 金屬度貼圖 (Metalic Map) 遮蔽貼圖 (AO Map)
*/

namespace INVENT
{
	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class ITieredImageMemoryManager
	{

		using DedicatedImageSizeCache = std::unordered_map<
			std::uint64_t,
			VkDeviceSize,
			std::hash<std::uint64_t>,
			std::equal_to<std::uint64_t>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::uint64_t, VkDeviceSize>>>;

	public:
		static bool Init();
		static void Clear();

		struct ICreateImageInfo
		{
			std::pair<std::uint32_t, std::uint32_t> ImageSize;
			VkFormat ImageFormat = VK_FORMAT_UNDEFINED;
			std::uint32_t MipLevels = 0;
		};

		VkResult CreateVkImage(VkImage& out, const ICreateImageInfo& info);
		void DestroyVkImage(VkImage image, const ICreateImageInfo& info);
		VkResult CreateStagingBuffer(VkBuffer& out, VkDeviceSize BufferSize, void** out_mapped_data = nullptr);

		


	private:
		bool _is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size);

	private:
		inline static VkDeviceSize _current_total_texture_memory{ 0 };
		// 独立分配的显存使用记录
		inline static DedicatedImageSizeCache* _size_cache = nullptr;
	};
}
