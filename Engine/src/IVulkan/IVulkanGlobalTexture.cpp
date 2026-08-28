#include "IVulkan/IVulkanGlobalTexture.h"

#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"
#include "IThread/IThreadPool.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/ITieredImageMemoryManager.h"
#include "Memory/Memory.h"

#include <StbImage/stb_image.h>
#include <StbImage/stb_image_resize2.h>
#include <StbImage/stb_image_write.h>
#include <ispc_texcomp.h>

#include <stdexcept>
#include <fstream>
#include <istream>
#include <new>

namespace INVENT
{
	using Uint8Vector = std::vector<uint8_t, IMemPoolAllocatorOnlyFixedBlock<uint8_t>>;

	namespace ITools
	{
#pragma pack(push, 1)
		struct DDS_Header {
			uint32_t dwMagic = 0x20534444; // "DDS "
			uint32_t dwSize = 124;
			uint32_t dwFlags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000; // CAPS, HEIGHT, WIDTH, PIXELFORMAT, MIPMAPCOUNT
			uint32_t dwHeight;
			uint32_t dwWidth;
			uint32_t dwPitchOrLinearSize;
			uint32_t dwDepth = 0;
			uint32_t dwMipMapCount;
			uint32_t dwReserved1[11] = { 0 };
			// Pixel Format
			uint32_t pfSize = 32;
			uint32_t pfFlags = 0x4; // FOURCC
			uint32_t pfFourCC = 0x30315844; // "DX10" (代表後面有 DX10 擴展檔頭)
			uint32_t pfRGBBitCount = 0;
			uint32_t pfRBitMask = 0;
			uint32_t pfGBitMask = 0;
			uint32_t pfBBitMask = 0;
			uint32_t pfABitMask = 0;
			// Caps
			uint32_t dwCaps = 0x1000 | 0x400008; // COMPLEX, TEXTURE, MIPMAP
			uint32_t dwCaps2 = 0;
			uint32_t dwCaps3 = 0;
			uint32_t dwCaps4 = 0;
			uint32_t dwReserved2 = 0;
		};
		struct DDS_Header_DX10 {
			uint32_t dxgiFormat = 98; // DXGI_FORMAT_BC7_UNORM
			uint32_t resourceDimension = 3; // TEXTURE2D
			uint32_t miscFlag = 0;
			uint32_t arraySize = 1;
			uint32_t miscFlags2 = 0;
		};
#pragma pack(pop)
		static_assert(sizeof(DDS_Header) == 128);
		static_assert(sizeof(DDS_Header_DX10) == 20);

		struct CompressedTextureData
		{
			VkFormat            format;     // VK_FORMAT_BC1_RGB_SRGB_BLOCK ...
			uint32_t            width;      // 逻辑尺寸（base 已 4 对齐）
			uint32_t            height;
			uint32_t            mipLevels;
			Uint8Vector			blocks;    // 所有 mip 连续排列，与 DDS 文件体字节级一致
		};
		
	}

	IVulkanTexture2DManagement& IVulkanTexture2DManagement::Instance()
	{
		static IVulkanTexture2DManagement m;
		return m;
	}

	bool IVulkanTexture2DManagement::Init()
	{
		if (_is_valid) return false;

		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
		INVENT_LOG_INFO(std::format("[IVulkanTexture2DManagement] current bindless descriptor count: {}.", textureCount));

		_textures.resize(textureCount, IVulkanTexture2DHandle());
		_bit_vector_used.ResizeBitCount(textureCount);
		_bit_vector_valid.ResizeBitCount(textureCount);

		_init_other();
		_init_default_image();
		if (!ITieredImageMemoryManager::Init()) return false;

#if 1
		_test();
#endif // 1

		_is_valid = true;
		return true;
	}

	void IVulkanTexture2DManagement::Clear()
	{
		_bit_vector_used.FastForEachOne([this](size_t index) {

			auto& tex = _textures[index];
			if (tex.ImageView != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().DestroyImageView(tex.ImageView);
			}
			if (tex.Image != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyImage(tex.Image);
			}

			});
		_texture_name_cache->clear();
		_texture_handle_name_cache->clear();
		_bit_vector_used.ResetBitToZero();
		_bit_vector_valid.ResetBitToZero();
		_is_valid = false;
	}

	void IVulkanTexture2DManagement::Terminate()
	{
		if (_texture_name_cache)
		{
			delete _texture_name_cache;
			_texture_name_cache = nullptr;
		}
		if (_texture_handle_name_cache)
		{
			delete _texture_handle_name_cache;
			_texture_handle_name_cache = nullptr;
		}
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AllocateTextureHandle()
	{
		Texture2DHandle handle;
		handle.handle = _bit_vector_used.FindFirstZero();
		if (!handle.handle.IsValid())
		{
			if (!IVulkanBase::Base().ResizeBindlessDescriptorPoolAndGobalSet())
			{
				return Texture2DHandle();
			}

			_update_texture_count();
			handle.handle = _bit_vector_used.FindFirstZero();
		}

		//auto index = handle.GetRealIndex();

		_bit_vector_used.SetValue<true>(handle.handle);

		/*
		_bit_vector_vaild.SetValue<false>(handle.BitSetIndex, handle.BitIndex);
		_textures[index] = IVulkanTexture2DHandle{ VK_NULL_HANDLE, VK_NULL_HANDLE };
		*/

		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& path,
		TextureType texture_type,
		bool is_create_mipmaps)
	{

		auto startcount = path.find_last_of("/\\") + 1;
		auto lastcount = path.find_last_of('.');
		std::string name = path.substr(startcount, lastcount - startcount);
		if (name.empty())
		{
			INVENT_LOG_WARNING(std::format("name is empty; path : {}", path));
			name = "Empty";
		}

		return AddTexture2D(name, path, texture_type, is_create_mipmaps);
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& name,
		const std::string& path,
		TextureType texture_type,
		bool is_create_mipmaps)
	{
		Texture2DHandle handle = _find_handle_from_cache(name);
		if (handle.IsValid())
			return handle;
		handle = AllocateTextureHandle();
		if (!handle.IsValid())
		{
			INVENT_LOG_ERROR("纹理数量已达到上限.");
			return handle;
		}

		IEngineTools::Instance().GetWorkThreadPool()->Submit(0, [this, handle, path, texture_type, is_create_mipmaps]() {
			int width = 0, height = 0, channels = 0;
			auto texData = stbi_load(path.c_str(), &width, &height, &channels, 4);

			if (!texData)
			{
				throw std::runtime_error(std::format("failed to load texture image! path : {}", path));
			}
			uint32_t levelCount = is_create_mipmaps ? IEngineTools::CalculateMipLevels(width, height) : 1;
			VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;
			if (texture_type == TextureType::TYPE_Data)
			{
				textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
			}

			VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

			VkBuffer stagingBuffer;
			void* data;
			if (VkResult res = IVulkanBase::Base().UseVmaCreateBuffer(imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				stagingBuffer,
				&data))
			{
				stbi_image_free(texData);
				throw std::runtime_error(std::format("failed to load staging buffer! path : {}", path));
			}

			memcpy(data, texData, static_cast<size_t>(imageSize));
			if (!IVulkanBase::Base().UseVmaFlushAllocationBuffer(stagingBuffer))
			{
				throw std::runtime_error("failed to flush buffer allocation!");
			}

			stbi_image_free(texData);

			//

			VkImage image = VK_NULL_HANDLE;
			if (VkResult res = IVulkanBase::Base().UseVmaCreateImage(width,
				height,
				levelCount,
				textureFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 生成 mipmap
				0,
				image))
			{
				throw std::runtime_error(std::format("failed to create image! path : {}", path));
			}

			_upload_texture_and_generate_mipmaps(stagingBuffer,
				image,
				textureFormat,
				width,
				height,
				levelCount);

			IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);

			VkImageView imageView = IVulkanBase::Base().CreateImageView(image,
				textureFormat,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_VIEW_TYPE_2D,
				levelCount);
			if (imageView == VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyImage(image);
				throw std::runtime_error(std::format("failed to create texture image view! path : {}", path));
			}

			IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), imageView);
			{
				std::unique_lock lock(_textures_mutex);
				_textures[handle.handle.GetRealIndex()] = { image,imageView,textureFormat,std::uint64_t{0},static_cast<uint32_t>(width),static_cast<uint32_t>(height),levelCount };
			}
			_bit_vector_valid.SetValue<true>(handle.handle);
			});

		_insert_name_cache(name, handle);
		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& name,
		VkImage image, VkImageView image_view,
		std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels,
		VkFormat format)
	{
		Texture2DHandle handle = _find_handle_from_cache(name);
		if (handle.IsValid())
			return handle;
		handle = AllocateTextureHandle();
		if (!handle.IsValid())
		{
			INVENT_LOG_ERROR("纹理数量已达到上限.");
			return handle;
		}

		IEngineTools::Instance().GetWorkThreadPool()->Submit(0, [this, handle, image, image_view, width, height, mip_levels, format]() {
			IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), image_view);
			{
				std::unique_lock lock(_textures_mutex);
				_textures[handle.handle.GetRealIndex()] = { image,image_view,format,std::uint64_t{0},std::uint32_t{width},std::uint32_t{height},mip_levels };
			}
			_bit_vector_valid.SetValue<true>(handle.handle);
			});

		_insert_name_cache(name, handle);
		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::UpdateTexture2D(Texture2DHandle& handle,
		const std::string& path, TextureType texture_type,
		bool is_create_mipmaps)
	{
		if (!handle.IsValid()) return Texture2DHandle{};

		
		size_t index = handle.handle.GetRealIndex();

		int width = 0, height = 0, channels = 0;
		auto texData = stbi_load(path.c_str(), &width, &height, &channels, 4);

		if (!texData)
		{
			throw std::runtime_error(std::format("failed to load texture image! path : {}", path));
		}
		uint32_t newWidth = static_cast<uint32_t>(width);
		uint32_t newHeight = static_cast<uint32_t>(height);
		uint32_t newMipLevels = is_create_mipmaps ? IEngineTools::CalculateMipLevels(newWidth, newHeight) : 1;
		VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;
		if (texture_type == TextureType::TYPE_Data)
		{
			textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
		}

		{
			std::unique_lock lock(_textures_mutex);
			auto& slot = _textures[index];
			bool reusable = slot.CanReused({ textureFormat, newWidth, newHeight, newMipLevels });
			VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

			VkBuffer stagingBuffer;
			void* data;
			if (VkResult res = IVulkanBase::Base().UseVmaCreateBuffer(imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				stagingBuffer,
				&data))
			{
				stbi_image_free(texData);
				throw std::runtime_error(std::format("failed to load staging buffer! path : {}", path));
			}

			memcpy(data, texData, static_cast<size_t>(imageSize));
			if (!IVulkanBase::Base().UseVmaFlushAllocationBuffer(stagingBuffer))
			{
				throw std::runtime_error("failed to flush buffer allocation!");
			}

			stbi_image_free(texData);

			if (reusable)
			{
				_upload_texture_and_generate_mipmaps(
					stagingBuffer,
					slot.Image,
					textureFormat,
					newWidth,
					newHeight,
					newMipLevels,
					0,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);
				++slot;
				handle.version = slot.Version;
				slot.MipLevels = newMipLevels;
			}
			else
			{
				VkImage newImage = VK_NULL_HANDLE;
				if (VkResult res = IVulkanBase::Base().UseVmaCreateImage(width,
					height,
					newMipLevels,
					textureFormat,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 生成 mipmap
					0,
					newImage))
				{
					IVulkanBase::Base().UseVmaDestroyImage(newImage);
					throw std::runtime_error(std::format("failed to create image! path : {}", path));
				}

				_upload_texture_and_generate_mipmaps(stagingBuffer,
					newImage,
					textureFormat,
					width,
					height,
					newMipLevels);

				IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);

				VkImageView newImageView = IVulkanBase::Base().CreateImageView(newImage,
					textureFormat,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_VIEW_TYPE_2D,
					newMipLevels);
				if (newImageView == VK_NULL_HANDLE)
				{
					throw std::runtime_error(std::format("failed to create texture image view! path : {}", path));
				}
				// 保存旧资源，稍后销毁
				VkImage oldImage = slot.Image;
				VkImageView oldImageView = slot.ImageView;
				// 更新槽位为新的 image/view
				slot.Image = newImage;
				slot.ImageView = newImageView;
				slot.Width = newWidth;
				slot.Height = newHeight;
				slot.Format = textureFormat;
				slot.MipLevels = newMipLevels;
				IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(index), newImageView);
				++slot;
				handle.version = slot.Version;

				QueueDestroy(oldImage, oldImageView);
			}
		}
		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::UpdateTexture2D(Texture2DHandle & handle,
		VkImage image, VkImageView image_view,
		std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels,
		VkFormat format)
	{
		if (!handle.IsValid()) return Texture2DHandle{};
		auto& slot = _textures[handle.handle.GetRealIndex()];
		VkImage oldImage = slot.Image;
		VkImageView oldImageView = slot.ImageView;
		slot.Image = image;
		slot.ImageView = image_view;
		slot.Width = width;
		slot.Height = height;
		slot.Format = format;
		IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), image_view);
		++slot; // ++slot.Version
		handle.version = slot.Version;
		_bit_vector_valid.SetValue<true>(handle.handle);

		QueueDestroy(oldImage, oldImageView);

		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::UpdateTexture2DWithoutDestory(Texture2DHandle & handle,
		VkImage image, VkImageView image_view,
		std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels,
		VkFormat format)
	{
		if (!handle.IsValid()) return Texture2DHandle{};
		{
			std::unique_lock lock(_textures_mutex);
			auto& slot = _textures[handle.handle.GetRealIndex()];
			VkImage oldImage = slot.Image;
			VkImageView oldImageView = slot.ImageView;
			slot.Image = image;
			slot.ImageView = image_view;
			slot.Width = width;
			slot.Height = height;
			slot.Format = format;
			IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), image_view);
			++slot; // ++slot.Version
			handle.version = slot.Version;
		}
		_bit_vector_valid.SetValue<true>(handle.handle);

		return handle;
	}

	void IVulkanTexture2DManagement::DestroyTexture2D(Texture2DHandle& handle)
	{
		if (!handle.IsValid()) return;
		const size_t index = handle.handle.GetRealIndex();
		{
			std::unique_lock<std::shared_mutex> texLock(_textures_mutex);
			auto& slot = _textures[index];
			// 1. 销毁 Vulkan 资源
			QueueDestroy(slot.Image, slot.ImageView);
			// 2. 清空槽位
			slot = IVulkanTexture2DHandle{};
		}
		// 
		_bit_vector_valid.SetValue<false>(handle.handle);
		_bit_vector_used.SetValue<false>(handle.handle);
		//
		_remove_name_cache_by_handle(handle);
		handle = {};
	}

	bool IVulkanTexture2DManagement::IsTextureReady(const Texture2DHandle & handle) const
	{
		if (!handle.IsValid()) return false;
		return _bit_vector_valid[handle.handle.BitSetIndex][handle.handle.BitIndex];
	}

	IVulkanTexture2DManagement::IVulkanTexture2DHandle 
		IVulkanTexture2DManagement::GetVulkanTextureHandle(const Texture2DHandle& handle) const
	{
		if (!handle.IsValid()) throw std::runtime_error("[GetVulkanTextureHandle] handle is not valid!");
		size_t index = handle.handle.GetRealIndex();
		std::shared_lock lock(_textures_mutex);
		return _textures[index];
	}

	void IVulkanTexture2DManagement::QueueDestroy(VkImage image, VkImageView image_view)
	{
		if (image == VK_NULL_HANDLE && image_view == VK_NULL_HANDLE)
			return;
		std::lock_guard<std::mutex> lock(_destroy_mutex);
		_pending_destroy.push_back({ image, image_view });
	}

	void IVulkanTexture2DManagement::FlushDestroyQueue()
	{
		std::vector<PendingDestroy> items;
		{
			std::lock_guard<std::mutex> lock(_destroy_mutex);
			items.swap(_pending_destroy);
		}
		for (auto& item : items)
		{
			if (item.ImageView != VK_NULL_HANDLE)
				IVulkanBase::Base().DestroyImageView(item.ImageView);
			if (item.Image != VK_NULL_HANDLE)
				IVulkanBase::Base().UseVmaDestroyImage(item.Image);
		}
	}

	void IVulkanTexture2DManagement::_init_default_image()
	{
		_bit_vector_used.SetValue<true>(IHandle(0));
		_bit_vector_used.SetValue<true>(IHandle(1));
		_bit_vector_used.SetValue<true>(IHandle(2));
		_insert_name_cache("DefaultWhitePixel", 0 );
		_insert_name_cache("DefaultBlackPixel", 1);
		_insert_name_cache("DefaultNormalPixel", 2);

		uint32_t whitePixel = 0xFFFFFFFF;
		uint32_t blackPixel = 0xFF000000;
		uint32_t normalPixel = 0xFFFF8080;

		constexpr VkDeviceSize DefaultSingleImageSize = static_cast<VkDeviceSize>(1) * 1 * 4;
		constexpr VkDeviceSize stagingBufferSize = DefaultSingleImageSize * 3;

		VkBuffer stagingBuffer;
		void* data;
		if (VkResult res = IVulkanBase::Base().UseVmaCreateBuffer(stagingBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			stagingBuffer,
			&data))
		{
			throw std::runtime_error("failed to load staging buffer! : _init_default_image");
		}

		uint32_t pixels[] = { whitePixel,blackPixel,normalPixel };
		memcpy(data, pixels, static_cast<size_t>(stagingBufferSize));
		if (!IVulkanBase::Base().UseVmaFlushAllocationBuffer(stagingBuffer))
		{
			throw std::runtime_error("failed to flush buffer allocation!");
		}

		// white
		VkImage whiteImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			whiteImage);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			whiteImage,
			VK_FORMAT_R8G8B8A8_SRGB,
			1,
			1,
			1,
			0);
		VkImageView whiteImageView = IVulkanBase::Base().CreateImageView(whiteImage,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (whiteImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		IVulkanBase::Base().UpdateBindlessTextureSlot(0, whiteImageView);
		_textures[0] = { whiteImage, whiteImageView, VK_FORMAT_R8G8B8A8_SRGB, 0, 1, 1, 1 };
		_bit_vector_valid.SetValue<true>(IHandle(0));

		// black
		VkImage blackImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			blackImage);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			blackImage,
			VK_FORMAT_R8G8B8A8_SRGB,
			1,
			1,
			1,
			DefaultSingleImageSize);
		VkImageView blackImageView = IVulkanBase::Base().CreateImageView(blackImage,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (blackImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		IVulkanBase::Base().UpdateBindlessTextureSlot(1, blackImageView);
		_textures[1] = { blackImage, blackImageView, VK_FORMAT_R8G8B8A8_SRGB, 0, 1, 1, 1 };
		_bit_vector_valid.SetValue<true>(IHandle(1));

		// normal
		VkImage normalImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			normalImage);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			normalImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			1,
			1,
			1,
			DefaultSingleImageSize * 2);
		VkImageView normalImageView = IVulkanBase::Base().CreateImageView(normalImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (normalImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		IVulkanBase::Base().UpdateBindlessTextureSlot(2, normalImageView);
		_textures[2] = { normalImage, normalImageView, VK_FORMAT_R8G8B8A8_UNORM, 0, 1, 1, 1 };
		_bit_vector_valid.SetValue<true>(IHandle(2));

		IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);
	}

	void IVulkanTexture2DManagement::_init_other()
	{
		_texture_name_cache = new TextureNameMap(64,
			std::hash<std::string>(),
			std::equal_to<std::string>(),
			IMemPoolAllocatorOnlyFixedBlock< std::pair<const std::string, Texture2DHandle>>(IEngineTools::Instance().GetMemPoolPool()));
		_texture_handle_name_cache = new TextureHandleNameMap(64,
			std::hash<size_t>(),
			std::equal_to<size_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const size_t, std::string>>(IEngineTools::Instance().GetMemPoolPool()));
	}

	void IVulkanTexture2DManagement::_insert_name_cache(const std::string& name, Texture2DHandle handle)
	{
		std::unique_lock<std::shared_mutex> lock(_cache_mutex);
		_texture_name_cache->insert({ name, handle });
		_texture_handle_name_cache->insert({ handle.handle.GetRealIndex(), name });
	}

	void IVulkanTexture2DManagement::_remove_name_cache_by_handle(const Texture2DHandle& handle)
	{
		if (!handle.IsValid())
			return;
		std::unique_lock<std::shared_mutex> lock(_cache_mutex);
		auto it = _texture_handle_name_cache->find(handle.handle.GetRealIndex());
		if (it != _texture_handle_name_cache->end())
		{
			_texture_name_cache->erase(it->second);   // 删除正向表项
			_texture_handle_name_cache->erase(it);    // 删除反向表项
		}
	}

	void IVulkanTexture2DManagement::_update_texture_count()
	{
		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
		INVENT_LOG_INFO(std::format("[IVulkanTexture2DManagement] updated! current bindless descriptor count: {}.", textureCount));
		_bit_vector_used.ResizeBitCount(textureCount);
		_bit_vector_valid.ResizeBitCount(textureCount);
		_textures.resize(textureCount, IVulkanTexture2DHandle());
	}

	void IVulkanTexture2DManagement::_transition_image_layout(VkCommandBuffer commond_buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t base_mip_level, uint32_t level_count)
	{
		VkAccessFlags srcAccess = 0;
		VkAccessFlags dstAccess = 0;
		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcAccess = 0;
			dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
			dstAccess = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstAccess = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcAccess = VK_ACCESS_SHADER_READ_BIT;
			dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}

		VkImageMemoryBarrier2 barrier2{};
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier2.oldLayout = old_layout;
		barrier2.newLayout = new_layout;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.image = image;
		barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level, level_count, 0, 1 };
		barrier2.srcStageMask = srcStage;
		barrier2.srcAccessMask = srcAccess;
		barrier2.dstStageMask = dstStage;
		barrier2.dstAccessMask = dstAccess;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &barrier2;

		vkCmdPipelineBarrier2(commond_buffer, &depInfo);

	}

	void IVulkanTexture2DManagement::_upload_texture_and_generate_mipmaps(VkBuffer staging_buffer,
		VkImage tex_image,
		VkFormat trans_format,
		uint32_t width,
		uint32_t height,
		uint32_t level_count,
		VkDeviceSize buffer_offset,
		VkImageLayout initial_layout)
	{
		auto cmd = IVulkanBase::Base().BeginSingleTimeCommands();

		_transition_image_layout(cmd,
			tex_image,
			trans_format,
			initial_layout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			level_count);

		VkBufferImageCopy region{};
		region.bufferOffset = buffer_offset;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;   // 目標是 Level 0
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd, staging_buffer, tex_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < level_count; ++i)
		{
			_transition_image_layout(cmd,
				tex_image,
				trans_format,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				i - 1,
				1);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd,
				tex_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				tex_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit, VK_FILTER_LINEAR);

			_transition_image_layout(cmd,
				tex_image,
				trans_format,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				i - 1,
				1);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		} // for end

		_transition_image_layout(cmd,
			tex_image,
			trans_format,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			level_count - 1,
			1);
		IVulkanBase::Base().EndSingleTimeCommands(cmd);
	}

	bool IVulkanTexture2DManagement::_save_dds_texture(const std::string& path, const std::string& filename,
		const ITools::DDS_Header& header, const ITools::DDS_Header_DX10& header_dx10, const uint8_t* data, size_t data_size)
	{
		auto _current_path = std::filesystem::path{ path } / (filename + ".dds");
		if (auto path = _current_path.parent_path(); !path.empty())
		{
			std::filesystem::create_directories(path);
		}
		std::ofstream out(_current_path, std::ios::out | std::ios::trunc | std::ios::binary);
		if (!out.is_open())
		{
			return false;
		}
		// 1. 寫入標準 DDS 檔頭
		out.write(reinterpret_cast<const char*>(&header), sizeof(ITools::DDS_Header));
		// 2. 寫入 DX10 擴展檔頭
		out.write(reinterpret_cast<const char*>(&header_dx10), sizeof(ITools::DDS_Header_DX10));
		// 3. 一次性刷入所有 Mipmaps 的壓縮區塊數據
		out.write(reinterpret_cast<const char*>(data), data_size);
		return true;
	}

	bool IVulkanTexture2DManagement::_dxgi_to_bcn(uint32_t dxgi, VkFormat& fmt, uint32_t& bytesPerBlock)
	{
		switch (dxgi)
		{
		case 71: fmt = VK_FORMAT_BC1_RGB_UNORM_BLOCK; bytesPerBlock = 8;  return true;
		case 72: fmt = VK_FORMAT_BC1_RGB_SRGB_BLOCK; bytesPerBlock = 8;  return true;
		case 77: fmt = VK_FORMAT_BC3_UNORM_BLOCK;    bytesPerBlock = 16; return true;
		case 78: fmt = VK_FORMAT_BC3_SRGB_BLOCK;     bytesPerBlock = 16; return true;
		case 80: fmt = VK_FORMAT_BC4_UNORM_BLOCK;    bytesPerBlock = 8;  return true;
		case 81: fmt = VK_FORMAT_BC4_SNORM_BLOCK;    bytesPerBlock = 8;  return true;
		case 83: fmt = VK_FORMAT_BC5_UNORM_BLOCK;    bytesPerBlock = 16; return true;
		case 84: fmt = VK_FORMAT_BC5_SNORM_BLOCK;    bytesPerBlock = 16; return true;
		case 95: fmt = VK_FORMAT_BC6H_UFLOAT_BLOCK;  bytesPerBlock = 16; return true;
		case 96: fmt = VK_FORMAT_BC6H_SFLOAT_BLOCK;  bytesPerBlock = 16; return true;
		case 98: fmt = VK_FORMAT_BC7_UNORM_BLOCK;    bytesPerBlock = 16; return true;
		case 99: fmt = VK_FORMAT_BC7_SRGB_BLOCK;     bytesPerBlock = 16; return true;
		}
		return false;
	}

	bool IVulkanTexture2DManagement::_load_dds_to_compressed_data(const std::string& filepath, ITools::CompressedTextureData& out)
	{
		std::ifstream in(filepath, std::ios::binary);
		if (!in) return false;

		ITools::DDS_Header hdr{};
		in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
		if (!in || hdr.dwMagic != 0x20534444 || hdr.dwSize != 124) return false;

		return true;
	}

	const IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::_find_handle_from_cache(const std::string& name) const
	{
		auto iter = _texture_name_cache->find(name);
		if (iter != _texture_name_cache->end())
		{
			return iter->second;
		}
		return Texture2DHandle();
	}


#if 1
	void IVulkanTexture2DManagement::_test()
	{
		int width1, height1, channels1;
		auto src_pixels1 = stbi_loadf("Config/EngineAssets/image/test.png", &width1, &height1, &channels1, 4);
		if (!src_pixels1)
		{
			INVENT_LOG_ERROR("Failed to load input image: 1");
			return;
		}
		
		auto pool = IEngineTools::Instance().GetMemPoolPool();
		ITextureCompresser::CompressedTextureData texdata{};
		//texdata.data = new ITextureCompresser::Uint8Vector{ IMemPoolAllocatorOnlyFixedBlock<uint8_t>(pool) };
		//texdata.offsets = new ITextureCompresser::Uint32Vector{ IMemPoolAllocatorOnlyFixedBlock<uint32_t>(pool) };
		void* dataptr = EngineAllocator::Allocate(sizeof(ITextureCompresser::Uint8Vector));
		texdata.data = ::new(dataptr) ITextureCompresser::Uint8Vector{ IMemPoolAllocatorOnlyFixedBlock<uint8_t>(pool) };
		void* offsetptr = EngineAllocator::Allocate(sizeof(ITextureCompresser::Uint32Vector));
		texdata.offsets = ::new(offsetptr) ITextureCompresser::Uint32Vector{ IMemPoolAllocatorOnlyFixedBlock<uint32_t>(pool) };
		ITextureCompresser::CompressTextureFRGBA(texdata, src_pixels1, { static_cast<uint32_t>(width1),static_cast<uint32_t>(height1) });

		// 準備 DDS 檔頭
		ITools::DDS_Header dds_hdr;
		dds_hdr.dwWidth = texdata.width;
		dds_hdr.dwHeight = texdata.height;
		dds_hdr.dwMipMapCount = texdata.mipLevels;
		// BC7 每個 4x4 區塊(16像素)佔 16 位元組，所以 LinearSize = 區塊數量 * 16
		// BC1/BC4 8
		// BC6H 16
		dds_hdr.dwPitchOrLinearSize = ((texdata.width + 3) / 4) * ((texdata.height + 3) / 4) * 16;
		ITools::DDS_Header_DX10 dds_dx10;
		dds_dx10.dxgiFormat = 95;
		
		INVENT_LOG_DEBUG("[texture test] Writing compressed DDS asset ...");
		if (!_save_dds_texture("Config/EngineAssets/image", "test.png", dds_hdr, dds_dx10, texdata.data->data(), texdata.data->size()))
		{
			INVENT_LOG_ERROR("[texture test] FAILED to save file.");
		}

		EngineAllocator::Deallocate(dataptr);
		EngineAllocator::Deallocate(offsetptr);

	}
#endif // 1
	

}

