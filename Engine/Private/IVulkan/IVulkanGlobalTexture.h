#pragma once

#include "IBitArray.h"
#include "ITextureCompresser.h"
#include "IVulkan/ITieredImageMemoryManager.h"

#include <cstdint>
#include <string>
#include <mutex>
#include <cmath>
#include <utility>
#include <shared_mutex>

#include <vulkan/vulkan.h>

namespace INVENT
{
	namespace ITools
	{
		struct DDS_Header;
		struct DDS_Header_DX10;
		
	}


	/*
	* SRGB:  擴散貼圖 (Diffuse Map) 基礎顏色貼圖 (Albedo Map) 反射貼圖 (Specular Map)
	* UNORM: 法線貼圖 (Normal Map) 粗糙度貼圖 (Roughness Map) 金屬度貼圖 (Metalic Map) 遮蔽貼圖 (AO Map)
	*/
	enum class TextureType : uint32_t
	{
		TYPE_Undefined = 0,
		TYPE_Color,				// diffuse / emission
		TYPE_Normal,			// normal BC5
		TYPE_SingleChannel,		// roughness / ao / opacity
		TYPE_Data				// specular / clear coat
	};
	enum class TextureCompressionType : uint32_t {
		NoCompression = 0,
		Auto,					// 根据 alpha 通道自动选择
		BC1_RGB_UNORM = 71,
		BC1_RGB_SRGB = 72,
		BC3_UNORM = 77,
		BC3_SRGB = 78,
		BC4_UNORM = 80,
		BC4_SNORM = 81,
		BC5_UNORM = 83,
		BC5_SNORM = 84,
		BC6H_UFLOAT = 95,
		BC6H_SFLOAT = 96,
		BC7_UNORM = 98,
		BC7_SRGB = 99
	};

	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class IVulkanTexture2DManagement
	{
	public:
		struct PendingDestroy
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView ImageView = VK_NULL_HANDLE;
		};

		enum DefaultTextureType : uint32_t 
		{
			White = 0,
			Black,
			NormalBlue,
			Transparent,
			DefaultCount
		};

		struct Texture2DHandle
		{
			std::uint32_t slot{ UINT32_MAX };

			bool IsValid() const noexcept { return slot != UINT32_MAX; }
		};

		struct IVulkanTexture2DHandle
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView ImageView = VK_NULL_HANDLE;
			std::uint32_t CurrentMaxMipLevel{ UINT32_MAX };

			bool IsValid() const noexcept
			{
				return CurrentMaxMipLevel != UINT32_MAX;
			}
		};

	private:

		enum class LodDDSType : uint32_t 
		{
			BC1,
			BC3,
			BC4,
			BC5,
			BC6H,
			BC7
		};

		using TextureNameMap = std::unordered_map < std::string,
			Texture2DHandle,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, Texture2DHandle>>>;
		using TextureHandleNameMap = std::unordered_map<
			std::uint32_t,
			std::string,
			std::hash<std::uint32_t>,
			std::equal_to<std::uint32_t>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::uint32_t, std::string>>>;


		IVulkanTexture2DManagement() = default;
	public:
		~IVulkanTexture2DManagement() = default;

		static IVulkanTexture2DManagement& Instance();
		bool Init();
		// 清除所有标识符，释放所有 VkImage/VkImageView
		void Clear();
		// 释放 CPU 内存，必须先调用 Clear 否则会造成 GPU 内存泄露
		// 在调用 IEngine::Shutdown() 之前调用
		void Terminate();


		Texture2DHandle AllocateTextureHandle();
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& path,
			TextureType texture_type = TextureType::TYPE_Undefined,
			TextureCompressionType compression_type = TextureCompressionType::Auto,
			std::uint32_t mip_levels = 0);
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name,
			const std::string& path,
			TextureType texture_type = TextureType::TYPE_Undefined,
			TextureCompressionType compression_type = TextureCompressionType::Auto,
			std::uint32_t mip_levels = 0);
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name,
			VkImage image, VkImageView image_view,
			std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels = 1,
			VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
		Texture2DHandle UpdateTexture2D(Texture2DHandle& handle, const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined, bool is_create_mipmaps = true);
		// 自动销毁 VkImage 与 VkImageView
		Texture2DHandle UpdateTexture2D(Texture2DHandle& handle,
			VkImage image, VkImageView image_view,
			std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels = 1,
			VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
		// 不自动销毁 VkImage 与 VkImageView
		Texture2DHandle UpdateTexture2DWithoutDestory(Texture2DHandle& handle,
			VkImage image, VkImageView image_view,
			std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels = 1,
			VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
		void DestroyTexture2D(Texture2DHandle& handle);

		bool IsTextureReady(const Texture2DHandle& handle) const;
		bool IsValid() const { return _is_valid; }

		IVulkanTexture2DHandle GetVulkanTextureHandle(const Texture2DHandle& handle) const;
		
		void QueueDestroy(VkImage image, VkImageView image_view);
		void FlushDestroyQueue();
		void UploadTextureAndGenerateMipmaps(VkBuffer staging_buffer,
			VkImage tex_image,
			VkFormat trans_format,
			uint32_t width,
			uint32_t height,
			uint32_t level_count = 1,
			VkDeviceSize buffer_offset = 0,
			VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED)
		{
			_upload_texture_and_generate_mipmaps(staging_buffer, tex_image, trans_format, width, height, level_count, buffer_offset, initial_layout);
		}
	private:
		void _init_default_image();
		void _init_other();
		void _insert_name_cache(const std::string& name, Texture2DHandle handle);
		void _remove_name_cache_by_handle(const Texture2DHandle& handle);
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
			VkDeviceSize buffer_offset = 0,
			VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED);

		bool _save_dds_texture(const std::string& path, const std::string& filename,
			const ITools::DDS_Header& header, const ITools::DDS_Header_DX10& header_dx10, const uint8_t* data, size_t data_size);

		bool _load_dds_to_compressed_data(const std::string& filepath, ITextureCompresser::CompressedTextureData& out, LodDDSType type);

		Texture2DHandle _find_handle_from_cache(const std::string& name) const;

#if 1
		void _test();
#endif // 1


	private:
		std::vector<IVulkanTexture2DHandle> _default_textures;
		std::vector<IVulkanTexture2DHandle> _textures;

		std::vector<ITextureCompresser::CompressedTextureData> _textures_data;


		IBitVectorSafe _bit_vector_used{};
		IBitVectorSafe _bit_vector_valid{};

		std::mutex _destroy_mutex;
		std::vector<PendingDestroy> _pending_destroy;

		mutable	std::shared_mutex _textures_mutex;
		mutable	std::shared_mutex _cache_mutex;

		TextureNameMap* _texture_name_cache = nullptr;
		TextureHandleNameMap* _texture_handle_name_cache = nullptr;

		bool _is_valid = false;

	};
}
