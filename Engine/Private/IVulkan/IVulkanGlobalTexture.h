#pragma once

#include "IBitArray.h"

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
		struct IVulkanTexture2DHandle
		{
			VkImageView ImageView = VK_NULL_HANDLE;
			VkImage Image = VK_NULL_HANDLE;

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

			bool IsVaild() const noexcept
			{
				return Image != VK_NULL_HANDLE &&
					ImageView != VK_NULL_HANDLE;
			}
		};

		using TextureNameMap = std::unordered_map < std::string,
			IHandle,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, IHandle>>>;


		IVulkanTexture2DManagement();
	public:
		~IVulkanTexture2DManagement();

		using Texture2DHandle = IHandle;

		static IVulkanTexture2DManagement& Instance();
		void Clear();

		static constexpr Texture2DHandle GetWhitePixel() noexcept { return 0; }
		static constexpr Texture2DHandle GetBlackPixel() noexcept { return 1; }
		static constexpr Texture2DHandle GetNormalPixel() noexcept { return 2; }

		Texture2DHandle AllocateTextureHandle();
		Texture2DHandle AddTexture2D(const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined);
		Texture2DHandle AddTexture2D(const std::string& name, const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined);
		Texture2DHandle AddTexture2D(const std::string& name, VkImage image, VkImageView image_view);
		void UpateTexture2D(const Texture2DHandle& hanlde, const std::string& path);
		// 自动销毁 VkImage 与 VkImageView
		void UpateTexture2D(const Texture2DHandle& hanlde, VkImage image, VkImageView image_view);
		// 不自动销毁 VkImage 与 VkImageView
		void UpateTexture2DWithoutDestory(const Texture2DHandle& hanlde, VkImage image, VkImageView image_view);

		bool IsTextureReady(const Texture2DHandle& handle) const;
		bool IsVaild() const { return _is_vaild; }

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

		const IHandle& _find_handle_from_cache(const std::string& name) const;
		uint32_t _calculate_level_count(const int& width, const int& height) const
		{
			return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

	private:
		std::vector<IVulkanTexture2DHandle> _textures;
		IBitVector _bit_vector_used;
		IBitVector _bit_vector_vaild;
		std::mutex _bits_vaild_mutex;

		TextureNameMap* _texture_name_cache = nullptr;

		bool _is_vaild = false;

	};
}
