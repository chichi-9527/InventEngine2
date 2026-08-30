#include "IVulkan/ITieredImageMemoryManager.h"

#include "IVulkan/VulkanBase.h"
#include "IVulkan/VulkanConfig.h"
#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"

#include <vma/vk_mem_alloc.h>

namespace INVENT
{
	struct StagingPool 
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		VkDeviceSize  currentOffset{ 0 };
	};

	static std::array<StagingPool, IVulkan::MAX_FRAMES_IN_FLIGHT> stagingPools{ nullptr };
	static std::array<std::vector<VmaAllocation>, IVulkan::MAX_FRAMES_IN_FLIGHT> stagingAllocations;
	static VmaPool vkimagePool = nullptr;
	static std::vector<VmaBudget> Budgets;

	bool ITieredImageMemoryManager::Init()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;
		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (nullptr == pool) return false;

		// stagingPools

		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(IVulkan::DEF_STAGING_BUFFER_SIZE,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				stagingPools[i].buffer,
				&(stagingPools[i].mappedData)))
			{
				INVENT_LOG_ERROR("[ITieredImageMemoryManager] Failed to create staging pool(VkBuffer) for frame!");
				return false;
			}
		}

		// vkimagePool

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent = { 1024,1024,1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = VK_FORMAT_BC7_SRGB_BLOCK;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		VmaAllocationCreateInfo imageAllocInfo{};
		imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

#if 0
		vmaFindMemoryTypeIndexForImageInfo(allocator, &imageCreateInfo, &imageAllocInfo, &_image_memory_type_index);
		const VkPhysicalDeviceMemoryProperties* memProps;
		vmaGetMemoryProperties(allocator, &memProps);
		uint32_t heapIndex = memProps->memoryTypes[_image_memory_type_index].heapIndex;
		INVENT_LOG_INFO(std::format("[ITieredImageMemoryManager] Find memory heap index for image: {}.", heapIndex));
#endif

		VmaPoolCreateInfo imagePoolCreateInfo{};
		imagePoolCreateInfo.memoryTypeIndex = _image_memory_type_index;
		imagePoolCreateInfo.blockSize = static_cast<VkDeviceSize>(IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE);
		imagePoolCreateInfo.minBlockCount = 1;
		imagePoolCreateInfo.maxBlockCount = 0; // 設為 0 表示無上限，允許隨時向顯存動態擴容！

		if (VkResult result = vmaCreatePool(allocator, &imagePoolCreateInfo, &vkimagePool))
		{
			INVENT_LOG_ERROR("[ITieredImageMemoryManager] Failed to create VMA VkImage pool!");
			return false;
		}
		_current_total_texture_memory = static_cast<VkDeviceSize>(IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE);

		// init cache
		_size_cache = new DedicatedImageSizeCache(64,
			std::hash<size_t>(),
			std::equal_to<size_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const size_t, VkDeviceSize>>(pool));

		// get bugets
		uint32_t heapCount = IVulkanBase::Base().GetPhysicalDeviceMemoryProperties().memoryHeapCount;
		Budgets.resize(heapCount);
		vmaGetHeapBudgets(allocator, Budgets.data());
		INVENT_LOG_INFO("[ITieredImageMemoryManager] Heap budgets: ");
		uint32_t i{ 0 };
		for (auto& budget : Budgets)
		{
			INVENT_LOG_INFO(std::format("[ITieredImageMemoryManager] \tNO.{} : {} MB.", i++, static_cast<std::uint64_t>(budget.budget) / (1024 * 1024)));
		}

		return true;
	}

	void ITieredImageMemoryManager::Clear()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return;

		for (auto& stagingPool : stagingPools)
		{
			if (stagingPool.buffer != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyBuffer(stagingPool.buffer);
				stagingPool.buffer = VK_NULL_HANDLE;
			}
		}
		if (vkimagePool != nullptr)
		{
			vmaDestroyPool(allocator, vkimagePool);
			vkimagePool = nullptr;
		}
		if (_size_cache)
		{
			delete _size_cache;
			_size_cache = nullptr;
		}

	}

	VkResult ITieredImageMemoryManager::CreateVkImage(VkImage& out, const ICreateImageInfo& info)
	{
		auto device = IVulkanBase::Base().GetDevice();
		if (device == VK_NULL_HANDLE) return VK_ERROR_DEVICE_LOST;
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return VK_ERROR_UNKNOWN;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { info.ImageWidth, info.ImageHeight, 1 };
		imageInfo.mipLevels = info.MipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = info.ImageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkDeviceImageMemoryRequirements deviceImgReqs{};
		deviceImgReqs.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
		deviceImgReqs.pCreateInfo = &imageInfo;
		VkMemoryRequirements2 memRequirements2{};
		memRequirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

		vkGetDeviceImageMemoryRequirements(device, &deviceImgReqs, &memRequirements2);
		VkMemoryRequirements memReqs = memRequirements2.memoryRequirements;

		if (memReqs.size < IVulkan::CREATE_VKIMAGE_LIMIT) // pool create
		{
			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.pool = vkimagePool;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT;

			VmaAllocation allocation;
			VkResult res = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &out, &allocation, nullptr);
			if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
			{
				if (!_is_texture_budget_sufficient(_image_memory_type_index, IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE))
					return VK_ERROR_OUT_OF_DEVICE_MEMORY; // 擴容超標

				VmaStatistics vmaStats;
				vmaGetPoolStatistics(allocator, vkimagePool, &vmaStats);
				uint32_t currentBlockCount = vmaStats.blockCount;
				if (currentBlockCount >= IVulkan::MAX_IMAGE_POOL_BLOCK_COUNT)
					return VK_ERROR_OUT_OF_DEVICE_MEMORY;

				// 允许扩容
				allocCreateInfo.flags = 0;
				if (VkResult res = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &out, &allocation, nullptr))
				{
					return res;
				}
				_current_total_texture_memory += static_cast<VkDeviceSize>(IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE);
			}
			IVulkanBase::Base().InsertVmaImageCache(out, allocation);
			return VK_SUCCESS;
		}
		else // dedicated create
		{
			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

			uint32_t memTypeIndex = 0;
			vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &allocCreateInfo, &memTypeIndex);
			if (!_is_texture_budget_sufficient(memTypeIndex, memReqs.size))
				return VK_ERROR_OUT_OF_DEVICE_MEMORY;

			{
				std::shared_lock<std::shared_mutex> lock(_size_cache_mutex);
				if (_size_cache->size() > IVulkan::MAX_DEDICATE_VKIMAGE_NUM)
					return VK_ERROR_OUT_OF_DEVICE_MEMORY;
			}

			if (VkResult res = IVulkanBase::Base().UseVmaCreateImage(info.ImageWidth,
				info.ImageHeight,
				info.MipLevels,
				info.ImageFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
				out))
			{
				return res;
			}
			_current_total_texture_memory += memReqs.size;

			std::unique_lock<std::shared_mutex> lock(_size_cache_mutex);
			(*_size_cache)[reinterpret_cast<std::uint64_t>(out)] = memReqs.size;
			return VK_SUCCESS;
		}

		return VK_ERROR_UNKNOWN;
	}

	void ITieredImageMemoryManager::DestroyVkImage(VkImage image)
	{
		if (image == VK_NULL_HANDLE) return;

		std::unique_lock<std::shared_mutex> lock(_size_cache_mutex);
		auto iter = _size_cache->find(reinterpret_cast<std::uint64_t>(image));
		if (iter != _size_cache->end()) // pool
		{
			_size_cache->erase(iter);
		}
		else // Dedicated
		{
			auto allocatedSize = iter->second;
			if (_current_total_texture_memory >= allocatedSize) _current_total_texture_memory -= allocatedSize;
			else _current_total_texture_memory = 0;
		}

		IVulkanBase::Base().UseVmaDestroyImage(image);
	}

	bool ITieredImageMemoryManager::CreateStagingBuffer(VkBuffer& out, VkDeviceSize& out_buffer_offset, std::uint32_t frame_index, VkDeviceSize buffer_size, void** out_mapped_data)
	{
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return false;
		
		auto& stagingPool = stagingPools[frame_index];

		VkDeviceSize alignedOffset = (stagingPool.currentOffset + 15) & ~(VkDeviceSize{ 15 });
		if (alignedOffset + buffer_size > IVulkan::DEF_STAGING_BUFFER_SIZE)
		{
			return false;
		}

		out = stagingPool.buffer;
		out_buffer_offset = alignedOffset;
		*out_mapped_data = static_cast<std::byte*>(stagingPool.mappedData) + alignedOffset;

		stagingPool.currentOffset = alignedOffset + buffer_size;

		return true;
	}

	void ITieredImageMemoryManager::ResetStagingBuffer(std::uint32_t frame_index)
	{
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;

		stagingPools[frame_index].currentOffset = 0;
	}

	bool ITieredImageMemoryManager::_is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size)
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;

		// 需要重新获取
		vmaGetHeapBudgets(allocator, Budgets.data());

		const VkPhysicalDeviceMemoryProperties* memProps;
		vmaGetMemoryProperties(allocator, &memProps);
		uint32_t heapIndex = memProps->memoryTypes[memory_type_index].heapIndex;

		VkDeviceSize osBudget = Budgets[heapIndex].budget;
		VkDeviceSize maxTextureAllowed = static_cast<VkDeviceSize>(osBudget * IVulkan::MAX_TEXTURE_BUDGET_RATIO);
		if (_current_total_texture_memory + required_size > maxTextureAllowed)
		{
			INVENT_LOG_WARNING(std::format("纹理显存预算已满: {:2f} MB.", static_cast<double>(_current_total_texture_memory) / (1024.0 * 1024.0)));
			return false;
		}
		VkDeviceSize globalUsage = Budgets[heapIndex].usage;
		if (globalUsage + required_size + IVulkan::ABSOLUTE_SAFETY_MARGIN > osBudget)
		{
			INVENT_LOG_WARNING(std::format("系统显存不足!"));
			return false;
		}
		
		return true;
	}


}
