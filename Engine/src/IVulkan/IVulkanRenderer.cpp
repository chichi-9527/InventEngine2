#include "IVulkan/IVulkanRenderer.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/IVulkanGlobalTexture.h"
#include "IVulkan/IVulkanGlobalVerticesIndices.h"
#include "IVulkan/IVulkanInstanceBuffer.h"
#include "IVulkan/IVulkanGlobalMaterial.h"
#include "ILog.h"
#include "IEngine.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <shared_mutex>

namespace IVulkan
{
	enum LightFlag : std::uint32_t
	{
		UseNone = 0,
		UseParallelLight = 1 << 0,
		UsePointLight = 1 << 1,
	};

	struct UBO
	{
		glm::mat4 VP{ 1.0f };
		glm::mat4 InvVP{ 1.0f };
		glm::vec4  cameraDatas; // 相机数据 [xyz]: 相机位置 [w]: LOD 系数
		glm::vec4 parallelLightDatas; // 灯光数据 [xyz]: 平行光方向 [w]: 平行光强度
		glm::vec4 parallelLightColor; // 平行光颜色
		std::uint32_t lightCount; // 灯光数量
		std::uint32_t UseLightFlags = LightFlag::UseNone; // 是否使用光源 LightFlag
	};

	struct PointLight
	{
		glm::vec4 position_radius{ 0.0f }; // [xyz] 位置 [w] 半径
		glm::vec4 color{ 1.0f };
		glm::vec4 attenuation{ 1.0f }; // [x] 衰减系数 [yzw] 预留(目前未使用)
	};

	struct ObjectIDGetBack 
	{
		glm::vec2 mouseCoords{ 0.0f };
		std::uint16_t idTextureSlot{ 0 };
		std::uint16_t depthTextureSlot{ 0 };
		std::uint32_t isTakeCS{ 0 }; // 0: 计算着色器中不会执行任何操作; 非0: 执行
		glm::vec3 wPosition{ 0.0f };
		std::uint32_t objectID{ 0 }; // 返回的id，0 为无效同时position也无效
	};
	static_assert(sizeof(ObjectIDGetBack) == 32);

}

namespace INVENT
{
	constexpr const char* TestQuadVSPath = "Config/Shaders/SPV/TestQuad.slang.vert.spv";
	constexpr const char* TestQuadFSPath = "Config/Shaders/SPV/TestQuad.slang.frag.spv";
	constexpr const char* TestFeedbackCSPath = "Config/Shaders/SPV/TestMipFeedback.slang.comp.spv";
	constexpr const char* TestTexturePath = "Config/EngineAssets/image/test.png.dds";

	constexpr const char* MainMeshVSPath = "Config/Shaders/SPV/MainMesh.vert.spv";
	constexpr const char* MainMeshFSPath = "Config/Shaders/SPV/MainMesh.frag.spv";
	constexpr const char* GeneralCSPath = "Config/Shaders/SPV/GeneralCompute.comp.spv";
	constexpr const char* ObjectIDGetCSPath = "Config/Shaders/SPV/ObjectIDGet.comp.spv";

	// 后处理 (默认什么都不做: 主场景离屏颜色原样输出)
	constexpr const char* PostProcessingVSPath = "Config/Shaders/SPV/PostProcessing.vert.spv";
	constexpr const char* PostProcessingFSPath = "Config/Shaders/SPV/PostProcessing.frag.spv";

	static IVulkan::PushConstants pushConstants{};

	struct DestroyRT
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
	};
	static std::array<std::vector<DestroyRT>, IVulkan::MAX_FRAMES_IN_FLIGHT + 1> FrameRTDestroyRing;
	static std::array<std::vector<VkPipeline>, IVulkan::MAX_FRAMES_IN_FLIGHT + 1> FramePipelineDestroyRing;
	static std::uint32_t _destroy_cursor = 0;
	struct MouseWorldPositionObjGetBackPackage {
		MouseWorldPositionObjGetBackFunc func = nullptr;
		EEngineThread target = EEngineThread::AnyThread;
	};
	static std::array<std::vector<MouseWorldPositionObjGetBackPackage>, IVulkan::MAX_FRAMES_IN_FLIGHT> FramePosOIDCallbacks;
	static std::shared_mutex PosOIDCallbacksMutex;

	bool IVulkanRenderer::Init()
	{
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE)
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] VulkanBase 未初始化!");
			return false;
		}

		// 管理器 (顺序: 纹理 -> 材质 -> 顶点索引 -> 实例)
		if (!IVulkanTexture2DManagement::Instance().Init() ||
			!IVulkanGlobalMaterial::Instance().Init() ||
			!IVulkanGlobalVerticesIndices::Instance().Init() ||
			!InstanceBuffer::Init())
		{
			return false;
		}

		// init cache map
		_image_barrier_cache_map = new VkImageBarrierCacheMap(64,
			std::hash<std::uint64_t>(),
			std::equal_to<std::uint64_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::uint64_t, VkImageBarrierCache>>(IEngineTools::Instance().GetMemPoolPool()));

		if (_get_current_mouse_position_func == nullptr)
		{
			INVENT_LOG_WARNING("[IVulkanRenderer] 未设置获取当前鼠标位置的函数!");
		}

		_mainRT_format = VK_FORMAT_B8G8R8A8_SRGB;
		if (!_use_imgui)
		{
			_mainRT_extent = base.GetSwapChainExtent();
		}
		_mainRT_extent = base.GetSwapChainExtent();
		_destroy_cursor = 0;

		if (!_init_buffers() ||
			!_init_descriptor_sets() ||
			!_init_frames() ||
			!_init_pipelines() ||
			!_init_test_resources())
		{
			return false;
		}

		_vk_clear.color = { 0.02f, 0.02f, 0.04f, 1.0f };

		INVENT_LOG_INFO("[IVulkanRenderer] init done.");
		return true;
	}

	void IVulkanRenderer::RegisterMouseWPositionObjGetFunc(EEngineThread target, MouseWorldPositionObjGetBackFunc func)
	{
		// 没有注册返回函数直接丢弃防止内存爆炸
		if (_return_world_position_obj_id_func == nullptr) return;
		std::unique_lock<std::shared_mutex> lock(PosOIDCallbacksMutex);
		FramePosOIDCallbacks[_current_frame].emplace_back(func, target);
	}

	bool IVulkanRenderer::_init_buffers()
	{
		auto& base = IVulkanBase::Base();

		// 每帧 buffers
		_ubo_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_point_light_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_draw_count_and_indirect_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_mouse_back_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);

		// point light
		VkDeviceSize pointLightSSBOSize = sizeof(IVulkan::PointLight) * IVulkan::MAX_POINT_LIGHT_COUNT;
		// binding3
		VkDeviceSize binding3Size = sizeof(std::uint32_t) * IVulkan::MAX_INSTANCE_BATCH_COUNT +
			sizeof(VkDrawIndirectCommand) * IVulkan::EXPECTED_INSTANCE_SIZE;
		VkBufferUsageFlags binding3UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		// 在渲染前需要用 vkCmdFillBuffer 將其清零加上 TRANSFER_DST 標記
		binding3UsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		
		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			// ubo
			if (VkResult r = base.UseVmaCreateBuffer(sizeof(IVulkan::UBO),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				_ubo_buffers[i].buffer, &_ubo_buffers[i].mapped))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] ubo 缓冲创建失败! VkResult: {}.", static_cast<std::int32_t>(r)));
				return false;
			}

			// binding1 light
			if (VkResult r = base.UseVmaCreateBuffer(pointLightSSBOSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				_point_light_buffers[i].buffer, &_point_light_buffers[i].mapped))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] ubo 缓冲创建失败! VkResult: {}.", static_cast<std::int32_t>(r)));
				return false;
			}

			// binding3
			if (VkResult r = base.UseVmaCreateBuffer(binding3Size,
				binding3UsageFlags,
				0,
				_draw_count_and_indirect_buffers[i].buffer, 
				nullptr))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] VkDrawIndirectCommand ssbo 创建失败!");
				return false;
			}

			// binding5: 鼠标悬停反馈 ObjectID (CPU 写入坐标, GPU 回写 ID)
			if (VkResult r = base.UseVmaCreateBuffer(sizeof(IVulkan::ObjectIDGetBack),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				_mouse_back_buffers[i].buffer,
				&_mouse_back_buffers[i].mapped))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] ObjectID ssbo 创建失败!");
				return false;
			}


		}

		return true;
	}

	bool IVulkanRenderer::_init_descriptor_sets()
	{
		auto& base = IVulkanBase::Base();

		// set0 布局的一个 set 包含全部 binding 0~4, 不能按 binding 拆成多个 set!
		//   每帧槽一个 set, binding 0~3 初始化时写一次, binding 4 每帧由 BindMipFeedback 写
		_set0.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (VkResult r = base.AllocateDescriptSetFromOtherDescriptorPools(_set0[i]))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] set0[{}] 分配失败! VkResult: {}.", i, static_cast<std::int32_t>(r)));
				return false;
			}
			_write_buffer_binding(_set0[i], 0, _ubo_buffers[i].buffer, true);		// ubo
			_write_buffer_binding(_set0[i], 1, _point_light_buffers[i].buffer, false);		// 灯光 
			_write_buffer_binding(_set0[i], 2, InstanceBuffer::GetBuffer(), false);	// 实例
			_write_buffer_binding(_set0[i], 3, _draw_count_and_indirect_buffers[i].buffer, false);		// 间接命令
			// binding 4: 每帧 BindMipFeedback 写
			_write_buffer_binding(_set0[i], 5, _mouse_back_buffers[i].buffer, false); // 
		}
		return true;
	}

	void IVulkanRenderer::_write_buffer_binding(VkDescriptorSet set, std::uint32_t binding, VkBuffer buffer, bool is_ubo)
	{
		VkDescriptorBufferInfo bi{};
		bi.buffer = buffer;
		bi.offset = 0;
		bi.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet w{};
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.dstSet = set;
		w.dstBinding = binding;
		w.descriptorType = is_ubo ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		w.descriptorCount = 1;
		w.pBufferInfo = &bi;

		vkUpdateDescriptorSets(IVulkanBase::Base().GetDevice(), 1, &w, 0, nullptr);
	}

	void IVulkanRenderer::_upload_per_frame_data(std::uint32_t frame_index)
	{
		auto& base = IVulkanBase::Base();
		auto& engine = IEngine::Instatnce();

		IVulkan::UBO ubo{};
		/////////////////////
		auto aspectRatio = engine.GetWindowAspectRatio();
		if (_use_imgui)
		{
			aspectRatio = _mainRT_extent.height ?
				static_cast<float>(_mainRT_extent.width) / static_cast<float>(_mainRT_extent.height) :
				0.0f;
		}
		glm::mat4 projectionMatrix{ 1.0f };
		if (aspectRatio != 0.0f) 
		{ 
			projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
		}
		glm::mat4 viewMatrix{ 1.0f };
		glm::vec3 cameraPos = { 0.0f,0.0f,2.0f };
		glm::vec3 cameraForward = { 0.0f,0.0f,-1.0f };
		glm::vec3 cameraUp = { 0.0f,1.0f,0.0f };
		viewMatrix = glm::lookAt(cameraPos, cameraPos + cameraForward, cameraUp);
		//////////////////////
		ubo.VP = projectionMatrix * viewMatrix;
		ubo.InvVP = glm::inverse(ubo.VP);
		ubo.cameraDatas = { cameraPos, 0.0f };
		if (auto height = engine.GetWindowHeight())
		{
			ubo.cameraDatas.w = static_cast<float>(height) / (2.0f * std::tan(glm::radians(45.0f) / 2.0f));
		}
		if (_use_imgui)
		{
			if (auto height = _mainRT_extent.height)
			{
				ubo.cameraDatas.w = static_cast<float>(height) / (2.0f * std::tan(glm::radians(45.0f) / 2.0f));
			}
		}

		memcpy(_ubo_buffers[frame_index].mapped, &ubo, sizeof(IVulkan::UBO));

		//IVulkan::PointLight lights[IVulkan::MAX_POINT_LIGHT_COUNT]{};
		// TODO: 从场景收集点光源数据
		//memcpy(_point_light_buffers[frame_index].mapped, lights, sizeof(lights));

		base.UseVmaFlushAllocationBuffer(_ubo_buffers[frame_index].buffer);
		//base.UseVmaFlushAllocationBuffer(_point_light_buffers[frame_index].buffer);

		// 鼠标指针位置数据获取
		IVulkan::ObjectIDGetBack back{};
		if (_get_current_mouse_position_func)
		{
			auto mPosition = _get_current_mouse_position_func();
			//INVENT_LOG_DEBUG(std::format("[IVulkanRenderer] _get_current_mouse_position_func() x: {}. y:{}.", mPosition.x, mPosition.y));
			back.mouseCoords = { mPosition.x, mPosition.y };
			if (mPosition.x > 0.0f && mPosition.y > 0.0f)
				back.isTakeCS = 1;
		}
		back.idTextureSlot = static_cast<std::uint16_t>(_object_id_RTs[frame_index].bindlessSlot);
		back.depthTextureSlot = static_cast<std::uint16_t>(_main_depth_RT.bindlessSlot);

		memcpy(_mouse_back_buffers[frame_index].mapped, &back, sizeof(IVulkan::ObjectIDGetBack));
		base.UseVmaFlushAllocationBuffer(_mouse_back_buffers[frame_index].buffer);
	}

	void IVulkanRenderer::_read_and_callback_mouseback(std::uint32_t frame_index)
	{
		std::unique_lock<std::shared_mutex> lock(PosOIDCallbacksMutex);

		auto& callbacks = FramePosOIDCallbacks[frame_index];
		auto& mBuffer = _mouse_back_buffers[frame_index];

		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;
		if (mBuffer.mapped == nullptr) return;
		if (callbacks.empty()) return;
		if (_return_world_position_obj_id_func == nullptr) return;

		IVulkanBase::Base().UseVmaInvalidateAllocationBuffer(mBuffer.buffer);

		IVulkan::ObjectIDGetBack* back = reinterpret_cast<IVulkan::ObjectIDGetBack*>(mBuffer.mapped);

		for (auto& callback : callbacks)
		{
			_return_world_position_obj_id_func(callback.target, callback.func,
				{ back->wPosition.x, back->wPosition.y, back->wPosition.z },
				back->objectID);
		}

		callbacks.clear();
	}

	bool IVulkanRenderer::_recreate_swapchain()
	{
		auto& base = IVulkanBase::Base();
		auto device = base.GetDevice();

		for (auto image : base.GetSwapChainImages())
		{
			_image_barrier_cache_map->erase(reinterpret_cast<std::uint64_t>(image));
		}
		if (!base.RecreateSwapChain())
		{
			return false;
		}
		for (auto f : _frame_fences)
			if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
		for (auto s : _acquire_semaphores)
			if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
		for (auto s : _submit_semaphores)
			if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
		_frame_fences.clear(); _acquire_semaphores.clear(); _submit_semaphores.clear();

		if (!base.CreateSyncObjects(_frame_fences, _acquire_semaphores, _submit_semaphores))
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] 同步对象重建失败!");
			return false;
		}

		if (!_use_imgui)
		{
			_mainRT_extent = base.GetSwapChainExtent();
			_main_rt_extent_dirty = true;
		}

		// 在 UseVmaCreateImage 成功后立即:
		VkImageBarrierCache init{};
		init.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		init.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		init.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
		init.currentAccessMask = 0;
		for (auto image : base.GetSwapChainImages())
		{
			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(image)] = init;
		}

		// 交换链深度图 (直接渲染到交换链时使用) 也要登记, aspect 必须是 DEPTH
		VkImageBarrierCache depthInit{};
		depthInit.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		depthInit.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthInit.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
		depthInit.currentAccessMask = 0;
		if (base.GetDepthImage() != VK_NULL_HANDLE)
		{
			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(base.GetDepthImage())] = depthInit;
		}

		return true;
	}

	bool IVulkanRenderer::_recreate_mainRTs()
	{
		auto& base = IVulkanBase::Base();

		VkImageBarrierCache ColorImageInit{};
		ColorImageInit.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		ColorImageInit.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ColorImageInit.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
		ColorImageInit.currentAccessMask = 0;

		VkImageUsageFlags rtUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		for (std::size_t i = 0; i < _mainRTs.size(); ++i)
		{
			auto& rt = _mainRTs[i];
			if (rt.imageView != VK_NULL_HANDLE || rt.image != VK_NULL_HANDLE)
			{
				FrameRTDestroyRing[_destroy_cursor].push_back({ rt.image, rt.imageView });
			}

			if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
				1,
				_mainRT_format,
				VK_IMAGE_TILING_OPTIMAL,
				rtUsage,
				0,
				rt.image))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] main rt image create error! result: {}.", static_cast<int>(res)));
				return false;
			}

			rt.imageView = base.CreateImageView(
				rt.image,
				_mainRT_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
			if (rt.imageView == VK_NULL_HANDLE)
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] main rt image view create error! result: {}.");
				return false;
			}

			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.image)] = ColorImageInit;

			// 重新绑定 bindless 槽位 (槽位不变, 只是 view 换了; 后处理通过它采样主场景颜色)
			if (i < _mainRT_bindless_slots.size())
			{
				IVulkanTexture2DManagement::Instance().UpdateTexture2D({ _mainRT_bindless_slots[i] }, rt.imageView);
			}
		}

		// 后处理离屏 RT: 与主场景离屏 RT 同大小 / 同格式
		for (auto& rt : postProcessingRTs)
		{
			if (rt.imageView != VK_NULL_HANDLE || rt.image != VK_NULL_HANDLE)
			{
				FrameRTDestroyRing[_destroy_cursor].push_back({ rt.image, rt.imageView });
			}

			if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
				1,
				_mainRT_format,
				VK_IMAGE_TILING_OPTIMAL,
				rtUsage,
				0,
				rt.image))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] post processing rt image create error! result: {}.", static_cast<int>(res)));
				return false;
			}

			rt.imageView = base.CreateImageView(
				rt.image,
				_mainRT_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
			if (rt.imageView == VK_NULL_HANDLE)
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] post processing rt image view create error!");
				return false;
			}

			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.image)] = ColorImageInit;
		}
		if (_main_depth_RT.sRT.imageView != VK_NULL_HANDLE || _main_depth_RT.sRT.image != VK_NULL_HANDLE)
		{
			FrameRTDestroyRing[_destroy_cursor].push_back({ _main_depth_RT.sRT.image, _main_depth_RT.sRT.imageView });
		}

		VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
			1,
			base.GetDepthFormat(),
			VK_IMAGE_TILING_OPTIMAL,
			depthUsage,
			0,
			_main_depth_RT.sRT.image))
		{
			INVENT_LOG_ERROR(std::format("[IVulkanRenderer] main depth rt image create error! result: {}.", static_cast<int>(res)));
			return false;
		}
		_main_depth_RT.sRT.imageView = base.CreateImageView(
			_main_depth_RT.sRT.image,
			base.GetDepthFormat(),
			VK_IMAGE_ASPECT_DEPTH_BIT);
		if (_main_depth_RT.sRT.imageView == VK_NULL_HANDLE)
		{
			INVENT_LOG_ERROR("[IVulkanRenderer] main rt image view create error! result: {}.");
			return false;
		}

		IVulkanTexture2DManagement::Instance().UpdateTexture2D({ _main_depth_RT.bindlessSlot }, _main_depth_RT.sRT.imageView);

		VkImageBarrierCache DepthImageInit{};
		DepthImageInit.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		DepthImageInit.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		DepthImageInit.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
		DepthImageInit.currentAccessMask = 0;
		(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(_main_depth_RT.sRT.image)] = DepthImageInit;

		// 重建 U32 ObjectID RT
		auto cmd = base.BeginSingleTimeCommands();
		for (auto& rt : _object_id_RTs)
		{
			if (rt.sRT.imageView != VK_NULL_HANDLE || rt.sRT.image != VK_NULL_HANDLE)
			{
				FrameRTDestroyRing[_destroy_cursor].push_back({ rt.sRT.image, rt.sRT.imageView });
			}
			VkImageUsageFlags idUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
				1,
				VK_FORMAT_R32_UINT,
				VK_IMAGE_TILING_OPTIMAL,
				idUsage,
				0,
				rt.sRT.image))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] object id rt recreate error! result: {}.", static_cast<int>(res)));
				return false;
			}
			rt.sRT.imageView = base.CreateImageView(rt.sRT.image, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT);
			if (rt.sRT.imageView == VK_NULL_HANDLE)
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] object id rt image view recreate error!");
				return false;
			}
			IVulkanTexture2DManagement::Instance().UpdateTexture2D({ rt.bindlessSlot }, rt.sRT.imageView);

			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.sRT.image)] = ColorImageInit;

			_record_image_barrier_auto(cmd, rt.sRT.image,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_READ_BIT);
			
		}

		base.EndSingleTimeCommands(cmd);

		return true;
	}

	bool IVulkanRenderer::_is_pipeline_ok(IRendererPipelineType pipeline, const IRendererPipelineType* pre, std::uint32_t pre_count)
	{
		if (_pipelines[pipeline] == VK_NULL_HANDLE)
			return false;
		if (pre == nullptr || pre_count == 0) return true;

		for (std::uint32_t i = 0; i < pre_count; ++i)
		{
			if (_pipelines[pre[i]] == VK_NULL_HANDLE) return false;
		}
		return true;
	}

	void IVulkanRenderer::_update_pipelines()
	{
		std::vector<std::pair<IRendererPipelineType, VkPipeline>> pending;
		{
			std::lock_guard<std::mutex> lock(_pipelines_updates_mutex);
			if (_pipelines_updates.empty()) return;
			pending.swap(_pipelines_updates);
		}
		
		for (auto& [type, pipeline] : pending)
		{
			auto& currentP = _pipelines[type];
			if (currentP != pipeline)
			{
				if (currentP != VK_NULL_HANDLE)
					FramePipelineDestroyRing[_destroy_cursor].push_back(currentP);
				currentP = pipeline;
			}
		}

		// judge main renderer ready
		IRendererPipelineType mainNeed[] = { 
			IRendererPipelineType::PipelineComputeGeneral,
			IRendererPipelineType::PipelineComputeReturnObjectID
		};
		if (_is_pipeline_ok(IRendererPipelineType::PipelineMainOpaque,
			mainNeed,
			sizeof(mainNeed) / sizeof(IRendererPipelineType)))
		{
			_is_main_renderer_ready = 1;

			// judge post processing ready
			if (_is_pipeline_ok(IRendererPipelineType::PipelinePostProcessing, nullptr, 0))
			{
				_is_post_processing_renderer_ready = 1;
			}
			else
			{
				_is_post_processing_renderer_ready = 0;
			}
		}
		else
		{
			_is_main_renderer_ready = 0;
		}

	}

	bool IVulkanRenderer::_flush_destroy_queue()
	{
		_destroy_cursor = (_destroy_cursor + 1) % (IVulkan::MAX_FRAMES_IN_FLIGHT + 1);
		auto& items = FrameRTDestroyRing[_destroy_cursor];	// 恰好 N+1 帧前入队的资源
		for (auto& it : items)
		{
			if (it.imageView != VK_NULL_HANDLE)
				IVulkanBase::Base().DestroyImageView(it.imageView);
			if (it.image != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyImage(it.image);
				_image_barrier_cache_map->erase(reinterpret_cast<std::uint64_t>(it.image));
			}
		}
		items.clear();

		auto device = IVulkanBase::Base().GetDevice();
		auto& pipelines = FramePipelineDestroyRing[_destroy_cursor];
		for (auto& p : pipelines)
		{
			if (p != VK_NULL_HANDLE) vkDestroyPipeline(device, p, nullptr);
		}
		pipelines.clear();
		return true;
	}

	bool IVulkanRenderer::_init_frames()
	{
		auto& base = IVulkanBase::Base();
		if (!base.CreateSyncObjects(_frame_fences, _acquire_semaphores, _submit_semaphores))
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] CreateSyncObjects failed!");
			return false;
		}
		if (!base.CreateCommandBuffers(_command_buffers))
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] CreateCommandBuffers failed!");
			return false;
		}
		return true;
	}

	bool IVulkanRenderer::_init_pipelines()
	{
		auto& base = IVulkanBase::Base();
		auto vs = base.CreateShaderMoudle(MainMeshVSPath);
		auto fs = base.CreateShaderMoudle(MainMeshFSPath);
		auto cs = base.CreateShaderMoudle(GeneralCSPath);
		if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE || cs == VK_NULL_HANDLE)
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] 测试 shader 加载失败!");
			return false;
		}

		VkImageBarrierCache ColorImageInit{};
		ColorImageInit.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		ColorImageInit.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ColorImageInit.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
		ColorImageInit.currentAccessMask = 0;
		// swapchain cache
		for (auto image : base.GetSwapChainImages())
			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(image)] = ColorImageInit;

		// 图形管线
		// main
		if (_mainRT_format != base.GetSwapChainImageFormat())
		{
			INVENT_LOG_WARNING(std::format("[IVulkanRenderer] 主场景离屏格式 ({}) 与交换链格式 ({}) 不一致, 同一图形管线同时用于离屏与直接上屏可能触发校验错误.",
				static_cast<std::uint32_t>(_mainRT_format),
				static_cast<std::uint32_t>(base.GetSwapChainImageFormat())));
		}
		_mainRTs.resize(static_cast<size_t>(IVulkan::MAX_FRAMES_IN_FLIGHT));
		_mainRT_bindless_slots.resize(static_cast<size_t>(IVulkan::MAX_FRAMES_IN_FLIGHT), UINT32_MAX);
		_object_id_RTs.resize(static_cast<size_t>(IVulkan::MAX_FRAMES_IN_FLIGHT));
		postProcessingRTs.resize(static_cast<size_t>(IVulkan::MAX_FRAMES_IN_FLIGHT));
		if (_mainRT_extent.width != 0 && _mainRT_extent.height != 0)
		{
			VkImageUsageFlags rtUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			for (std::size_t i = 0; i < _mainRTs.size(); ++i)
			{
				auto& rt = _mainRTs[i];
				if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
					1,
					_mainRT_format,
					VK_IMAGE_TILING_OPTIMAL,
					rtUsage,
					0,
					rt.image))
				{
					INVENT_LOG_ERROR(std::format("[IVulkanRenderer] main rt image create error! result: {}.", static_cast<int>(res)));
					return false;
				}

				rt.imageView = base.CreateImageView(
					rt.image,
					_mainRT_format,
					VK_IMAGE_ASPECT_COLOR_BIT);
				if (rt.imageView == VK_NULL_HANDLE)
				{
					INVENT_LOG_ERROR("[IVulkanRenderer] main rt image view create error! result: {}.");
					return false;
				}

				(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.image)] = ColorImageInit;

				// 注册到 bindless (外部渲染目标槽位): 后处理通过该槽位采样主场景离屏颜色
				_mainRT_bindless_slots[i] = IVulkanTexture2DManagement::Instance().AllocateTextureHandle().slot;
				IVulkanTexture2DManagement::Instance().UpdateTexture2D({ _mainRT_bindless_slots[i] }, rt.imageView);
			}

			// 后处理离屏 RT: 与主场景离屏 RT 同大小 / 同格式 (使用 imgui 时后处理结果写到这里供 imgui 采样)
			for (auto& rt : postProcessingRTs)
			{
				if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
					1,
					_mainRT_format,
					VK_IMAGE_TILING_OPTIMAL,
					rtUsage,
					0,
					rt.image))
				{
					INVENT_LOG_ERROR(std::format("[IVulkanRenderer] post processing rt image create error! result: {}.", static_cast<int>(res)));
					return false;
				}

				rt.imageView = base.CreateImageView(
					rt.image,
					_mainRT_format,
					VK_IMAGE_ASPECT_COLOR_BIT);
				if (rt.imageView == VK_NULL_HANDLE)
				{
					INVENT_LOG_ERROR("[IVulkanRenderer] post processing rt image view create error!");
					return false;
				}

				(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.image)] = ColorImageInit;
			}

			// U32 ObjectID 渲染目标 (绑定到 bindless 供 CS 通过 slot 查找)
			VkImageUsageFlags idUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			auto cmd = base.BeginSingleTimeCommands();
			for (auto& rt : _object_id_RTs)
			{
				if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
					1,
					VK_FORMAT_R32_UINT,
					VK_IMAGE_TILING_OPTIMAL,
					idUsage,
					0,
					rt.sRT.image))
				{
					INVENT_LOG_ERROR(std::format("[IVulkanRenderer] object id rt image create error! result: {}.", static_cast<int>(res)));
					return false;
				}

				rt.sRT.imageView = base.CreateImageView(
					rt.sRT.image,
					VK_FORMAT_R32_UINT,
					VK_IMAGE_ASPECT_COLOR_BIT);
				if (rt.sRT.imageView == VK_NULL_HANDLE)
				{
					INVENT_LOG_ERROR("[IVulkanRenderer] object id rt image view create error!");
					return false;
				}

				// 注册到全局 bindless 纹理数组
				rt.bindlessSlot = IVulkanTexture2DManagement::Instance().AllocateTextureHandle().slot;
				IVulkanTexture2DManagement::Instance().UpdateTexture2D({ rt.bindlessSlot }, rt.sRT.imageView);

				(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(rt.sRT.image)] = ColorImageInit;

				_record_image_barrier_auto(cmd, rt.sRT.image,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					VK_ACCESS_2_SHADER_READ_BIT);
				
			}
			base.EndSingleTimeCommands(cmd);

			VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			if (VkResult res = base.UseVmaCreateImage(_mainRT_extent.width, _mainRT_extent.height,
				1,
				base.GetDepthFormat(),
				VK_IMAGE_TILING_OPTIMAL,
				depthUsage,
				0,
				_main_depth_RT.sRT.image))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanRenderer] main depth rt image create error! result: {}.", static_cast<int>(res)));
				return false;
			}
			_main_depth_RT.sRT.imageView = base.CreateImageView(
				_main_depth_RT.sRT.image,
				base.GetDepthFormat(),
				VK_IMAGE_ASPECT_DEPTH_BIT);
			if (_main_depth_RT.sRT.imageView == VK_NULL_HANDLE)
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] main rt image view create error! result: {}.");
				return false;
			}
			_main_depth_RT.bindlessSlot = IVulkanTexture2DManagement::Instance().AllocateTextureHandle().slot;
			IVulkanTexture2DManagement::Instance().UpdateTexture2D({ _main_depth_RT.bindlessSlot }, _main_depth_RT.sRT.imageView);
			// _main_depth_RT
			VkImageBarrierCache DepthInit{};
			DepthInit.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
			DepthInit.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			DepthInit.currentStageMask = VK_PIPELINE_STAGE_2_NONE;
			DepthInit.currentAccessMask = 0;
			(*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(_main_depth_RT.sRT.image)] = DepthInit;
		}

		IVulkanBase::GraphicsPipelineConfig cfg{};
		cfg.VertexShader = vs;
		cfg.FragmentShader = fs;
		cfg.ColorAttachmentFormat = base.GetSwapChainImageFormat();
		//INVENT_LOG_DEBUG(std::format("[IVulkanRenderer] base.GetSwapChainImageFormat(): {}.", static_cast<uint32_t>(base.GetSwapChainImageFormat())));
		cfg.SecondColorAttachmentFormat = VK_FORMAT_R32_UINT;
		cfg.DepthAttachmentFormat = base.GetDepthFormat();
		cfg.EnableDepthTest = VK_TRUE;
		cfg.CullMode = VK_CULL_MODE_NONE;
		auto mainOpaquePipeline = base.CreateGraphicsPipeline(cfg);
		if (mainOpaquePipeline == VK_NULL_HANDLE) return false;
		UpdatePipeline(IRendererPipelineType::PipelineMainOpaque, mainOpaquePipeline);

		// 反馈计算管线 (全局 layout: set0)
		auto computeGeneralPipeline = base.CreateComputePipeline(cs);
		if (computeGeneralPipeline == VK_NULL_HANDLE) return false;
		UpdatePipeline(IRendererPipelineType::PipelineComputeGeneral, computeGeneralPipeline);

		// ObjectID 拾取计算管线
		{
			auto csObj = base.CreateShaderMoudle(ObjectIDGetCSPath);
			if (csObj == VK_NULL_HANDLE)
			{
				INVENT_LOG_FATAL("[IVulkanRenderer] ObjectID 计算着色器加载失败!");
				return false;
			}
			auto ObjectIDComputePipeline = base.CreateComputePipeline(csObj);
			base.DestroyShaderMoudle(csObj);
			if (ObjectIDComputePipeline == VK_NULL_HANDLE) return false;
			UpdatePipeline(IRendererPipelineType::PipelineComputeReturnObjectID, ObjectIDComputePipeline);
		}

		// post process 后期处理管线
		{
			auto postVS = base.CreateShaderMoudle(PostProcessingVSPath);
			auto postFS = base.CreateShaderMoudle(PostProcessingFSPath);
			if (postVS == VK_NULL_HANDLE || postFS == VK_NULL_HANDLE)
			{
				// 后处理 shader 还没编译: 不阻塞启动, 主场景会按 "无后处理" 的路径渲染
				INVENT_LOG_WARNING(std::format("[IVulkanRenderer] 后处理 shader 加载失败 ({} / {}), 后处理管线关闭.",
					PostProcessingVSPath, PostProcessingFSPath));
			}
			else
			{
				IVulkanBase::GraphicsPipelineConfig postCfg{};
				postCfg.VertexShader = postVS;
				postCfg.FragmentShader = postFS;
				// 后处理输出格式与主场景离屏 RT 一致 (写入交换链时也要求格式一致)
				postCfg.ColorAttachmentFormat = _mainRT_format;
				postCfg.SecondColorAttachmentFormat = VK_FORMAT_UNDEFINED;
				postCfg.DepthAttachmentFormat = VK_FORMAT_UNDEFINED;
				postCfg.EnableDepthTest = VK_FALSE;		// 后处理不需要深度
				postCfg.CullMode = VK_CULL_MODE_NONE;	// 全屏三角形不做剔除
				postCfg.BlendMode = IVulkanBase::ModelBlendMode::Opaque;

				auto postPipeline = base.CreateGraphicsPipeline(postCfg);
				if (postPipeline == VK_NULL_HANDLE)
				{
					INVENT_LOG_WARNING("[IVulkanRenderer] 后处理图形管线创建失败, 后处理管线关闭.");
				}
				else
				{
					UpdatePipeline(IRendererPipelineType::PipelinePostProcessing, postPipeline);
				}
			}
			if (postVS != VK_NULL_HANDLE) base.DestroyShaderMoudle(postVS);
			if (postFS != VK_NULL_HANDLE) base.DestroyShaderMoudle(postFS);
		}

		base.DestroyShaderMoudle(vs);
		base.DestroyShaderMoudle(fs);
		base.DestroyShaderMoudle(cs);
		return true;
	}

	bool IVulkanRenderer::_init_test_resources()
	{
		auto& base = IVulkanBase::Base();
		auto& texMgmt = IVulkanTexture2DManagement::Instance();

		// 全局采样器: maxLod 必须不钳制 (流式窗口贴图的高层必须可采)
		VkSamplerCreateInfo sci{};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.maxLod = VK_LOD_CLAMP_NONE;
		sci.anisotropyEnable = VK_TRUE;
		sci.maxAnisotropy = base.GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
		if (vkCreateSampler(base.GetDevice(), &sci, nullptr, &_global_sampler))
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] 全局采样器创建失败!");
			return false;
		}

		// set1 binding 0 写入采样器 (一次)
		VkDescriptorImageInfo ii{};
		ii.sampler = _global_sampler;
		VkWriteDescriptorSet sw{};
		sw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		sw.dstSet = base.GetGlobalBindlessDescriptorSet();
		sw.dstBinding = 0;
		sw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		sw.descriptorCount = 1;
		sw.pImageInfo = &ii;
		vkUpdateDescriptorSets(base.GetDevice(), 1, &sw, 0, nullptr);

		// 测试纹理 (渲染线程调用; pin: 反馈回路冷启动期间不受驱逐统计影响)
		auto h = texMgmt.AddTexture2D("test_quad_texture", TestTexturePath,
			IVulkanTexture2DManagement::TextureType::TYPE_Deffuse);
		
		if (!h.IsValid())
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] 测试纹理分配失败!");
			return false;
		}
		texMgmt.SetTexturePinned(h, true);
		_test_texture_slot = h.slot;

		INVENT_LOG_INFO(std::format("[IVulkanRenderer] test texture slot: {}.", h.slot));

		// test insatane / material
		auto gviHandle = IVulkanGlobalVerticesIndices::Instance().GetDefVertexIndexData(IVulkanGlobalVerticesIndices::DefMeshVertHandle::DefQuad);
		Material testMaterial{};
		testMaterial.DiffuseTextureId = static_cast<uint16_t>(_test_texture_slot);
		testMaterial.textureReadyFlags = IMaterialTextureHasFlag::HasBitDeffuse;
		auto mHandle = IVulkanGlobalMaterial::Instance().AddMaterial(testMaterial);
		InstanceBuffer::InstanceData testData{};
		testData.Model = { 1.0f };
		testData.VertexAddress = gviHandle.vertexAddress;
		testData.IndexAddress = gviHandle.indexAddress;
		testData.IndexCount = gviHandle.indexCount;
		testData.MaterialAddress = IVulkanGlobalMaterial::Instance().GetMaterialAddress(mHandle);
		testData.AABBExtent = { 0.5f,0.5f,0.5f,0.0f };
		testData.ObjectID = 2;
		testData.LevelID = 0;
		testData.InstanceOptionFlags |= InstanceBuffer::InstanceOptionFlag::ShowInstance;
		if (gviHandle.use32BitIndex)
		{
			testData.InstanceOptionFlags |= InstanceBuffer::InstanceOptionFlag::Use32BitIndex;
		}
		auto instanceHandle = InstanceBuffer::AddInstanceData(testData);
		
		return true;
	}

	void IVulkanRenderer::BeginRender(std::uint32_t frameindex)
	{
		auto& engine = IEngine::Instatnce();
		auto& base = IVulkanBase::Base();
		auto device = base.GetDevice();
		auto& texMgmt = IVulkanTexture2DManagement::Instance();

		_current_frame = frameindex;
		_frame_skipped = false;

		if (_swapchain_dirty)
		{
			if (std::chrono::steady_clock::now() < _recreate_cooldown_until) { _frame_skipped = true; return; }
			if (!_recreate_swapchain())
			{
				_frame_skipped = true;
				_recreate_cooldown_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
				return;
			}
			_swapchain_dirty = false;
		}
		_flush_destroy_queue();
		if (_main_rt_extent_dirty)
		{
			if (std::chrono::steady_clock::now() < _recreate_cooldown_until) { _frame_skipped = true; return; }
			if (!_recreate_mainRTs())
			{
				_frame_skipped = true;
				_recreate_cooldown_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
				return;
			}
			_main_rt_extent_dirty = false;
		}

		// 1) 帧同步
		vkWaitForFences(device, 1, &_frame_fences[frameindex], VK_TRUE, UINT64_MAX);

		_update_pipelines();

		// 回读
		_read_and_callback_mouseback(frameindex);

		// 2) 流式三连 (fence 已等, readback 安全); FlushDestroyQueue 每帧恰好一次
		texMgmt.ProcessMipFeedback(frameindex);
		texMgmt.ProcessCompletedUploads();
		texMgmt.FlushDestroyQueue();

		// 3) 获取交换链图像
		VkSwapchainKHR swapchain = base.GetSwapChain();
		VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
			_acquire_semaphores[frameindex], VK_NULL_HANDLE, &_image_index);
		if (acq == VK_ERROR_OUT_OF_DATE_KHR)
		{
			_swapchain_dirty = true;
			INVENT_LOG_INFO("[IVulkanRenderer] recreate swap chain reason: VK_ERROR_OUT_OF_DATE_KHR");
			_frame_skipped = true;
			return;
		}
		if (acq == VK_SUBOPTIMAL_KHR)
		{
			_swapchain_dirty = true;	// acquire 已成功, 信号量会被本帧 submit 消费掉 -> 照常渲染
			INVENT_LOG_INFO("[IVulkanRenderer] recreate swap chain reason: VK_SUBOPTIMAL_KHR");
		}
		vkResetFences(device, 1, &_frame_fences[frameindex]);

		// imgui
		if (_use_imgui)
		{
			_imgui_new_frame_func();
		}

		// 
		_upload_per_frame_data(frameindex);

		// 4) set0 binding 4: 本帧反馈缓冲
		texMgmt.BindMipFeedback(_set0[frameindex], frameindex);

		// 5) 开始录制
		VkCommandBuffer cmd = _command_buffers[frameindex];
		vkResetCommandBuffer(cmd, 0);
		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &bi);
	}

	void IVulkanRenderer::RenderFrame(std::uint32_t frameindex)
	{
		if (_frame_skipped) return;

		auto& base = IVulkanBase::Base();
		auto& texMgmt = IVulkanTexture2DManagement::Instance();
		VkCommandBuffer cmd = _command_buffers[frameindex];
		VkImage swapImage = base.GetSwapChainImages()[_image_index];

		// ===== 主场景渲染目标选择 =====
		//   _use_imgui (编辑器)                 -> 离屏: 场景写 _mainRTs, 之后由 imgui 采样显示
		//   _is_post_processing_renderer_ready  -> 离屏: 场景写 _mainRTs, 之后由后处理采样
		//   两者都不成立                        -> 直接渲染到交换链图像 (省掉一次全屏拷贝)
		const bool scene_offscreen = (_use_imgui || _is_post_processing_renderer_ready != 0);
		const VkExtent2D scene_extent = scene_offscreen ? _mainRT_extent : base.GetSwapChainExtent();
		const VkImage scene_color_image = scene_offscreen ? _mainRTs[frameindex].image : swapImage;
		const VkImageView scene_color_view = scene_offscreen ? _mainRTs[frameindex].imageView : base.GetSwapChainImageViews()[_image_index];
		// 深度统一使用离屏深度 RT: 拾取 CS 通过 bindless slot 采样它,
		// 非 imgui 模式下 _mainRT_extent 会随交换链尺寸同步, 因此与交换链颜色附件尺寸一致
		const VkImage scene_depth_image = _main_depth_RT.sRT.image;
		const VkImageView scene_depth_view = _main_depth_RT.sRT.imageView;

		// 1) 新上传窗口 image 的所有权获取 (transfer -> graphics), 必须在所有采样之前
		texMgmt.RecordAcquireBarriers(cmd);

		// 2) 主场景颜色目标: UNDEFINED -> COLOR_ATTACHMENT (srcStage 与提交时的 acquire 等待阶段对齐)
		_record_image_barrier_auto(cmd, scene_color_image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		// 3) 主场景深度: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		_record_image_barrier_auto(cmd, scene_depth_image,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		// 4) U32 ObjectID RT: SHADER -> COLOR_ATTACHMENT_OPTIMAL
		_record_image_barrier_auto(cmd, _object_id_RTs[frameindex].sRT.image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		// 脏材质上传 + 读屏障
		IVulkanGlobalMaterial::Instance().FlushDirtyMaterials(cmd, frameindex);
		// 实例刷新
		InstanceBuffer::FlushBuffer();

		// 5) mip 反馈 CS
		vkCmdFillBuffer(cmd, _draw_count_and_indirect_buffers[frameindex].buffer,
			0, 
			sizeof(uint32_t) * IVulkan::MAX_INSTANCE_BATCH_COUNT,
			0);	// 将shader中渲染原子计数器置0
		texMgmt.ResetMipFeedback(cmd, frameindex);		// fill 0xFF + transfer->compute 屏障
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
			base.GetVkPipelineLayout(), 0, 1, &_set0[frameindex], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
			_pipelines[IRendererPipelineType::PipelineComputeGeneral]);
		// 传入 Push Constants
		pushConstants.BatchID = 0;
		pushConstants.DrawObjectIndexOffset = 0;
		vkCmdPushConstants(cmd,
			base.GetVkPipelineLayout(),
			base.GetPushConstantStageFlags(),
			0,
			sizeof(IVulkan::PushConstants),
			&pushConstants);
		// 
		auto instanceNeedCount = InstanceBuffer::GetUsedHighWaterMark(0);
		//INVENT_LOG_DEBUG(std::format("[renderer debug] instanceNeedCount: {}.", instanceNeedCount));
		vkCmdDispatch(cmd, (instanceNeedCount + 63) / 64, 1, 1);
		texMgmt.RecordMipFeedbackCopy(cmd, frameindex);	// compute->transfer 屏障 + 拷贝 readback

		// CS → Draw Indirect 屏障：确保 compute shader 写入的间接命令对绘制可见
		{
			VkMemoryBarrier2 mb{};
			mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
			mb.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			mb.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
			mb.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
			mb.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

			VkDependencyInfo di{};
			di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			di.memoryBarrierCount = 1;
			di.pMemoryBarriers = &mb;
			vkCmdPipelineBarrier2(cmd, &di);
		}

		// 6) 主场景渲染
		//    - 离屏: 渲染到 _mainRTs[frameindex] (imgui 或 后处理需要)
		//    - 直接: 渲染到交换链图像
		if (_is_main_renderer_ready != 0)
		{
			VkDescriptorSet sets[2] = { _set0[frameindex], base.GetGlobalBindlessDescriptorSet() };

			// ---- 6.1 主场景几何 pass (动态渲染) ----
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				_pipelines[IRendererPipelineType::PipelineMainOpaque]);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				base.GetVkPipelineLayout(), 0, 2, sets, 0, nullptr);

			VkViewport viewport{};
			viewport.width = static_cast<float>(scene_extent.width);
			viewport.height = static_cast<float>(scene_extent.height);
			viewport.maxDepth = 1.0f;
			VkRect2D scissor{ {0, 0}, scene_extent };
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			VkRenderingAttachmentInfo color{};
			color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			color.imageView = scene_color_view;
			color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color.clearValue = _vk_clear;

			VkRenderingAttachmentInfo depth{};
			depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depth.imageView = scene_depth_view;
			depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // 清除深度缓冲区
			depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	  // 深度需保留供后续 CS 采样
			depth.clearValue.depthStencil = { 1.0f, 0 };	  // 默认深度设为 1.0

			// U32 ObjectID 第二颜色附件
			VkRenderingAttachmentInfo objectIDAttachment{};
			objectIDAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			objectIDAttachment.imageView = _object_id_RTs[frameindex].sRT.imageView;
			objectIDAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			objectIDAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			objectIDAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			objectIDAttachment.clearValue.color.uint32[0] = 0;

			VkRenderingAttachmentInfo colorAttachments[2] = { color, objectIDAttachment };

			VkRenderingInfo ri{};
			ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			ri.renderArea = { {0, 0}, scene_extent };
			ri.layerCount = 1;
			ri.colorAttachmentCount = 2;
			ri.pColorAttachments = colorAttachments;
			ri.pDepthAttachment = &depth;

			vkCmdBeginRendering(cmd, &ri);

			vkCmdDrawIndirectCount(cmd,
				_draw_count_and_indirect_buffers[frameindex].buffer, sizeof(uint32_t) * IVulkan::MAX_INSTANCE_BATCH_COUNT,
				_draw_count_and_indirect_buffers[frameindex].buffer, 0,
				IVulkan::EXPECTED_INSTANCE_SIZE,
				sizeof(VkDrawIndirectCommand));
			vkCmdEndRendering(cmd);

			// 离屏颜色: COLOR_ATTACHMENT -> SHADER_READ_ONLY (后处理采样)
			if (scene_offscreen && _is_post_processing_renderer_ready != 0)
			{
				_record_image_barrier_auto(cmd, _mainRTs[frameindex].image,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_READ_BIT);
			}

			// U32 ObjectID RT: COLOR_ATTACHMENT -> SHADER_READ_ONLY (供 CS 通过 bindless 读取)
			_record_image_barrier_auto(cmd, _object_id_RTs[frameindex].sRT.image,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_READ_BIT);

			// 深度 RT: DEPTH_STENCIL_ATTACHMENT -> SHADER_READ_ONLY (供 CS 通过 bindless 读取)
			_record_image_barrier_auto(cmd, scene_depth_image,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_READ_BIT);

			// ---- 6.2 ObjectID 拾取计算着色器 (必须在本帧几何之后, 采样本帧的 objectID / 深度) ----
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
				_pipelines[IRendererPipelineType::PipelineComputeReturnObjectID]);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
				base.GetVkPipelineLayout(), 0, 2, sets, 0, nullptr);
			vkCmdDispatch(cmd, 1, 1, 1);

			// ---- 6.3 后处理 ----
			if (_is_post_processing_renderer_ready != 0)
			{
				// 后处理输出目标:
				//   使用 imgui (编辑器) -> postProcessingRTs[frameindex] (离屏, 供 imgui 采样)
				//   不使用 imgui       -> 交换链图像 (直接上屏)
				const bool post_offscreen = _use_imgui;
				const VkExtent2D post_extent = post_offscreen ? _mainRT_extent : base.GetSwapChainExtent();
				const VkImage post_color_image = post_offscreen ? postProcessingRTs[frameindex].image : swapImage;
				const VkImageView post_color_view = post_offscreen ? postProcessingRTs[frameindex].imageView : base.GetSwapChainImageViews()[_image_index];

				// 后处理输出: -> COLOR_ATTACHMENT_OPTIMAL
				_record_image_barrier_auto(cmd, post_color_image,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					_pipelines[IRendererPipelineType::PipelinePostProcessing]);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					base.GetVkPipelineLayout(), 0, 2, sets, 0, nullptr);

				// 源纹理 = 主场景离屏颜色, 以 bindless slot 形式传入
				// (后处理 pass 无 "本次绘制对象索引" 语义, 复用 PushConstants.DrawObjectIndex 传递源纹理槽位)
				IVulkan::PushConstants postPushConstants{};
				postPushConstants.DrawObjectIndexOffset = _mainRT_bindless_slots[frameindex];
				postPushConstants.LevelID = 0xFFFF;
				postPushConstants.BatchID = 0;
				vkCmdPushConstants(cmd,
					base.GetVkPipelineLayout(),
					base.GetPushConstantStageFlags(),
					0,
					sizeof(IVulkan::PushConstants),
					&postPushConstants);

				VkViewport postViewport{};
				postViewport.width = static_cast<float>(post_extent.width);
				postViewport.height = static_cast<float>(post_extent.height);
				postViewport.maxDepth = 1.0f;
				VkRect2D postScissor{ {0, 0}, post_extent };
				vkCmdSetViewport(cmd, 0, 1, &postViewport);
				vkCmdSetScissor(cmd, 0, 1, &postScissor);

				VkRenderingAttachmentInfo postColor{};
				postColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				postColor.imageView = post_color_view;
				postColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				postColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		// 全屏 pass, 像素被完全覆盖
				postColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				postColor.clearValue = _vk_clear;

				VkRenderingInfo postRenderingInfo{};
				postRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
				postRenderingInfo.renderArea = { {0, 0}, post_extent };
				postRenderingInfo.layerCount = 1;
				postRenderingInfo.colorAttachmentCount = 1;
				postRenderingInfo.pColorAttachments = &postColor;
				postRenderingInfo.pDepthAttachment = nullptr;		// 后处理不需要深度

				vkCmdBeginRendering(cmd, &postRenderingInfo);
				vkCmdDraw(cmd, 3, 1, 0, 0);		// 全屏大三角形 (无顶点缓冲)
				vkCmdEndRendering(cmd);

				// 后处理离屏结果: COLOR_ATTACHMENT -> SHADER_READ_ONLY (供 imgui 采样)
				if (post_offscreen)
				{
					_record_image_barrier_auto(cmd, postProcessingRTs[frameindex].image,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						VK_ACCESS_2_SHADER_READ_BIT);
				}
			}
		}

		// 7) draw imgui (编辑器)
		if (_use_imgui)
		{
			// imgui 要采样的最终场景图像:
			//   后处理已执行 -> postProcessingRTs[frameindex] (后处理结果)
			//   否则         -> _mainRTs[frameindex] (主场景离屏颜色)
			const bool scene_post_rendered =
				(_is_main_renderer_ready != 0 && _is_post_processing_renderer_ready != 0);
			const VkImageView scene_display_view = scene_post_rendered ?
				postProcessingRTs[frameindex].imageView :
				_mainRTs[frameindex].imageView;

			// 離屏顏色 RT 屏障：COLOR_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
			// (后处理前的屏障已录好; 这里处理 imgui 直接采样主场景 RT 的情况,
			//  主场景 pass 因管线未就绪而没执行时也需要转换 layout 才能被采样)
			if (!scene_post_rendered)
			{
				_record_image_barrier_auto(cmd, _mainRTs[frameindex].image,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_READ_BIT);
			}

			// 交換鏈: -> COLOR_ATTACHMENT (與 acquire 等待階段對齊)
			_record_image_barrier_auto(cmd, swapImage,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

			// 使用离屏渲染场景时不需要了
			// _imgui_barrier(cmd, swapImage);

			_imgui_frame_data = _imgui_end_frame_and_fetch_func(frameindex, scene_display_view, _global_sampler);

			VkRenderingAttachmentInfo uiColor{};
			uiColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			uiColor.imageView = base.GetSwapChainImageViews()[_image_index]; // 相同的 Swapchain View
			uiColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			uiColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;     // ImGui 模式下 3D 场景在离屏 RT，swapchain 无需保留旧内容
			uiColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // 儲存最終包含 UI 的畫面
			//uiColor.clearValue = _vk_clear;

			VkRenderingInfo uiRenderingInfo{};
			uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			uiRenderingInfo.renderArea = { {0, 0}, base.GetSwapChainExtent() };
			uiRenderingInfo.layerCount = 1;
			uiRenderingInfo.colorAttachmentCount = 1;
			uiRenderingInfo.pColorAttachments = &uiColor;
			uiRenderingInfo.pDepthAttachment = nullptr;       // 關鍵：不傳深度，UI 永遠置頂

			// 3. 開啟 ImGui 的動態渲染 Pass
			vkCmdBeginRendering(cmd, &uiRenderingInfo);
			if (_imgui_frame_data)
			{
				_imgui_draw_data_func(cmd, _imgui_frame_data);
			}
			vkCmdEndRendering(cmd);
		}

		// 8) COLOR_ATTACHMENT -> PRESENT_SRC
		_record_image_barrier_auto(cmd, swapImage,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
			0);
	}

	void IVulkanRenderer::EndRender()
	{
		if (_frame_skipped) return;

		auto& base = IVulkanBase::Base();
		const std::uint32_t f = _current_frame;
		VkCommandBuffer cmd = _command_buffers[f];

		if (vkEndCommandBuffer(cmd))
		{
			INVENT_LOG_ERROR("[IVulkanRenderer] vkEndCommandBuffer 失败!");
			return;
		}

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.waitSemaphoreCount = 1;
		si.pWaitSemaphores = &_acquire_semaphores[f];
		si.pWaitDstStageMask = &waitStage;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;
		si.signalSemaphoreCount = 1;
		si.pSignalSemaphores = &_submit_semaphores[_image_index];

		{
			std::lock_guard<std::mutex> lock(base.GetQueueMutex());
			if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &si, _frame_fences[f]))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] 提交失败!");
				return;
			}
		}

		VkSwapchainKHR swapchain = base.GetSwapChain();
		VkPresentInfoKHR pi{};
		pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		pi.waitSemaphoreCount = 1;
		pi.pWaitSemaphores = &_submit_semaphores[_image_index];
		pi.swapchainCount = 1;
		pi.pSwapchains = &swapchain;
		pi.pImageIndices = &_image_index;
		VkResult pr = vkQueuePresentKHR(base.GetPresentQueue(), &pi);
		if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR || IEngine::Instatnce().GetWindowSizeResized().load(std::memory_order_acquire))
		{
			_swapchain_dirty = true;
			//INVENT_LOG_INFO("[IVulkanRenderer] recreate swap chain reason: VK_ERROR_OUT_OF_DATE_KHR | VK_SUBOPTIMAL_KHR | window resized");
			IEngine::Instatnce().GetWindowSizeResized().store(false, std::memory_order_release);
		}

	}

	void IVulkanRenderer::UpdatePipeline(IRendererPipelineType type, VkPipeline pipeline)
	{
		assert(static_cast<std::size_t>(type) < _pipelines.size());

		std::lock_guard<std::mutex> lock(_pipelines_updates_mutex);
		_pipelines_updates.emplace_back(type, pipeline);
	}

	void IVulkanRenderer::SetMainRTExtent(std::uint32_t width, std::uint32_t height)
	{
		if (_mainRT_extent.width == width && _mainRT_extent.height == height) return;
		_mainRT_extent = { width, height };
		_main_rt_extent_dirty = true;
	}

	void IVulkanRenderer::_record_image_barrier_auto(VkCommandBuffer cmd, VkImage image,
		VkImageLayout dst_layout,
		VkPipelineStageFlags2 dst_stage_mask,
		VkAccessFlags2 dst_access_mask)
	{
		auto& current = (*_image_barrier_cache_map)[reinterpret_cast<std::uint64_t>(image)];

		VkImageMemoryBarrier2 b_rt{};
		b_rt.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		b_rt.image = image;
		b_rt.subresourceRange = { current.imageAspectFlags, 0, 1, 0, 1 };
		b_rt.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b_rt.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		b_rt.oldLayout = current.currentLayout;
		b_rt.newLayout = dst_layout;
		current.currentLayout = dst_layout;
		
		b_rt.srcStageMask = current.currentStageMask;
		b_rt.dstStageMask = dst_stage_mask;
		current.currentStageMask = dst_stage_mask;
		
		b_rt.srcAccessMask = current.currentAccessMask;
		b_rt.dstAccessMask = dst_access_mask;
		current.currentAccessMask = dst_access_mask;
			
		VkDependencyInfo dep{};
		dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers = &b_rt;

		vkCmdPipelineBarrier2(cmd, &dep);
	}

	void IVulkanRenderer::Shutdown()
	{
		_clear();
		InstanceBuffer::Destroy();
		IVulkanGlobalVerticesIndices::Instance().Destroy();
		IVulkanGlobalMaterial::Instance().Clear();
		IVulkanTexture2DManagement::Instance().Clear();		// 内部会冲刷帧延迟销毁环
		IVulkanTexture2DManagement::Instance().Terminate();
	}

	void IVulkanRenderer::_clear()
	{
		auto& base = IVulkanBase::Base();
		auto device = base.GetDevice();
		if (device == VK_NULL_HANDLE) return;

		{
			std::lock_guard<std::mutex> lock(base.GetQueueMutex());
			vkDeviceWaitIdle(device);
		}

		for (auto& pipeline : _pipelines)
		{
			if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
		if (_global_sampler != VK_NULL_HANDLE) { vkDestroySampler(device, _global_sampler, nullptr); _global_sampler = VK_NULL_HANDLE; }

		for (auto& rt : _mainRTs)
		{
			if (rt.imageView != VK_NULL_HANDLE)
			{
				base.DestroyImageView(rt.imageView);
			}
			if (rt.image != VK_NULL_HANDLE)
			{
				base.UseVmaDestroyImage(rt.image);
			}
		}
		_mainRTs.clear();
		for (auto slot : _mainRT_bindless_slots)
		{
			if (slot != UINT32_MAX)
				IVulkanTexture2DManagement::Instance().DestroyTexture2D({ slot });
		}
		_mainRT_bindless_slots.clear();
		for (auto& rt : postProcessingRTs)
		{
			if (rt.imageView != VK_NULL_HANDLE)
			{
				base.DestroyImageView(rt.imageView);
			}
			if (rt.image != VK_NULL_HANDLE)
			{
				base.UseVmaDestroyImage(rt.image);
			}
		}
		postProcessingRTs.clear();
		for (auto& rt : _object_id_RTs)
		{
			IVulkanTexture2DManagement::Instance().DestroyTexture2D({ rt.bindlessSlot });
			if (rt.sRT.imageView != VK_NULL_HANDLE)
				base.DestroyImageView(rt.sRT.imageView);
			if (rt.sRT.image != VK_NULL_HANDLE)
				base.UseVmaDestroyImage(rt.sRT.image);
		}
		_object_id_RTs.clear();
		_image_barrier_cache_map->clear();
		for (auto& d : FrameRTDestroyRing)
		{
			for (auto& rt : d)
			{
				if (rt.imageView != VK_NULL_HANDLE)
				{
					base.DestroyImageView(rt.imageView);
				}
				if (rt.image != VK_NULL_HANDLE)
				{
					base.UseVmaDestroyImage(rt.image);
				}
			}
			d.clear();
		}
		for (auto& d : FramePipelineDestroyRing)
		{
			for (auto& p : d)
			{
				if (p != VK_NULL_HANDLE)
				{
					vkDestroyPipeline(device, p, nullptr);
				}
			}
			d.clear();
		}
		_destroy_cursor = 0;
		
		IVulkanTexture2DManagement::Instance().DestroyTexture2D({ _main_depth_RT.bindlessSlot });
		if (_main_depth_RT.sRT.imageView != VK_NULL_HANDLE)
		{
			base.DestroyImageView(_main_depth_RT.sRT.imageView);
		}
		if (_main_depth_RT.sRT.image != VK_NULL_HANDLE)
		{
			base.UseVmaDestroyImage(_main_depth_RT.sRT.image);
		}

		if (_image_barrier_cache_map)
		{
			delete _image_barrier_cache_map;
			_image_barrier_cache_map = nullptr;
		}

		for (auto& b : _ubo_buffers)
		{
			if (b.buffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(b.buffer);
			b.buffer = VK_NULL_HANDLE;
		}
		for (auto& b : _point_light_buffers)
		{
			if (b.buffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(b.buffer);
			b.buffer = VK_NULL_HANDLE;
		}
		for (auto& b : _draw_count_and_indirect_buffers)
		{
			if (b.buffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(b.buffer);
			b.buffer = VK_NULL_HANDLE;
		}
		for (auto& b : _mouse_back_buffers)
		{
			if (b.buffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(b.buffer);
			b.buffer = VK_NULL_HANDLE;
		}
			
		_ubo_buffers.clear();
		_point_light_buffers.clear();
		_draw_count_and_indirect_buffers.clear();
		_mouse_back_buffers.clear();

		// 测试纹理 (GPU 资源走管理器延迟销毁环, 由随后的 TextureMgmt::Clear 冲刷)
		if (_test_texture_slot != UINT32_MAX)
			IVulkanTexture2DManagement::Instance().DestroyTexture2D({ _test_texture_slot });
		_test_texture_slot = UINT32_MAX;

		for (auto f : _frame_fences)
			if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
		for (auto s : _acquire_semaphores)
			if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
		for (auto s : _submit_semaphores)
			if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
		_frame_fences.clear(); _acquire_semaphores.clear(); _submit_semaphores.clear();
	}
}