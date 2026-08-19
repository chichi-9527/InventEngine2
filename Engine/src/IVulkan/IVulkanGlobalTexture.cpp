#include "IVulkan/IVulkanGlobalTexture.h"

#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"
#include "IThread/IThreadPool.h"
#include "IVulkan/VulkanBase.h"

#include <StbImage/stb_image.h>

#include <stdexcept>

namespace INVENT
{
	IVulkanTexture2DManagement::IVulkanTexture2DManagement()
	{
		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
		INVENT_LOG_INFO(std::format("[IVulkanTexture2DManagement] current bindless descriptor count: {}.", textureCount));

		_textures.resize(textureCount, IVulkanTexture2DHandle());
		_bit_vector_used.ResizeBitCount(textureCount);
		_bit_vector_valid.ResizeBitCount(textureCount);

		_init_other();
		_init_default_image();

		_is_valid = true;
	}

	IVulkanTexture2DManagement::~IVulkanTexture2DManagement()
	{
	}

	IVulkanTexture2DManagement& IVulkanTexture2DManagement::Instance()
	{
		static IVulkanTexture2DManagement m;
		return m;
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
			uint32_t levelCount = is_create_mipmaps ? _calculate_level_count(width, height) : 1;
			VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;
			if (texture_type == TextureType::TYPE_CalculateColor)
			{
				textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
			}

			VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

			VkBuffer stagingBuffer;
			if (!IVulkanBase::Base().UseVmaCreateBuffer(imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer))
			{
				stbi_image_free(texData);
				throw std::runtime_error(std::format("failed to load staging buffer! path : {}", path));
			}

			void* data;
			IVulkanBase::Base().UseVmaMapMemory(stagingBuffer, data);
			memcpy(data, texData, static_cast<size_t>(imageSize));
			IVulkanBase::Base().UseVmaUnmapMemory(stagingBuffer);

			stbi_image_free(texData);

			//

			VkImage image = VK_NULL_HANDLE;
			if (!IVulkanBase::Base().UseVmaCreateImage(width,
				height,
				levelCount,
				textureFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 生成 mipmap
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
		uint32_t newMipLevels = is_create_mipmaps ? _calculate_level_count(newWidth, newHeight) : 1;
		VkFormat textureFormat = VK_FORMAT_R8G8B8A8_SRGB;
		if (texture_type == TextureType::TYPE_CalculateColor)
		{
			textureFormat = VK_FORMAT_R8G8B8A8_UNORM;
		}

		{
			std::unique_lock lock(_textures_mutex);
			auto& slot = _textures[index];
			bool reusable = slot.CanReused({ textureFormat, newWidth, newHeight, newMipLevels });
			VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

			VkBuffer stagingBuffer;
			if (!IVulkanBase::Base().UseVmaCreateBuffer(imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer))
			{
				stbi_image_free(texData);
				throw std::runtime_error(std::format("failed to load staging buffer! path : {}", path));
			}

			void* data;
			IVulkanBase::Base().UseVmaMapMemory(stagingBuffer, data);
			memcpy(data, texData, static_cast<size_t>(imageSize));
			IVulkanBase::Base().UseVmaUnmapMemory(stagingBuffer);
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
				if (!IVulkanBase::Base().UseVmaCreateImage(width,
					height,
					newMipLevels,
					textureFormat,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 生成 mipmap
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
		if (!IVulkanBase::Base().UseVmaCreateBuffer(stagingBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer))
		{
			throw std::runtime_error("failed to load staging buffer! : _init_default_image");
		}

		void* data;
		IVulkanBase::Base().UseVmaMapMemory(stagingBuffer, data);
		uint32_t pixels[] = { whitePixel,blackPixel,normalPixel };
		memcpy(data, pixels, static_cast<size_t>(stagingBufferSize));
		IVulkanBase::Base().UseVmaUnmapMemory(stagingBuffer);

		// white
		VkImage whiteImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
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

	const IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::_find_handle_from_cache(const std::string& name) const
	{
		auto iter = _texture_name_cache->find(name);
		if (iter != _texture_name_cache->end())
		{
			return iter->second;
		}
		return Texture2DHandle();
	}

}

