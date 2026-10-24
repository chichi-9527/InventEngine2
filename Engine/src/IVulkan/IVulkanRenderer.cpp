#include "IVulkan/IVulkanRenderer.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/IVulkanGlobalTexture.h"
#include "IVulkan/IVulkanGlobalVerticesIndices.h"
#include "IVulkan/IVulkanInstanceBuffer.h"
#include "IVulkan/IVulkanGlobalMaterial.h"
#include "ILog.h"
#include "IEngine.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>

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

	static IVulkan::PushConstants pushConstants{};

	bool IVulkanRenderer::Init()
	{
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE)
		{
			INVENT_LOG_FATAL("[IVulkanRenderer] VulkanBase 未初始化!");
			return false;
		}

		IVulkanTexture2DManagement::Instance().SetTextureResidentChangedCallback(
			[](IVulkanTexture2DManagement::Texture2DHandle h,
				std::uint32_t baseMip, std::uint32_t mipCount, std::uint32_t totalMips) {
					// 材质 baseMip 更新接入点 (测试阶段空实现)
			});

		// 管理器 (顺序: 纹理 -> 材质 -> 顶点索引 -> 实例)
		if (!IVulkanTexture2DManagement::Instance().Init() ||
			!IVulkanGlobalMaterial::Instance().Init() ||
			!IVulkanGlobalVerticesIndices::Instance().Init() ||
			!InstanceBuffer::Init())
		{
			return false;
		}

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

	bool IVulkanRenderer::_init_buffers()
	{
		auto& base = IVulkanBase::Base();

		// 每帧 buffers
		_ubo_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_point_light_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_draw_count_and_indirect_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);

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


		}

		// 测试实例数据: 1 个实例 = 1 个 uint (texture slot), 内容在 _init_test_resources 填
		if (VkResult r = base.UseVmaCreateBuffer(16,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			_test_instance_buffer, &_test_instance_mapped))
		{
			INVENT_LOG_ERROR("[IVulkanRenderer] 测试实例缓冲创建失败!");
			return false;
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
		ubo.cameraDatas = { cameraPos, 0.0f };
		if (auto height = engine.GetWindowHeight())
		{
			ubo.cameraDatas.w = static_cast<float>(height) / (2.0f * std::tan(glm::radians(45.0f) / 2.0f));
		}

		memcpy(_ubo_buffers[frame_index].mapped, &ubo, sizeof(IVulkan::UBO));

		//IVulkan::PointLight lights[IVulkan::MAX_POINT_LIGHT_COUNT]{};
		// TODO: 从场景收集点光源数据
		//memcpy(_point_light_buffers[frame_index].mapped, lights, sizeof(lights));

		base.UseVmaFlushAllocationBuffer(_ubo_buffers[frame_index].buffer);
		//base.UseVmaFlushAllocationBuffer(_point_light_buffers[frame_index].buffer);
	}

	bool IVulkanRenderer::_recreate_swapchain()
	{
		auto& base = IVulkanBase::Base();
		auto device = base.GetDevice();

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

		// 图形管线
		IVulkanBase::GraphicsPipelineConfig cfg{};
		cfg.VertexShader = vs;
		cfg.FragmentShader = fs;
		cfg.ColorAttachmentFormat = base.GetSwapChainImageFormat();	// 不要硬编码! 实际是 B8G8R8A8_SRGB
		cfg.DepthAttachmentFormat = base.GetDepthFormat();
		cfg.EnableDepthTest = VK_TRUE;
		cfg.CullMode = VK_CULL_MODE_NONE;
		_main_opaque_pipeline = base.CreateGraphicsPipeline(cfg);
		if (_main_opaque_pipeline == VK_NULL_HANDLE) return false;

		// 反馈计算管线 (全局 layout: set0)
		_compute_pipeline = base.CreateComputePipeline(cs);
		if (_compute_pipeline == VK_NULL_HANDLE) return false;

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

		// 实例 0 = 测试纹理 slot
		*static_cast<std::uint32_t*>(_test_instance_mapped) = h.slot;
		base.UseVmaFlushAllocationBuffer(_test_instance_buffer);

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
		testData.ObjectID = 1;
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

		// 1) 帧同步
		vkWaitForFences(device, 1, &_frame_fences[frameindex], VK_TRUE, UINT64_MAX);

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
		if (_imgui_new_frame_func)
		{
			_imgui_new_frame_func();
		}
		_imgui_frame_data = nullptr;
		if (_imgui_end_frame_and_fetch_func)
		{
			_imgui_frame_data = _imgui_end_frame_and_fetch_func(frameindex);
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

		// 1) 新上传窗口 image 的所有权获取 (transfer -> graphics), 必须在所有采样之前
		texMgmt.RecordAcquireBarriers(cmd);

		// 2) 交换链: UNDEFINED -> COLOR_ATTACHMENT (srcStage 与提交时的 acquire 等待阶段对齐)
		_record_swapchain_barrier(cmd, swapImage, false);

		// 深度缓冲: UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		_record_depth_barrier(cmd, base.GetDepthImage());

		// 脏材质上传 + 读屏障
		IVulkanGlobalMaterial::Instance().FlushDirtyMaterials(cmd, frameindex);
		// 实例刷新
		InstanceBuffer::FlushBuffer();

		// 3) mip 反馈 CS
		vkCmdFillBuffer(cmd, _draw_count_and_indirect_buffers[frameindex].buffer,
			0, 
			sizeof(uint32_t) * IVulkan::MAX_INSTANCE_BATCH_COUNT,
			0);	// 将shader中渲染原子计数器置0
		texMgmt.ResetMipFeedback(cmd, frameindex);		// fill 0xFF + transfer->compute 屏障
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
			base.GetVkPipelineLayout(), 0, 1, &_set0[frameindex], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute_pipeline);
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

		// 4) quad 绘制 (动态渲染)
		VkDescriptorSet sets[2] = { _set0[frameindex], base.GetGlobalBindlessDescriptorSet() };
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _main_opaque_pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
			base.GetVkPipelineLayout(), 0, 2, sets, 0, nullptr);

		VkExtent2D extent = base.GetSwapChainExtent();
		VkViewport viewport{};
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, extent };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		VkRenderingAttachmentInfo color{};
		color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color.imageView = base.GetSwapChainImageViews()[_image_index];
		color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.clearValue = _vk_clear;

		VkRenderingAttachmentInfo depth{};
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth.imageView = base.GetDepthImageView();
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // 清除深度缓冲区
		depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 绘制完如果不需要采样，可以用 DONT_CARE 优化性能
		depth.clearValue.depthStencil = { 1.0f, 0 };	  // 默认深度设为 1.0

		VkRenderingInfo ri{};
		ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		ri.renderArea = { {0, 0}, extent };
		ri.layerCount = 1;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &color;
		ri.pDepthAttachment = &depth;

		vkCmdBeginRendering(cmd, &ri);
		
		vkCmdDrawIndirectCount(cmd,
			_draw_count_and_indirect_buffers[frameindex].buffer, sizeof(uint32_t) * IVulkan::MAX_INSTANCE_BATCH_COUNT,
			_draw_count_and_indirect_buffers[frameindex].buffer, 0,
			IVulkan::EXPECTED_INSTANCE_SIZE,
			sizeof(VkDrawIndirectCommand));
		vkCmdEndRendering(cmd);

		// draw imgui
		if (_imgui_draw_data_func && _imgui_frame_data)
		{
			//INVENT_LOG_DEBUG("draw imgui.");

			_imgui_barrier(cmd, swapImage);
			VkRenderingAttachmentInfo uiColor{};
			uiColor.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			uiColor.imageView = base.GetSwapChainImageViews()[_image_index]; // 相同的 Swapchain View
			uiColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			uiColor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;     // 關鍵：LOAD！保留 3D 間接渲染畫出的場景
			uiColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;   // 儲存最終包含 UI 的畫面

			VkRenderingInfo uiRenderingInfo{};
			uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			uiRenderingInfo.renderArea = { {0, 0}, extent };
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

		// 5) COLOR_ATTACHMENT -> PRESENT_SRC
		_record_swapchain_barrier(cmd, swapImage, true);
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

	void IVulkanRenderer::_record_swapchain_barrier(VkCommandBuffer cmd, VkImage image, bool to_present)
	{
		VkImageMemoryBarrier2 b{};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		b.image = image;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		if (!to_present)
		{
			b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;	// 与 acquire 等待阶段对齐
			b.srcAccessMask = 0;
			b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		}
		else
		{
			b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			b.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
			b.dstAccessMask = 0;
		}

		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.imageMemoryBarrierCount = 1;
		di.pImageMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &di);
	}

	void IVulkanRenderer::_record_depth_barrier(VkCommandBuffer cmd, VkImage depth_image)
	{
		VkImageMemoryBarrier2 b{};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		b.image = depth_image;
		// 注意：深度 Image 的 aspectMask 必須是 DEPTH_BIT
		b.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// 來源階段：因為 oldLayout 是 UNDEFINED，不需要等待先前的特定操作
		b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		b.srcAccessMask = 0;

		// 目的階段：對齊圖形管線的「早期深度測試」階段，並開啟讀寫權限
		b.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
		b.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.imageMemoryBarrierCount = 1;
		di.pImageMemoryBarriers = &b;

		vkCmdPipelineBarrier2(cmd, &di);
	}

	void IVulkanRenderer::_imgui_barrier(VkCommandBuffer cmd, VkImage image)
	{
		VkImageMemoryBarrier2 image_barrier{};
		image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_barrier.pNext = nullptr;
		image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 佈局保持不變
		image_barrier.image = image; // 你的真實 SwapChain Image 物件
		image_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkDependencyInfo dependency_info{};
		dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &image_barrier;

		vkCmdPipelineBarrier2(cmd, &dependency_info);
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

		if (_main_opaque_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, _main_opaque_pipeline, nullptr); _main_opaque_pipeline = VK_NULL_HANDLE; }
		if (_compute_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, _compute_pipeline, nullptr); _compute_pipeline = VK_NULL_HANDLE; }
		if (_global_sampler != VK_NULL_HANDLE) { vkDestroySampler(device, _global_sampler, nullptr); _global_sampler = VK_NULL_HANDLE; }

		if (_test_instance_buffer != VK_NULL_HANDLE) { base.UseVmaDestroyBuffer(_test_instance_buffer); _test_instance_buffer = VK_NULL_HANDLE; }
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
			
		_ubo_buffers.clear();
		_test_instance_mapped = nullptr;

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