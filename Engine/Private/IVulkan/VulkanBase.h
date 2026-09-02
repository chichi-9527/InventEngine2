#pragma once

#include "EngineConfig.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <cstdint>
#include <vector>
#include <functional>

namespace INVENT
{
	class IVulkanBase
	{
		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR Capabilities = {};
			std::vector<VkSurfaceFormatKHR> Formats;
			std::vector<VkPresentModeKHR> PresentModes;
		};

		struct QueueFamilyIndices {
			uint32_t GraphicsFamily = 0;
			uint32_t PresentFamily = 0;
			uint32_t TransferFamily = 0;

			bool HasGraphicsFamily = false;
			bool HasPresentFamily = false;
			bool HasTransferFamily = false;

			bool IsComplete() const
			{
				return HasGraphicsFamily && HasPresentFamily;
			}
		};

		struct OffscreenPassResources {
			VkImage Image = VK_NULL_HANDLE;
			VkImageView View = VK_NULL_HANDLE;

			// 离屏渲染关卡
			VkImageView DepthView = VK_NULL_HANDLE;
			void* Level = nullptr;

			// vulkan >= 1.3
			VkFormat DynamicFormat = VK_FORMAT_UNDEFINED;
		};
	
		IVulkanBase() = default;
	public:
		using WaitForWindowEventsFunc = std::function<void()>;

	public:
		~IVulkanBase();

		static void AddValidationLayer(const char* layerName);
		static void AddInstanceExtension(const char* extensionName);
		static void AddDeviceExtension(const char* extensionName);

		static bool InitVmaAllocationCache();

		static IVulkanBase& Base();

		void SetWaitForWindowEventsFunction(WaitForWindowEventsFunc func) { _wait_for_window_events = func; }
		void SetSurface(VkSurfaceKHR surface) { if (!_surface) _surface = surface; }
		bool CreateVulkanInstance();
		bool PickPhysicalDevice();
		// 开启现代化渲染
		bool CreateLogicalDevice();
		bool CreateSwapChain();
		bool CreateSwapChainImageView();
		bool CreateVmaAllocator();
		bool FindDepthFormat();
		bool InitializeAllOffscreenPasses();
		bool InitDescriptorCounts();
		bool CreateBindlessDescriptorPool();
		bool CreateOtherDsecriptorPools();
		bool CreateGlobalPipelineLayout();
		bool AllocaGlobalBindlessDescriptorSet();
		bool CreateCommandPool();

		// tools

		/// <param name="descriptor_set"> 分配的描述符 (out) </param>
		/// <param name="layout_index"> 布局编号; 例如 ubo 在 layout 0 </param>
		/// <returns></returns>
		VkResult AllocateDescriptSetFromOtherDescriptorPools(VkDescriptorSet& descriptor_set, size_t layout_index = 0);
		enum class ModelBlendMode
		{
			Opaque,       // 不透明（開啟深度寫入，關閉混合）
			Masked,       // 鏤空測試（開啟深度寫入，關閉混合，Shader 內 discard）
			Translucent   // 透明（關閉深度寫入，開啟 Alpha Blending）
		};
		struct SpecializationData
		{
			int BlendMode = 0;   // 對應 Slang 中的 BLEND_MODE
		};
		struct GraphicsPipelineConfig
		{
			VkShaderModule VertexShader = VK_NULL_HANDLE;
			VkShaderModule FragmentShader = VK_NULL_HANDLE;

			ModelBlendMode BlendMode = ModelBlendMode::Opaque;
			SpecializationData SpecData = {};
			uint32_t SpecCount = 1;

			// vulkan >= 1.3
			VkFormat ColorAttachmentFormat = VK_FORMAT_UNDEFINED;
			VkFormat DepthAttachmentFormat = VK_FORMAT_UNDEFINED;

			VkBool32 EnableDepthTest = VK_TRUE;
			VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;

		};
		VkPipeline CreateGraphicsPipeline(const GraphicsPipelineConfig& config);
		VkShaderModule CreateShaderMoudle(const std::string& path);
		void DestroyShaderMoudle(VkShaderModule shader_moudle);
		void UpdateBindlessTextureSlot(uint32_t slot_id, VkImageView texture_image_view);
		bool CreateSyncObjects(std::vector<VkFence>& frameFence,
			std::vector<VkSemaphore>& acquireSemaphores,
			std::vector<VkSemaphore>& submitSemaphores);
		bool CreateCommandBuffers(std::vector<VkCommandBuffer>& buffers);
		bool ResizeBindlessDescriptorPoolAndGobalSet();
		VkCommandBuffer BeginSingleTimeCommands();
		void EndSingleTimeCommands(VkCommandBuffer command_buffer);

		// use vma
		/*
		* 0															純 GPU 存取
		* VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT	每影格更新
		* VMA_ALLOCATION_CREATE_MAPPED_BIT							一創建完就能直接用指針寫入資料
		* VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT				需要拉回 CPU 處理的場景
		*/

		void InsertVmaBufferCache(VkBuffer buffer, VmaAllocation allocation);
		void InsertVmaImageCache(VkImage image, VmaAllocation allocation);
		VmaAllocation GetFormBufferCache(VkBuffer buffer);
		VmaAllocation GetFormImageCache(VkImage image);
		void EraseBufferCache(VkBuffer buffer);
		void EraseImageCache(VkImage image);
		VkResult UseVmaCreateBuffer(VkDeviceSize size,
			VkBufferUsageFlags usage,
			VmaAllocationCreateFlags vma_flags,
			VkBuffer& buffer,
			void** out_mapped_data = nullptr);
		void UseVmaDestroyBuffer(VkBuffer buffer);
		VkResult UseVmaCreateImage(uint32_t width,
			uint32_t height,
			uint32_t mip_levels,
			VkFormat format,
			VkImageTiling tiling,
			VkImageUsageFlags usage,
			VmaAllocationCreateFlags vma_flags,
			VkImage& image,
			void** out_mapped_data = nullptr);
		void UseVmaDestroyImage(VkImage image);
		bool UseVmaFlushAllocationBuffer(VkBuffer buffer);
		bool UseVmaFlushAllocationImage(VkImage buffer);
		VkImageView CreateImageView(VkImage image,
			VkFormat format,
			VkImageAspectFlags aspect_flags,
			VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D,
			uint32_t mip_levels = 1,
			uint32_t base_array_layer = 0,
			uint32_t layer_count = 1);
		void DestroyImageView(VkImageView image_view);
		bool UseVmaMapMemory(VkBuffer buffer, void*& data);
		void UseVmaUnmapMemory(VkBuffer buffer);
		VmaAllocator GetVmaAllocator() const;

		VkQueue GetTransferQueue() const { return _transfer_queue; }
		VkFormat GetSwapChainImageFormat() const { return _swap_chain_image_format; }
		VkFormat GetDepthFormat() const { return _depth_format; }
		VkFormat GetShadowDepthFormat() const { return _shadow_depth_format; }
		VkInstance GetVkInstance() const { return _instance; }
		VkDevice GetDevice() const { return _device; }
		VkSwapchainKHR GetSwapChain() const { return _swap_chain; }
		const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return _physical_device_properties; }
		const VkPhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties() const { return _physical_device_memory_properties; }
		uint32_t GetCurrentBindlessDescriptorCount() const { return _current_descriptor_count; }
		VkPipelineLayout GetVkPipelineLayout() const { return _global_pipeline_layout; }
		const std::uint32_t GetAPIVersion() const noexcept { return _api_version; }
		bool Version_1_3_OrHigher() const noexcept { return _api_version >= VK_API_VERSION_1_3; }
	private:
		const std::uint32_t _use_lastest_api_version();
		QueueFamilyIndices _find_queue_families(VkPhysicalDevice device);
		SwapChainSupportDetails _query_swap_chain_support(VkPhysicalDevice device);
		bool _check_device_extension_support(VkPhysicalDevice device);
		void _get_all_properties();
		VkSurfaceFormatKHR _choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
		VkPresentModeKHR _choose_swap_presenta_mode(const std::vector<VkPresentModeKHR>& available_present_modes, VkPresentModeKHR mode = VK_PRESENT_MODE_MAILBOX_KHR);
		VkExtent2D _choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
		VkFormat _find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkDescriptorSetLayout _create_descriptor_set_layout(std::vector<VkDescriptorSetLayoutBinding>& bindings, bool is_bindless_set = false);

#ifdef VULKAN_VALITADION_LAYER
		bool _check_validation_layers();
		bool _setup_debug_messenger();
#endif // !VULKAN_VALITADION_LAYER
	private:
		WaitForWindowEventsFunc _wait_for_window_events = nullptr;

		VkInstance _instance = VK_NULL_HANDLE;
		VkDevice _device = VK_NULL_HANDLE;
		VkSurfaceKHR _surface = VK_NULL_HANDLE;
		VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
		VkQueue _graphics_queue = VK_NULL_HANDLE;
		VkQueue _present_queue = VK_NULL_HANDLE;
		VkQueue _transfer_queue = VK_NULL_HANDLE;
		VkSwapchainKHR _swap_chain = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
		VkDescriptorPool _bindless_descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorPool _other_descriptor_pool = VK_NULL_HANDLE;
		VkPipelineLayout _global_pipeline_layout = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> _descriptor_set_layouts;
		VkDescriptorSet  _global_bindless_descriptor_set = VK_NULL_HANDLE;
		VkCommandPool _command_pool = VK_NULL_HANDLE;
		VkCommandPool _transfer_command_pool = VK_NULL_HANDLE;

		VkPhysicalDeviceDescriptorIndexingProperties _descriptor_indexing_properties{};
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT _enabled_indexing_features{};
		VkPhysicalDeviceProperties _physical_device_properties{};
		VkPhysicalDeviceMemoryProperties _physical_device_memory_properties{};

		std::vector<VkImage> _swap_chain_images;
		std::vector<VkImageView> _swap_chain_image_views;
		std::vector<VkFramebuffer> _swap_chain_framebuffers;

		SwapChainSupportDetails _swap_chain_support{};
		QueueFamilyIndices _queue_family_indices{};
		VkExtent2D _swap_chain_extent{};

		VkFormat _swap_chain_image_format{ VK_FORMAT_UNDEFINED };
		VkImageUsageFlags _swap_chain_image_usages{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT };
		VkFormat _depth_format{ VK_FORMAT_UNDEFINED };
		VkFormat _shadow_depth_format{ VK_FORMAT_UNDEFINED };
		std::uint32_t _api_version{ VK_API_VERSION_1_0 };
		std::uint32_t _frame_buffer_width{ 0 };
		std::uint32_t _frame_buffer_height{ 0 };
		std::uint32_t _swap_chain_image_count{ 0 };
		std::uint32_t _current_descriptor_count{ 0 };
		std::uint32_t _absolute_descriptor_limit{ 0 };

		bool _framebuffer_resized = false;
	};

	
}
