#include "IVulkan/IVulkanRenderer.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "IEngineTools.h"
#include "IVulkan/IVulkanGlobalTexture.h"
#include "IVulkan/IVulkanInstanceBuffer.h"
#include "ILog.h"

#include <filesystem>

namespace INVENT
{
	constexpr const char* MainMeshVertexShaderPath = "Config/Shaders/SPV/MainMesh.slang.vert.spv";
	constexpr const char* MainMeshFragmentShaderPath = "Config/Shaders/SPV/MainMesh.slang.frag.spv";

	bool IVulkanRenderer::Init()
	{
		/*if (!_init_pipelines())
		{
			return false;
		}*/
		if (!IVulkanTexture2DManagement::Instance().IsValid() ||
			!InstanceBuffer::Init())
		{
			return false;
		}
		return true;
	}

	void IVulkanRenderer::Shutdown()
	{
		_clear();
		InstanceBuffer::Destroy();
		IVulkanTexture2DManagement::Instance().Clear();
		IVulkanTexture2DManagement::Instance().Terminate();
	}

	void IVulkanRenderer::RenderFrame(std::uint32_t frameindex)
	{
		
	}

	bool IVulkanRenderer::_init_pipelines()
	{

		// main
		auto mainMeshVS = IVulkanBase::Base().CreateShaderMoudle(MainMeshVertexShaderPath);
		auto mainMeshFS = IVulkanBase::Base().CreateShaderMoudle(MainMeshFragmentShaderPath);
		// 不透明
		{
			IVulkanBase::GraphicsPipelineConfig pipelineConfig{};
			pipelineConfig.VertexShader = mainMeshVS;
			pipelineConfig.FragmentShader = mainMeshFS;
			pipelineConfig.BlendMode = IVulkanBase::ModelBlendMode::Opaque;
			pipelineConfig.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
			pipelineConfig.DepthAttachmentFormat = IVulkanBase::Base().GetDepthFormat();
			pipelineConfig.CullMode = VK_CULL_MODE_BACK_BIT;

			IVulkanRenderer::_main_opaque_pipeline = IVulkanBase::Base().CreateGraphicsPipeline(pipelineConfig);
			if (IVulkanRenderer::_main_opaque_pipeline == VK_NULL_HANDLE) return false;
		}
		IVulkanBase::Base().DestroyShaderMoudle(mainMeshVS); mainMeshVS = VK_NULL_HANDLE;
		IVulkanBase::Base().DestroyShaderMoudle(mainMeshFS); mainMeshFS = VK_NULL_HANDLE;

		return true;
	}

	bool IVulkanRenderer::_init_descriptor_sets()
	{
		IVulkanRenderer::_ubo.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		IVulkanRenderer::_point_light_ssbo.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		IVulkanRenderer::_instance_ssbo.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		IVulkanRenderer::_out_instance_ssbo.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (VkResult result = IVulkanBase::Base().AllocateDescriptSetFromOtherDescriptorPools(IVulkanRenderer::_ubo[i]))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] allocate ubo descript set error!");
				return false;
			}
			if (VkResult result = IVulkanBase::Base().AllocateDescriptSetFromOtherDescriptorPools(IVulkanRenderer::_point_light_ssbo[i]))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] allocate point light ssbo descript set error!");
				return false;
			}
			if (VkResult result = IVulkanBase::Base().AllocateDescriptSetFromOtherDescriptorPools(IVulkanRenderer::_instance_ssbo[i]))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] allocate instance ssbo descript set error!");
				return false;
			}
			if (VkResult result = IVulkanBase::Base().AllocateDescriptSetFromOtherDescriptorPools(IVulkanRenderer::_out_instance_ssbo[i]))
			{
				INVENT_LOG_ERROR("[IVulkanRenderer] allocate out instance ssbo descript set error!");
				return false;
			}
		}
		return true;
	}

	bool IVulkanRenderer::_init_buffers()
	{
		IVulkanRenderer::_ubo_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		IVulkanRenderer::_point_light_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		IVulkanRenderer::_out_instance_buffers.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);

		for (uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{

		}
		return true;
	}

	void IVulkanRenderer::_clear()
	{
		
	}
}
