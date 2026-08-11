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

		_textures.resize(textureCount, IVulkanTexture2DHandle());
		_bit_vector_used.ResizeBitCount(textureCount);
		_bit_vector_valid.ResizeBitCount(textureCount);

		_init_other();
		_init_default_image();

		_is_valid = true;
	}

	IVulkanTexture2DManagement::~IVulkanTexture2DManagement()
	{
		Clear();
		if (_texture_name_cache)
		{
			delete _texture_name_cache;
			_texture_name_cache = nullptr;
		}
	}

	IVulkanTexture2DManagement& IVulkanTexture2DManagement::Instance()
	{
		static IVulkanTexture2DManagement m;
		return m;
	}

	void IVulkanTexture2DManagement::Clear()
	{
		_bit_vector_used.ForEach([this](size_t index, bool bit_value) {
			if (bit_value)
			{
				auto& tex = _textures[index];
				if (tex.ImageView != VK_NULL_HANDLE)
				{
					IVulkanBase::Base().DestroyImageView(tex.ImageView);
				}
				if (tex.Image != VK_NULL_HANDLE)
				{
					IVulkanBase::Base().UseVmaDestroyImage(tex.Image);
				}
			}
			});
		_bit_vector_used.ResetBitToZero();
		_bit_vector_valid.ResetBitToZero();
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

		_bit_vector_used.SetValue<true>(handle.handle.BitSetIndex, handle.handle.BitIndex);

		/*
		_bit_vector_vaild.SetValue<false>(handle.BitSetIndex, handle.BitIndex);
		_textures[index] = IVulkanTexture2DHandle{ VK_NULL_HANDLE, VK_NULL_HANDLE };
		*/

		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& path, TextureType texture_type)
	{

		auto startcount = path.find_last_of("/\\") + 1;
		auto lastcount = path.find_last_of('.');
		std::string name = path.substr(startcount, lastcount - startcount);
		if (name.empty())
		{
			INVENT_LOG_WARNING(std::format("name is empty; path : {}", path));
			name = "Empty";
		}

		return AddTexture2D(name, path, texture_type);
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& name, const std::string& path, TextureType texture_type)
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

		IEngineTools::Instance().GetWorkThreadPool()->Submit(0, [this, handle, path, texture_type]() {
			int width = 0, height = 0, channels = 0;
			auto texData = stbi_load(path.c_str(), &width, &height, &channels, 4);

			if (!texData)
			{
				throw std::runtime_error(std::format("failed to load texture image! path : {}", path));
			}
			auto levelCount = _calculate_level_count(width, height);
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
				throw std::runtime_error(std::format("failed to load staging buffer! path : {}", path));
			}

			void* data;
			IVulkanBase::Base().UseVmaMapMemory(stagingBuffer, data);
			memcpy(data, texData, static_cast<size_t>(imageSize));
			IVulkanBase::Base().UseVmaUnmapMemory(stagingBuffer);

			stbi_image_free(texData);

			//

			VkImage image = VK_NULL_HANDLE;
			IVulkanBase::Base().UseVmaCreateImage(width,
				height,
				levelCount,
				textureFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 生成 mipmap
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				image);

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
				throw std::runtime_error(std::format("failed to create texture image view! path : {}", path));
			}

			IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), imageView);
			_textures[handle.handle.GetRealIndex()] = { image,imageView };
			_bit_vector_valid.SetValue<true>(handle.handle);
			});

		_texture_name_cache->insert({ name,handle });
		return handle;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& name, VkImage image, VkImageView image_view)
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

		IEngineTools::Instance().GetWorkThreadPool()->Submit(0, [this, handle, image, image_view]() {
			IVulkanBase::Base().UpdateBindlessTextureSlot(static_cast<uint32_t>(handle.handle.GetRealIndex()), image_view);
			_textures[handle.handle.GetRealIndex()] = { image,image_view };
			_bit_vector_valid.SetValue<true>(handle.handle);
			});

		_texture_name_cache->insert({ name,handle });
		return handle;
	}

	void IVulkanTexture2DManagement::UpdateTexture2D(const Texture2DHandle& hanlde, const std::string& path)
	{}

	void IVulkanTexture2DManagement::UpdateTexture2D(const Texture2DHandle & hanlde, VkImage image, VkImageView image_view)
	{}

	void IVulkanTexture2DManagement::UpdateTexture2DWithoutDestory(const Texture2DHandle & hanlde, VkImage image, VkImageView image_view)
	{}

	bool IVulkanTexture2DManagement::IsTextureReady(const Texture2DHandle & handle) const
	{
		if (!handle.IsValid()) return false;

	}

	void IVulkanTexture2DManagement::_init_default_image()
	{
		_bit_vector_used.SetValue<true>(IHandle(0));
		_bit_vector_used.SetValue<true>(IHandle(1));
		_bit_vector_used.SetValue<true>(IHandle(2));
		_texture_name_cache->insert({ "DefaultWhitePixel", 0 });
		(*_texture_name_cache)["DefaultBlackPixel"] = 1;
		(*_texture_name_cache)["DefaultNormalPixel"] = 2;

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
		_textures[0] = { whiteImage,whiteImageView };
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
		_textures[1] = { blackImage,blackImageView };
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
		_textures[2] = { normalImage,normalImageView };
		_bit_vector_valid.SetValue<true>(IHandle(2));

		IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);
	}

	void IVulkanTexture2DManagement::_init_other()
	{
		_texture_name_cache = new TextureNameMap(64,
			std::hash<std::string>(),
			std::equal_to<std::string>(),
			IMemPoolAllocatorOnlyFixedBlock< std::pair<const std::string, Texture2DHandle>>(IEngineTools::Instance().GetMemPoolPool()));
	}

	void IVulkanTexture2DManagement::_update_texture_count()
	{
		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
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

	void IVulkanTexture2DManagement::_upload_texture_and_generate_mipmaps(VkBuffer staging_buffer, VkImage tex_image, VkFormat trans_format, uint32_t width, uint32_t height, uint32_t level_count, VkDeviceSize buffer_offset)
	{
		auto cmd = IVulkanBase::Base().BeginSingleTimeCommands();

		_transition_image_layout(cmd,
			tex_image,
			trans_format,
			VK_IMAGE_LAYOUT_UNDEFINED,
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

