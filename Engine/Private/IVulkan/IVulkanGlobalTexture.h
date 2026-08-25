#pragma once

#include "IBitArray.h"
#include "ITextureCompresser.h"

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
		struct CompressedTextureData;
		
	}


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

		struct IVulkanTexture2DHandle
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView ImageView = VK_NULL_HANDLE;
			VkFormat Format = VK_FORMAT_UNDEFINED;
			std::uint64_t Version{ 0 };
			std::uint32_t Width{ 0 };
			std::uint32_t Height{ 0 };
			std::uint32_t MipLevels{ 1 };

			IVulkanTexture2DHandle() = default;
			IVulkanTexture2DHandle(const std::pair<VkImage, VkImageView>& v)
				: Image(v.first)
				, ImageView(v.second)
			{}
			IVulkanTexture2DHandle(VkImage i, VkImageView iv, VkFormat f, std::uint64_t v, std::uint32_t w, std::uint32_t h, std::uint32_t m)
				: Image(i)
				, ImageView(iv)
				, Format(f)
				, Version(v)
				, Width(w)
				, Height(h)
				, MipLevels(m)
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

			struct ImageSize 
			{
				VkFormat Format = VK_FORMAT_UNDEFINED;
				std::uint32_t Width{ 0 };
				std::uint32_t Height{ 0 };
				std::uint32_t MipLevels{ 0 };
			};
			friend bool operator==(const IVulkanTexture2DHandle& handle,const ImageSize& isize) noexcept
			{
				return handle.Format == isize.Format &&
					handle.Width == isize.Width &&
					handle.Height == isize.Height &&
					handle.MipLevels == isize.MipLevels;
			}
			bool CanReused(const ImageSize& isize) const noexcept
			{
				return this->IsValid() && (*this) == isize;
			}

			bool IsValid() const noexcept
			{
				return Image != VK_NULL_HANDLE &&
					ImageView != VK_NULL_HANDLE;
			}
		};

	private:

		using TextureNameMap = std::unordered_map < std::string,
			Texture2DHandle,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, Texture2DHandle>>>;
		using TextureHandleNameMap = std::unordered_map<
			size_t,
			std::string,
			std::hash<size_t>,
			std::equal_to<size_t>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const size_t, std::string>>>;


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

		static constexpr Texture2DHandle GetWhitePixel() noexcept { return 0; }
		static constexpr Texture2DHandle GetBlackPixel() noexcept { return 1; }
		static constexpr Texture2DHandle GetNormalPixel() noexcept { return 2; }

		Texture2DHandle AllocateTextureHandle();
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined, bool is_create_mipmaps = true);
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name, const std::string& path, TextureType texture_type = TextureType::TYPE_Undefined, bool is_create_mipmaps = true);
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
		void _upload_bcn_texture(VkBuffer staging_buffer);
		bool _save_dds_texture(const std::string& path, const std::string& filename,
			const ITools::DDS_Header& header, const ITools::DDS_Header_DX10& header_dx10, const uint8_t* data, size_t data_size);
		/// <param name="dxgi"> in </param>
		/// <param name="fmt"> out </param>
		/// <param name="bytesPerBlock"> out </param>
		bool _dxgi_to_bcn(uint32_t dxgi, VkFormat& fmt, uint32_t& bytesPerBlock);
		bool _load_dds_to_compressed_data(const std::string& filepath, ITools::CompressedTextureData& out);

		const Texture2DHandle _find_handle_from_cache(const std::string& name) const;
		

#if 1
		void _test();
#endif // 1


	private:
		std::vector<IVulkanTexture2DHandle> _textures;
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
