#pragma once

#include "IBitArray.h"

#include <cstdint>
#include <string>
#include <mutex>
#include <cmath>
#include <utility>

#include <vulkan/vulkan.h>

namespace INVENT
{
	enum class TextureType : uint32_t
	{
		TYPE_Undefined = 0,
		TYPE_WatchColor,
		TYPE_CalculateColor
	};

	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class IVulkanTexture2DManagement
	{
	public:
		struct Texture2DHandle
		{
			IHandle handle{};
			std::uint64_t version{ 0 };

			Texture2DHandle() = default;
			constexpr Texture2DHandle(size_t h)
				: handle(h){}
			constexpr Texture2DHandle(size_t h, std::uint32_t v)
				: handle(h), version(v) {}
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
	private:

		struct IVulkanTexture2DHandle
		{
			VkImageView ImageView = VK_NULL_HANDLE;
			VkImage Image = VK_NULL_HANDLE;
			std::uint64_t Version{ 0 };

			IVulkanTexture2DHandle() = default;
			IVulkanTexture2DHandle(const std::pair<VkImage, VkImageView>& v)
				: Image(v.first)
				, ImageView(v.second)
			{}
			IVulkanTexture2DHandle(const IVulkanTexture2DHandle&) = default;
			IVulkanTexture2DHandle(IVulkanTexture2DHandle&&) noexcept = default;
			IVulkanTexture2DHandle& operator=(const IVulkanTexture2DHandle&) = default;
			IVulkanTexture2DHandle& operator=(IVulkanTexture2DHandle&&) noexcept = default;
			IVulkanTexture2DHandle& operator=(const std::pair<VkImage, VkImageView>& v)
			{
				Image = v.first;
				ImageView = v.second;
				return *this;
			}
			IVulkanTexture2DHandle& operator++()
			{
				++Version;
				return *this;
			}
			IVulkanTexture2DHandle operator++(int)
			{
				IVulkanTexture2DHandle old = *this;
				++(*this);
				return old;
			}

			bool IsVaild() const noexcept
			{
				return Image != VK_NULL_HANDLE &&
					ImageView != VK_NULL_HANDLE;
			}
		};

		using TextureNameMap = std::unordered_map < std::string,
			Texture2DHandle,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, Texture2DHandle>>>;


		IVulkanTexture2DManagement();
	public:
		~IVulkanTexture2DManagement();

		static IVulkanTexture2DManagement& Instance();
		void Clear();

		static constexpr Texture2DHandle GetWhitePixel() noexcept { return 0; }
		static constexpr Texture2DHandle GetBlackPixel() noexcept { return 1; }
		static constexpr Texture2DHandle GetNormalPixel() noexcept { return 2; }

		Texture2DHandle AllocateTextureHandle();
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined);
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name, const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined);
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name, VkImage image, VkImageView image_view);
		void UpdateTexture2D(const Texture2DHandle& hanlde, const std::string& path);
		// 自动销毁 VkImage 与 VkImageView
		void UpdateTexture2D(const Texture2DHandle& hanlde, VkImage image, VkImageView image_view);
		// 不自动销毁 VkImage 与 VkImageView
		void UpdateTexture2DWithoutDestory(const Texture2DHandle& hanlde, VkImage image, VkImageView image_view);

		bool IsTextureReady(const Texture2DHandle& handle) const;
		bool IsValid() const { return _is_valid; }

		const IVulkanTexture2DHandle& GetVulkanTextureHanlde(const Texture2DHandle& handle) const;
		IVulkanTexture2DHandle& GetVulkanTextureHanlde(const Texture2DHandle& handle);

	private:
		void _init_default_image();
		void _init_other();
		void _update_texture_count();
		void _transition_image_layout(VkCommandBuffer commond_buffer,
			VkImage image,
			VkFormat format,
			VkImageLayout old_layout,
			VkImageLayout new_layout,
			uint32_t base_mip_level = 0,
			uint32_t level_count = 1);
		void _upload_texture_and_generate_mipmaps(VkBuffer staging_buffer,
			VkImage tex_image,
			VkFormat trans_format,
			uint32_t width,
			uint32_t height,
			uint32_t level_count = 1,
			VkDeviceSize buffer_offset = 0);

		const Texture2DHandle _find_handle_from_cache(const std::string& name) const;
		uint32_t _calculate_level_count(const int& width, const int& height) const
		{
			return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

	private:
		std::vector<IVulkanTexture2DHandle> _textures;
		IBitVector _bit_vector_used;
		IBitVector _bit_vector_valid;
		std::mutex _bits_vaild_mutex;

		TextureNameMap* _texture_name_cache = nullptr;

		bool _is_valid = false;

	};
}
