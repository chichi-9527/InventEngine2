#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <array>
#include <atomic>
#include <utility>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

namespace INVENT
{
	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class ITieredImageMemoryManager2
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
			VkFormat ImageFormat = VK_FORMAT_UNDEFINED;
			std::uint32_t ImageWidth{ 0 };
			std::uint32_t ImageHeight{ 0 };
			std::uint32_t MipLevels{ 1 };

			bool operator==(const ICreateImageInfo& other) const noexcept
			{
				return this->ImageFormat == other.ImageFormat &&
					this->ImageWidth == other.ImageWidth &&
					this->ImageHeight == other.ImageHeight &&
					this->MipLevels == other.MipLevels;
			}
		};

		static VkResult CreateVkImage(VkImage& out, const ICreateImageInfo& info);
		static void DestroyVkImage(VkImage image);

		/// <param name="pool_index">staging 环形槽位编号 (0 ~ UPLOAD_STAGING_COUNT-1), 语义是"传输轮次"而非帧号 </param>
		static bool CreateStagingBuffer(VkBuffer& out, VkDeviceSize& out_buffer_offset,
			std::uint32_t pool_index, VkDeviceSize buffer_size, void** out_mapped_data);
		static void ResetStagingBuffer(std::uint32_t pool_index);
		static std::uint32_t GetStagingPoolCount();
		static VkDeviceSize GetStagingBufferSize();

	private:
		static bool _is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size);
		// 新增: 按内存类型取/建图像池 (懒建)
		static VmaPool _get_or_create_image_pool(std::uint32_t memory_type_index);

	private:
		// 纹理显存总占用 (池块 + 独立分配); 上传线程与主线程都会写 -> atomic
		inline static std::atomic<VkDeviceSize> _current_total_texture_memory{ 0 };

		// 独立分配的显存使用记录
		inline static DedicatedImageSizeCache* _size_cache = nullptr;
		inline static std::shared_mutex _size_cache_mutex;

		// 按内存类型索引的图像池 (不同格式要求的 memoryTypeBits 可能不同)
		inline static std::unordered_map<std::uint32_t, VmaPool> _image_pools{};
		inline static std::shared_mutex _image_pools_mutex;
	};
}
