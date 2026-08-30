#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <utility>
#include <cstdint>
#include <shared_mutex>


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

		/// <param name="out">输出参数：返回 VkBuffer 句柄,一般情况下每帧内相同</param>
		/// <param name="out_buffer_offset">输出参数：返回 VkBuffer 句柄中被分配的可用数据的偏移（VkDeviceSize）。</param>
		/// <param name="frame_index">当前帧索引。</param>
		/// <param name="buffer_size">请求的缓冲区大小（以字节为单位）。内部自动 16 字节对齐</param>
		/// <param name="out_mapped_data">映射到缓冲区内存的指针,此指针不是 VkBuffer 句柄的映射指针,而是偏移后的指针。</param>
		/// <returns>返回布尔值：若成功分配缓冲区则返回 true，若可分配内存不足则返回 false。</returns>
		static bool CreateStagingBuffer(VkBuffer& out, VkDeviceSize& out_buffer_offset, std::uint32_t frame_index, VkDeviceSize buffer_size, void** out_mapped_data);
		static void ResetStagingBuffer(std::uint32_t frame_index);
		

	private:
		static bool _is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size);

	private:
		inline static VkDeviceSize _current_total_texture_memory{ 0 };
		// 独立分配的显存使用记录
		inline static DedicatedImageSizeCache* _size_cache = nullptr;
		inline static std::shared_mutex _size_cache_mutex;

		inline static std::uint32_t _image_memory_type_index{ 0 };
	};
}
