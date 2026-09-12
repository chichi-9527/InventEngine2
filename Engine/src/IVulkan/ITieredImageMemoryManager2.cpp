#include "IVulkan/ITieredImageMemoryManager2.h"

#include "IVulkan/VulkanBase.h"
#include "IVulkan/VulkanConfig.h"
#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"

#include <vma/vk_mem_alloc.h>

#include <mutex>

namespace INVENT
{
	// staging: 每个"传输轮次"一块, 环形复用; 复用前由调用方等待该槽位 fence
	struct StagingPool
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		void* mappedData = nullptr;
		VkDeviceSize currentOffset{ 0 };
	};
	static std::array<StagingPool, IVulkan::UPLOAD_STAGING_COUNT> stagingPools{};
	static std::vector<VmaBudget> Budgets;
	static std::mutex BudgetsMutex;		// 上传线程与主线程都可能查询预算

	bool ITieredImageMemoryManager2::Init()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;
		auto pool = IEngineTools::Instance().GetMemPoolPool();
		if (nullptr == pool) return false;

		// staging (轮次环形)
		for (std::uint32_t i = 0; i < IVulkan::UPLOAD_STAGING_COUNT; ++i)
		{
			if (VkResult result = IVulkanBase::Base().UseVmaCreateBuffer(IVulkan::DEF_STAGING_BUFFER_SIZE,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				stagingPools[i].buffer,
				&(stagingPools[i].mappedData)))
			{
				INVENT_LOG_ERROR("[ITieredImageMemoryManager] Failed to create staging pool(VkBuffer) for round!");
				return false;
			}
		}

		//   不再预先创建单一图像池
		//   旧代码把池钉死在内存类型 0 上 (#if 0 里那句 vmaFindMemoryTypeIndexForImageInfo
		//   本该填 _image_memory_type_index), 而不同格式 image 的 memoryTypeBits 不同,
		//   这正是本次 vkBindImageMemory 报错的根因。
		//   现在改为 CreateVkImage 时按图查询内存类型并懒建对应池。

		_size_cache = new DedicatedImageSizeCache(64,
			std::hash<size_t>(),
			std::equal_to<size_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const size_t, VkDeviceSize>>(pool));

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

	void ITieredImageMemoryManager2::Clear()
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return;

		for (auto& stagingPool : stagingPools)
		{
			if (stagingPool.buffer != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyBuffer(stagingPool.buffer);
				stagingPool.buffer = VK_NULL_HANDLE;
				stagingPool.mappedData = nullptr;
				stagingPool.currentOffset = 0;
			}
		}

		// 销毁所有按内存类型懒建的图像池
		{
			std::unique_lock<std::shared_mutex> lock(_image_pools_mutex);
			for (auto& [type, imgPool] : _image_pools)
			{
				if (imgPool != nullptr)
					vmaDestroyPool(allocator, imgPool);
			}
			_image_pools.clear();
		}

		if (_size_cache)
		{
			delete _size_cache;
			_size_cache = nullptr;
		}

		_current_total_texture_memory = 0;
	}

	VkResult ITieredImageMemoryManager2::CreateVkImage(VkImage& out, const ICreateImageInfo& info)
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
			// 为"这一张图"查询它真正兼容的内存类型
			// (注意 findInfo 不能带 pool 字段, 否则只会返回池自身的类型)
			VmaAllocationCreateInfo findInfo{};
			findInfo.usage = VMA_MEMORY_USAGE_AUTO;
			std::uint32_t memTypeIndex = 0;
			if (VkResult r = vmaFindMemoryTypeIndexForImageInfo(allocator, &imageInfo, &findInfo, &memTypeIndex))
			{
				INVENT_LOG_ERROR(std::format("[ITieredImageMemoryManager] 查询图像内存类型失败! VkResult: {}.", static_cast<std::int32_t>(r)));
				return r;
			}

			VmaPool pool = _get_or_create_image_pool(memTypeIndex);
			if (pool == nullptr) return VK_ERROR_OUT_OF_DEVICE_MEMORY;

			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.pool = pool;
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT;

			VmaAllocation allocation;
			VkResult res = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &out, &allocation, nullptr);
			if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
			{
				if (!_is_texture_budget_sufficient(memTypeIndex, IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE))
					return VK_ERROR_OUT_OF_DEVICE_MEMORY; // 擴容超標

				VmaStatistics vmaStats;
				vmaGetPoolStatistics(allocator, pool, &vmaStats);
				if (vmaStats.blockCount >= IVulkan::MAX_IMAGE_POOL_BLOCK_COUNT)
					return VK_ERROR_OUT_OF_DEVICE_MEMORY;

				// 允许该池扩容一个块
				allocCreateInfo.flags = 0;
				if (VkResult res2 = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &out, &allocation, nullptr))
				{
					return res2;
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

	VmaPool ITieredImageMemoryManager2::_get_or_create_image_pool(std::uint32_t memory_type_index)
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return nullptr;

		std::unique_lock<std::shared_mutex> lock(_image_pools_mutex);
		auto iter = _image_pools.find(memory_type_index);
		if (iter != _image_pools.end())
			return iter->second;

		if (!_is_texture_budget_sufficient(memory_type_index, IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE))
		{
			INVENT_LOG_WARNING(std::format("[ITieredImageMemoryManager] 显存预算不足, 无法为内存类型 {} 创建图像池.", memory_type_index));
			return nullptr;
		}

		VmaPoolCreateInfo imagePoolCreateInfo{};
		imagePoolCreateInfo.memoryTypeIndex = memory_type_index;
		imagePoolCreateInfo.blockSize = static_cast<VkDeviceSize>(IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE);
		imagePoolCreateInfo.minBlockCount = 1;
		imagePoolCreateInfo.maxBlockCount = 0; // 无上限, 按需扩容

		VmaPool newPool = nullptr;
		if (VkResult result = vmaCreatePool(allocator, &imagePoolCreateInfo, &newPool))
		{
			INVENT_LOG_ERROR(std::format("[ITieredImageMemoryManager] Failed to create Vma VkImage pool for memory type {}! VkResult: {}.",
				memory_type_index, static_cast<std::int32_t>(result)));
			return nullptr;
		}

		_image_pools.emplace(memory_type_index, newPool);
		_current_total_texture_memory += static_cast<VkDeviceSize>(IVulkan::DEF_IMAGE_POOL_BLOCK_SIZE);
		INVENT_LOG_INFO(std::format("[ITieredImageMemoryManager] created image pool for memory type {}.", memory_type_index));
		return newPool;
	}

	void ITieredImageMemoryManager2::DestroyVkImage(VkImage image)
	{
		if (image == VK_NULL_HANDLE) return;

		std::unique_lock<std::shared_mutex> lock(_size_cache_mutex);
		auto iter = _size_cache->find(reinterpret_cast<std::uint64_t>(image));
		if (iter != _size_cache->end()) // Dedicated
		{
			auto allocatedSize = iter->second;
			_size_cache->erase(iter);
			if (_current_total_texture_memory >= allocatedSize) _current_total_texture_memory -= allocatedSize;
			else _current_total_texture_memory = 0;
		}
		// pool: 无需统计

		IVulkanBase::Base().UseVmaDestroyImage(image);
	}

	bool ITieredImageMemoryManager2::CreateStagingBuffer(VkBuffer& out, VkDeviceSize& out_buffer_offset,
		std::uint32_t pool_index, VkDeviceSize buffer_size, void** out_mapped_data)
	{
		if (pool_index >= IVulkan::UPLOAD_STAGING_COUNT) return false;

		auto& stagingPool = stagingPools[pool_index];

		VkDeviceSize alignedOffset = (stagingPool.currentOffset + 15) & ~(VkDeviceSize{ 15 });
		if (alignedOffset + buffer_size > IVulkan::DEF_STAGING_BUFFER_SIZE)
		{
			return false;
		}

		out = stagingPool.buffer;
		out_buffer_offset = alignedOffset;
		if (out_mapped_data)
			*out_mapped_data = static_cast<std::byte*>(stagingPool.mappedData) + alignedOffset;
		stagingPool.currentOffset = alignedOffset + buffer_size;
		return true;
	}

	void ITieredImageMemoryManager2::ResetStagingBuffer(std::uint32_t pool_index)
	{
		if (pool_index >= IVulkan::UPLOAD_STAGING_COUNT) return;
		stagingPools[pool_index].currentOffset = 0;
	}

	std::uint32_t ITieredImageMemoryManager2::GetStagingPoolCount() { return IVulkan::UPLOAD_STAGING_COUNT; }
	VkDeviceSize ITieredImageMemoryManager2::GetStagingBufferSize() { return IVulkan::DEF_STAGING_BUFFER_SIZE; }

	bool ITieredImageMemoryManager2::_is_texture_budget_sufficient(std::uint32_t memory_type_index, VkDeviceSize required_size)
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (nullptr == allocator) return false;

		std::lock_guard<std::mutex> budgetLock(BudgetsMutex);	// 多线程防护
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
			INVENT_LOG_WARNING("系统显存不足!");
			return false;
		}

		return true;
	}
}
