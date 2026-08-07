#include "IVulkan/VulkanBase.h"

#include "ILog.h"
#include "IVulkan/VulkanConfig.h"

#include <cstdlib>
#include <filesystem>

#include <vma/vk_mem_alloc.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"
#endif // _WIN32

namespace INVENT
{
	static VmaAllocator vmaAllocator = nullptr;
	static std::unordered_map<VkBuffer, VmaAllocation> MapBufferAllocation;
	static std::unordered_map<VkImage, VmaAllocation> MapImageAllocation;

	static std::vector<const char*> validationLayers;
	static std::vector<const char*> instanceExtensions;
	static std::vector<const char*> deviceExtensions;

#ifdef VULKAN_VALITADION_LAYER
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{

		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			INVENT_LOG_TRACE(std::format("[VulkanBase]: {} ", pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			INVENT_LOG_INFO(std::format("[VulkanBase]: {} ", pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			INVENT_LOG_WARNING(std::format("[VulkanBase]: {} ", pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			INVENT_LOG_ERROR(std::format("[VulkanBase]: {} ", pCallbackData->pMessage));
			break;
		default:
			break;
		}

		return VK_FALSE;
	}
#endif // VULKAN_VALITADION_LAYER


	IVulkanBase::~IVulkanBase()
	{}

	void IVulkanBase::AddValidationLayer(const char* layerName)
	{
		validationLayers.push_back(layerName);
	}

	void IVulkanBase::AddInstanceExtension(const char* extensionName)
	{
		instanceExtensions.push_back(extensionName);
	}

	void IVulkanBase::AddDeviceExtension(const char* extensionName)
	{
		deviceExtensions.push_back(extensionName);
	}

	IVulkanBase& IVulkanBase::Base()
	{
		static IVulkanBase base;
		return base;
	}

	bool IVulkanBase::CreateVulkanInstance()
	{
		if (_use_lastest_api_version() == UINT32_MAX)
		{
			return false;
		}
		INVENT_LOG_INFO(
			std::format("[VulkanBase] 此电脑支持的最高 vulkan 版本：{}.{}.{}",
				VK_VERSION_MAJOR(_api_version),
				VK_VERSION_MINOR(_api_version),
				VK_VERSION_PATCH(_api_version))
		);
		if (!Version_1_3_OrHigher())
		{
			INVENT_LOG_FATAL("[VulkanBase] 需要的最低 vulkan 版本为 1.3");
			return false;
		}

#ifdef VULKAN_VALITADION_LAYER
		IVulkanBase::AddValidationLayer("VK_LAYER_KHRONOS_validation");
		IVulkanBase::AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // VULKAN_VALITADION_LAYER

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan App";
		appInfo.apiVersion = _api_version;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

#ifdef VULKAN_VALITADION_LAYER
		if (auto check = !_check_validation_layers())
		{
			return false;
		}
		createInfo.enabledLayerCount = uint32_t(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = (void*)&debugCreateInfo;
#else
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#endif // VULKAN_VALITADION_LAYER
		createInfo.enabledExtensionCount = uint32_t(instanceExtensions.size());
		createInfo.ppEnabledExtensionNames = instanceExtensions.data();

		if (VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance))
		{
			INVENT_LOG_ERROR("[VulkanBase] failed to create instance!");
			return false;
		}

		INVENT_LOG_INFO(std::format("[VulkanBase] Create Instance done, instance Extensions Num : {}.  :", instanceExtensions.size()));
		for (auto& name : instanceExtensions)
		{
			INVENT_LOG_INFO(std::format("\t {} ;", name));
		}

#ifdef VULKAN_VALITADION_LAYER
		return _setup_debug_messenger();
#endif // VULKAN_VALITADION_LAYER

		return true;
	}

	bool IVulkanBase::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		if (VkResult result = vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get the count of physical devices! VkResult: {}.", static_cast<std::int32_t>(result)));
			return false;
		}
		if (deviceCount == 0)
		{
			INVENT_LOG_ERROR("[VulkanBase] Failed to find GPUs with Vulkan support!");
			return false;
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		// 选择显卡
		auto isDeviceSuitable = [this](VkPhysicalDevice device)->bool {
			VkPhysicalDeviceProperties deviceProperties;
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			_queue_family_indices = _find_queue_families(device);
			_physical_device_properties = deviceProperties;

			auto swapChainAdequate = [this, device]()->bool {
				_swap_chain_support = _query_swap_chain_support(device);
				return !_swap_chain_support.Formats.empty() && !_swap_chain_support.PresentModes.empty();
				};

			return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
				&& deviceFeatures.geometryShader
				&& deviceFeatures.samplerAnisotropy
				&& _queue_family_indices.IsComplete()
				&& _check_device_extension_support(device)
				&& swapChainAdequate();
			};

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				_physical_device = device;
				break;
			}
		}

		if (_physical_device == VK_NULL_HANDLE)
		{
			INVENT_LOG_ERROR("[VulkanBase] failed to find a suitable GPU!");
			return false;
		}

		_get_descriptor_indexing_properties();

		INVENT_LOG_INFO(std::format("[VulkanBase] device name : {}.", _physical_device_properties.deviceName));
		INVENT_LOG_INFO(std::format("[VulkanBase] device maximum number of sampled image descriptors : {}.", _descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages));

		return true;
	}

	bool IVulkanBase::CreateLogicalDevice()
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { _queue_family_indices.GraphicsFamily,_queue_family_indices.PresentFamily };
		float queuePriority = 1.0f;
		for (auto queueFamily : uniqueQueueFamilies)
		{
			auto& queueCreateInfo = queueCreateInfos.emplace_back();
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE; // 允許紋理採樣器開啟各向異性過濾

		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.dynamicRendering = VK_TRUE;      // 开启动态渲染
		features13.synchronization2 = VK_TRUE;      // 开启同步2

		VkPhysicalDeviceVulkan12Features feat12{};
		feat12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		feat12.descriptorBindingPartiallyBound = VK_TRUE; // 部分綁定描述符
		feat12.runtimeDescriptorArray = VK_TRUE; // 運行時描述符陣列
		feat12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE; // 非均勻索引
		feat12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE; // 綁定後更新
		feat12.bufferDeviceAddress = VK_TRUE; // 开启Buffer设备地址
		feat12.pNext = &features13;

		VkPhysicalDeviceVulkan11Features feat11 = {};
		feat11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		feat11.shaderDrawParameters = VK_TRUE; // GPU 驱动渲染
		feat11.pNext = &feat12;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
#ifdef VULKAN_VALITADION_LAYER
		deviceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#else
		deviceCreateInfo.enabledLayerCount = 0;
#endif // VULKAN_VALITADION_LAYER
		deviceCreateInfo.pNext = &feat11;

		if (VkResult result = vkCreateDevice(_physical_device, &deviceCreateInfo, nullptr, &_device))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] failed to create logical device! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}

		vkGetDeviceQueue(_device, _queue_family_indices.GraphicsFamily, 0, &_graphics_queue);
		vkGetDeviceQueue(_device, _queue_family_indices.PresentFamily, 0, &_present_queue);

		return true;
	}

	bool IVulkanBase::CreateSwapChain()
	{
		auto surfaceFormat = _choose_swap_surface_format(_swap_chain_support.Formats);
		auto presentMode = _choose_swap_presenta_mode(_swap_chain_support.PresentModes);

		// 获取最新的 Surface Capabilities
		if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical_device, _surface, &_swap_chain_support.Capabilities))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to Get Physical Device Surface Capabilities! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}
		auto extent = _choose_swap_extent(_swap_chain_support.Capabilities);

		uint32_t imageCount = _swap_chain_support.Capabilities.minImageCount + 1;
		if (_swap_chain_support.Capabilities.maxImageCount > 0 && imageCount > _swap_chain_support.Capabilities.maxImageCount)
		{
			imageCount = _swap_chain_support.Capabilities.maxImageCount;
		}
		// 判断支持功能
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical_device, _surface, &surfaceCapabilities);
		// For screenshot
		if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		{
			_swap_chain_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		// For previous frame sampling
		if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		{
			_swap_chain_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		// VkSwapchainCreateInfoKHR
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = _surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = _swap_chain_image_usages;

		uint32_t queueFamilyIndices[] = { _queue_family_indices.GraphicsFamily, _queue_family_indices.PresentFamily };
		if (_queue_family_indices.GraphicsFamily != _queue_family_indices.PresentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
		createInfo.preTransform = _swap_chain_support.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		//
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// 
		if (VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swap_chain))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create a swapchain! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}

		uint32_t swapchainImageCount;
		if (VkResult result = vkGetSwapchainImagesKHR(_device, _swap_chain, &swapchainImageCount, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get the count of swapchain images! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}
		_swap_chain_images.resize(swapchainImageCount);
		if (VkResult result = vkGetSwapchainImagesKHR(_device, _swap_chain, &swapchainImageCount, _swap_chain_images.data()))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get swapchain images! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}
		_swap_chain_image_format = surfaceFormat.format;
		_swap_chain_extent = extent;
		_swap_chain_image_count = swapchainImageCount;

		return true;
	}

	bool IVulkanBase::CreateSwapChainImageView()
	{
		_swap_chain_image_views.resize(_swap_chain_images.size());
		for (size_t i = 0; i < _swap_chain_images.size(); ++i)
		{
			_swap_chain_image_views[i] = CreateImageView(_swap_chain_images[i], _swap_chain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
			if (_swap_chain_image_views[i] == nullptr)
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to Create SwapChainImageView! the number: {}.", i));
				return false;
			}
		}

		return true;
	}

	bool IVulkanBase::CreateVmaAllocator()
	{
		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		allocatorCreateInfo.vulkanApiVersion = _api_version;
		allocatorCreateInfo.physicalDevice = _physical_device;
		allocatorCreateInfo.device = _device;
		allocatorCreateInfo.instance = _instance;
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

		if (VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator))
		{
			INVENT_LOG_ERROR("[VulkanBase] Create Vma Allocator error!");
			return false;
		}
		INVENT_LOG_INFO("[VulkanBase] Create Vma Allocator done.");
		return true;
	}

	bool IVulkanBase::FindDepthFormat()
	{
		_depth_format = _find_supported_format(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
		if (VK_FORMAT_UNDEFINED == _depth_format)
		{
			INVENT_LOG_ERROR("[VulkanBase] Failed to find supported depth format!");
			return false;
		}
		return true;
	}

	bool IVulkanBase::InitializeAllOffscreenPasses()
	{
		return false;
	}

	bool IVulkanBase::CreateBindlessDescriptorPool()
	{
		_absolute_descriptor_limit = std::min({ IVulkan::MAX_BINDLESS_TEXTURES,
			_descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages,
			_descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages });
		INVENT_LOG_INFO(std::format("[VulkanBase] Absolute descriptor count : {}.", _absolute_descriptor_limit));
		_current_descriptor_count = std::min(IVulkan::DEF_BINDLESS_TEXTURES, _absolute_descriptor_limit);

		VkDescriptorPoolSize poolSizes[1];
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[0].descriptorCount = _current_descriptor_count;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = poolSizes;

		if (VkResult result = vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_bindless_descriptor_pool))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create descriptor pool! VkResult: {).", static_cast<int32_t>(result)));
			return false;
		}

		return true;
	}

	bool IVulkanBase::CreateGlobalPipelineLayout()
	{
		// Set0 
		// binding 0 : ubo
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		// binding 1 : point light ssbo
		VkDescriptorSetLayoutBinding pointLightSSBOLayoutBinding{};
		pointLightSSBOLayoutBinding.binding = 1;
		pointLightSSBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pointLightSSBOLayoutBinding.descriptorCount = 1;
		pointLightSSBOLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pointLightSSBOLayoutBinding.pImmutableSamplers = nullptr;
		std::vector<VkDescriptorSetLayoutBinding> set0Bindings = {
			uboLayoutBinding,
			pointLightSSBOLayoutBinding
		};
		auto set0Layout = _create_descriptor_set_layout(set0Bindings);

		// Set1 Bindless Layout and Material SSBO
		// binding 0 : 全局采样器
		VkDescriptorSetLayoutBinding globalSamplerLayoutBinding{};
		globalSamplerLayoutBinding.binding = 0;
		globalSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		globalSamplerLayoutBinding.descriptorCount = 1;
		globalSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		globalSamplerLayoutBinding.pImmutableSamplers = nullptr;
		// binding 1 : 贴图阵列
		VkDescriptorSetLayoutBinding textureListLayoutBinding{};
		textureListLayoutBinding.binding = 1;
		textureListLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		textureListLayoutBinding.descriptorCount = _current_descriptor_count;
		textureListLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureListLayoutBinding.pImmutableSamplers = nullptr;
		std::vector<VkDescriptorSetLayoutBinding> set1Bindings = {
			globalSamplerLayoutBinding,
			textureListLayoutBinding
		};
		auto set1Layout = _create_descriptor_set_layout(set1Bindings, true);

		_descriptor_set_layouts = {
			set0Layout,
			set1Layout
		};

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(IVulkan::PushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(_descriptor_set_layouts.size());
		pipelineLayoutInfo.pSetLayouts = _descriptor_set_layouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		if (VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_global_pipeline_layout))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create global pipeline layout! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}

		return true;
	}

	bool IVulkanBase::AllocaGlobalBindlessDescriptorSet()
	{
		return false;
	}

	bool IVulkanBase::CreateCommandPool()
	{
		return false;
	}

	bool IVulkanBase::UseVmaCreateBuffer(VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkBuffer& buffer)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo vmaAllocInfo{};
		vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}

		VmaAllocation allocation;
		if (VkResult result = vmaCreateBuffer(vmaAllocator, &bufferInfo, &vmaAllocInfo, &buffer, &allocation, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create buffer! VkBufferUsageFlags : {}, VkResult: {}.", static_cast<int32_t>(usage), static_cast<int32_t>(result)));
			return false;
		}

		MapBufferAllocation[buffer] = allocation;

		return true;
	}

	void IVulkanBase::UseVmaDestroyBuffer(VkBuffer buffer)
	{
		auto iter = MapBufferAllocation.find(buffer);
		if (iter != MapBufferAllocation.end())
			vmaDestroyBuffer(vmaAllocator, buffer, iter->second);
		MapBufferAllocation.erase(buffer);
	}

	bool IVulkanBase::UseVmaCreateImage(uint32_t width,
		uint32_t height,
		uint32_t mip_levels,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage & image)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mip_levels;
		imageInfo.arrayLayers = 1;
		//
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		VmaAllocationCreateInfo vmaAllocInfo{};
		vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}

		VmaAllocation allocation;
		if (VkResult result = vmaCreateImage(vmaAllocator, &imageInfo, &vmaAllocInfo, &image, &allocation, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create image! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}

		MapImageAllocation[image] = allocation;

		return true;
	}

	void IVulkanBase::UseVmaDestroyImage(VkImage image)
	{
		auto iter = MapImageAllocation.find(image);
		if (iter != MapImageAllocation.end())
			vmaDestroyImage(vmaAllocator, image, iter->second);
		MapImageAllocation.erase(image);
	}

	VkImageView IVulkanBase::CreateImageView(VkImage image,
		VkFormat format,
		VkImageAspectFlags aspect_flags,
		VkImageViewType view_type,
		uint32_t mip_levels,
		uint32_t base_array_layer,
		uint32_t layer_count)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = image;
		imageViewCreateInfo.viewType = view_type;
		imageViewCreateInfo.format = format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = aspect_flags;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = mip_levels;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = base_array_layer;
		imageViewCreateInfo.subresourceRange.layerCount = layer_count;

		VkImageView imageView = nullptr;
		if (VkResult result = vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &imageView))
		{
			return nullptr;
		}

		return imageView;
	}

	void IVulkanBase::DestroyImageView(VkImageView image_view)
	{
		if (image_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(_device, image_view, nullptr);
		}
	}

	bool IVulkanBase::UseVmaMapMemory(VkBuffer buffer, void*& data)
	{
		if (VkResult result = vmaMapMemory(vmaAllocator, MapBufferAllocation[buffer], &data))
		{
			INVENT_LOG_ERROR("[VulkanBase] use vma map memory error!");
			return false;
		}
		return true;
	}

	void IVulkanBase::UseVmaUnmapMemory(VkBuffer buffer)
	{
		vmaUnmapMemory(vmaAllocator, MapBufferAllocation[buffer]);
	}

	const std::uint32_t IVulkanBase::_use_lastest_api_version()
	{
		if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
		{
			if (VkResult result = vkEnumerateInstanceVersion(&_api_version))
			{
				INVENT_LOG_ERROR("[VulkanBase] vkEnumerateInstanceVersion error.");
				return UINT32_MAX;
			}
			return _api_version;
		}
		INVENT_LOG_ERROR("[VulkanBase] not found vkEnumerateInstanceVersion .");
		return UINT32_MAX;
	}

	IVulkanBase::QueueFamilyIndices IVulkanBase::_find_queue_families(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.GraphicsFamily = i;
				indices.HasGraphicsFamily = true;
			}

			VkBool32 presentSupport = false;
			if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport))
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] failed to get device surface! VkResult: {}.", static_cast<int32_t>(result)));
			}

			if (presentSupport)
			{
				indices.PresentFamily = i;
				indices.HasPresentFamily = true;
			}

			if (indices.IsComplete()) break;

			i++;
		}

		return indices;
	}

	IVulkanBase::SwapChainSupportDetails IVulkanBase::_query_swap_chain_support(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;
		if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.Capabilities))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get physical device surface capabilities! VkResult: {}.", static_cast<int32_t>(result)));
			return details;
		}

		uint32_t surfaceFormatCount;
		if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get the count of surface formats! VkResult: {}.", static_cast<int32_t>(result)));
			return details;
		}
		if (surfaceFormatCount)
		{
			details.Formats.resize((size_t)surfaceFormatCount);
			if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, details.Formats.data()))
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get surface formats! VkResult: {}.", static_cast<int32_t>(result)));
				details.Formats.clear();
				return details;
			}
		}
		else
		{
			INVENT_LOG_ERROR("[VulkanBase] Failed to find any supported surface format!");
		}

		uint32_t surfacePresentModeCount;
		if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &surfacePresentModeCount, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get the count of surface present modes! VkResult: {}.", static_cast<int32_t>(result)));
			return details;
		}
		if (surfacePresentModeCount)
		{
			details.PresentModes.resize(surfacePresentModeCount);
			if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &surfacePresentModeCount, details.PresentModes.data()))
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get surface present modes! VkResult: {}.", static_cast<int32_t>(result)));
				details.PresentModes.clear();
				return details;
			}
		}
		else
		{
			INVENT_LOG_ERROR("[VulkanBase] Failed to find any surface present mode!");
		}

		return details;
	}

	bool IVulkanBase::_check_device_extension_support(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		if (VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to get the count of device extension! VkResult: {}.", static_cast<int32_t>(result)));
			return false;
		}
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		if (extensionCount > 0)
		{

			if (VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()))
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to enumerate device extension properties! VkResult: {}.", static_cast<int32_t>(result)));
				return false;
			}
		}
		// 1. 將所有可用的擴展名字放入集合（Set）中，方便 O(1) 快速查找
		std::unordered_set<std::string> availableNameSet;
		for (const auto& ext : availableExtensions)
		{
			availableNameSet.insert(ext.extensionName);
		}
		// 2. 建立一個臨時陣列，只保留確定支援的擴展
		std::vector<const char*> validExtensions;
		bool allRequiredSupported = true;

		for (const auto& requiredExt : deviceExtensions)
		{
			if (requiredExt == nullptr) continue;

			if (availableNameSet.count(requiredExt) > 0)
			{
				validExtensions.push_back(requiredExt);
			}
			else
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] Required extension NOT supported: {}.", requiredExt));
				allRequiredSupported = false;
			}
		}

		deviceExtensions = validExtensions;

		INVENT_LOG_INFO(std::format("Check deviceExtensions done, Num : {}  :", deviceExtensions.size()));
		for (auto& name : deviceExtensions)
		{
			INVENT_LOG_INFO(std::format("\t {}.", name));
		}
		return allRequiredSupported;
	}

	void IVulkanBase::_get_descriptor_indexing_properties()
	{
		_descriptor_indexing_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
		VkPhysicalDeviceProperties2 deviceProps2{};
		deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProps2.pNext = &_descriptor_indexing_properties;
		vkGetPhysicalDeviceProperties2(_physical_device, &deviceProps2);
	}

	VkSurfaceFormatKHR IVulkanBase::_choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
	{
		for (const auto& availableFormat : available_formats)
		{
			if ((availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB) && (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
			{
				return availableFormat;
			}
		}
		return available_formats[0];
	}

	VkPresentModeKHR IVulkanBase::_choose_swap_presenta_mode(const std::vector<VkPresentModeKHR>& available_present_modes, VkPresentModeKHR mode)
	{
		for (const auto& availablePresentMode : available_present_modes)
		{
			if (availablePresentMode == mode)
			{
				return availablePresentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D IVulkanBase::_choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { _frame_buffer_width, _frame_buffer_height };
			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			return actualExtent;
		}
	}

	VkFormat IVulkanBase::_find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(_physical_device, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkDescriptorSetLayout IVulkanBase::_create_descriptor_set_layout(std::vector<VkDescriptorSetLayoutBinding>& bindings, bool is_bindless_set)
	{
		if (bindings.empty()) return VK_NULL_HANDLE;

		std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size());
		for (uint32_t i = 0; i < (uint32_t)bindings.size(); ++i)
		{
			if (is_bindless_set)
			{
				bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
					VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
				if (i == bindings.size() - 1)
				{
					bindingFlags[i] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT; // 在最后一个binding, shader-slang 中可不写死数量
				}
			}
			else
			{
				bindingFlags[i] = 0;
			}
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo layoutBindingFlagsInfo{};
		layoutBindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		layoutBindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
		layoutBindingFlagsInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();
		layoutInfo.pNext = &layoutBindingFlagsInfo;
		if (is_bindless_set)
		{
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}

		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		if (VkResult result = vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &descriptorSetLayout))
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] Failed to create descriptor set layout! VkResult: {}.", static_cast<int32_t>(result)));
			return VK_NULL_HANDLE;
		}

		return descriptorSetLayout;
	}

#ifdef VULKAN_VALITADION_LAYER
	bool IVulkanBase::_check_validation_layers()
	{
		uint32_t layerCount;
		if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr))
		{
			INVENT_LOG_ERROR("[ VulkanBase ] Failed to get the count of instance layers!");
			return false;
		}
		if (layerCount)
		{
			std::vector<VkLayerProperties> availableLayers(layerCount);
			if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
			{
				INVENT_LOG_ERROR("[ VulkanBase ] Failed to enumerate instance layer properties!");
				return false;
			}
			bool found = false;
			for (auto& i : validationLayers)
			{
				for (auto& j : availableLayers)
					if (!strcmp(i, j.layerName))
					{
						found = true;
						break;
					}
				if (!found)
					i = nullptr;
			}
		}
		else
		{
			validationLayers.clear();
		}
		INVENT_LOG_INFO(std::format("[VulkanBase] Check instanceLayers done, Num : {}  :", validationLayers.size()));
		for (auto& name : validationLayers)
		{
			INVENT_LOG_INFO(std::format("\t {}.", name));
		}
		return true;
	}

	bool IVulkanBase::_setup_debug_messenger()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMsgInfo{};
		debugMsgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMsgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugMsgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMsgInfo.pfnUserCallback = debugCallback;
		debugMsgInfo.pUserData = nullptr; // 自定义指针，直接传递到回调函数

		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
			reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
		if (vkCreateDebugUtilsMessenger)
		{
			if (vkCreateDebugUtilsMessenger(_instance, &debugMsgInfo, nullptr, &_debug_messenger) != VK_SUCCESS)
			{
				INVENT_LOG_ERROR(std::format("[VulkanBase] failed to set up debug messenger!"));
				return false;
			}
		}
		else
		{
			INVENT_LOG_ERROR(std::format("[VulkanBase] not found vkCreateDebugUtilsMessengerEXT"));
			return false;
		}
		return true;
	}
#endif // !VULKAN_VALITADION_LAYER

}