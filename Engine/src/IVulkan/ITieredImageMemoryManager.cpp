#include "IVulkan/ITieredImageMemoryManager.h"

#include "IVulkan/VulkanBase.h"
#include "IVulkan/VulkanConfig.h"
#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"

#include <vma/vk_mem_alloc.h>

namespace INVENT
{
	static std::array<VmaPool, IVulkan::MAX_FRAMES_IN_FLIGHT> stagingPools{ nullptr };
	static VmaPool vkimagePool = nullptr;

	bool ITieredImageMemoryManager::Init()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;
		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (nullptr == pool) return false;

		// stagingPools

		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = 1024; // 无意义
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo stagingAllocInfo{};
		stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

		std::uint32_t stagingMemTypeIndex = 0;
		vmaFindMemoryTypeIndexForBufferInfo(allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingMemTypeIndex);

		VmaPoolCreateInfo stagingPoolCreateInfo{};
		stagingPoolCreateInfo.memoryTypeIndex = stagingMemTypeIndex;
		stagingPoolCreateInfo.blockSize = VkDeviceSize{ 32 } *1024 * 1024;
		stagingPoolCreateInfo.minBlockCount = 1; // 初始就分配 1 塊 32MB
		stagingPoolCreateInfo.maxBlockCount = 1; // 鎖死上限為 1 塊！絕對不允許超出每影格頻寬

		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (VkResult result = vmaCreatePool(allocator, &stagingPoolCreateInfo, &stagingPools[i]))
			{
				INVENT_LOG_ERROR("[ITieredImageMemoryManager] Failed to create VMA staging pool for frame!");
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

		uint32_t imageMemTypeIndex = 0;
		vmaFindMemoryTypeIndexForImageInfo(allocator, &imageCreateInfo, &imageAllocInfo, &imageMemTypeIndex);

		VmaPoolCreateInfo imagePoolCreateInfo{};
		imagePoolCreateInfo.memoryTypeIndex = imageMemTypeIndex;
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

		return true;
	}

	void ITieredImageMemoryManager::Clear()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return;

		for (auto& stagingPool : stagingPools)
		{
			if (stagingPool != nullptr)
			{
				vmaDestroyPool(allocator, stagingPool);
				stagingPool = nullptr;
			}
		}
		if (vkimagePool != nullptr)
		{
			vmaDestroyPool(allocator, vkimagePool);
			vkimagePool = nullptr;
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
		imageInfo.extent = { info.ImageSize.first, info.ImageSize.second, 1 };
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

		if (memReqs.size < IVulkan::CREATE_VKIMAGE_LIMIT)
		{
			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.pool = vkimagePool;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT;

			VmaAllocation allocation;
			VkResult res = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &out, &allocation, nullptr);
			if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
			{
				uint32_t memTypeIndex = 0;
				vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &allocCreateInfo, &memTypeIndex);
				if (!_is_texture_budget_sufficient(memTypeIndex, IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE))
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
		else
		{
			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

			uint32_t memTypeIndex = 0;
			vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &allocCreateInfo, &memTypeIndex);
			if (!_is_texture_budget_sufficient(memTypeIndex, memReqs.size))
				return VK_ERROR_OUT_OF_DEVICE_MEMORY;
			if (VkResult res = IVulkanBase::Base().UseVmaCreateImage(info.ImageSize.first,
				info.ImageSize.second,
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
			(*_size_cache)[reinterpret_cast<std::uint64_t>(out)] = memReqs.size;
			return VK_SUCCESS;
		}

		return VK_ERROR_UNKNOWN;
	}

	bool ITieredImageMemoryManager::_is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size)
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;

		uint32_t heapCount = IVulkanBase::Base().GetPhysicalDeviceMemoryProperties().memoryHeapCount;
		std::vector<VmaBudget> budgets(heapCount);
		vmaGetHeapBudgets(allocator, budgets.data());

		const VkPhysicalDeviceMemoryProperties* memProps;
		vmaGetMemoryProperties(allocator, &memProps);
		uint32_t heapIndex = memProps->memoryTypes[memory_type_index].heapIndex;

		VkDeviceSize osBudget = budgets[heapIndex].budget;
		VkDeviceSize maxTextureAllowed = static_cast<VkDeviceSize>(osBudget * IVulkan::MAX_TEXTURE_BUDGET_RATIO);
		if (_current_total_texture_memory + required_size > maxTextureAllowed)
		{
			INVENT_LOG_WARNING(std::format("纹理显存预算已满: {:2f} MB.", static_cast<double>(_current_total_texture_memory) / (1024.0 * 1024.0)));
			return false;
		}
		VkDeviceSize globalUsage = budgets[heapIndex].usage;
		if (globalUsage + required_size + IVulkan::ABSOLUTE_SAFETY_MARGIN > osBudget)
		{
			INVENT_LOG_WARNING(std::format("系统显存不足!"));
			return false;
		}
		
		return true;
	}


}
