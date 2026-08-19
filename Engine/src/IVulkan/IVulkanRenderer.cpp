#include "IVulkan/IVulkanRenderer.h"

#include "IVulkan/VulkanBase.h"
#include "IEngineTools.h"
#include "IVulkan/IVulkanGlobalTexture.h"

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
		if (!IVulkanTexture2DManagement::Instance().IsValid())
		{
			return false;
		}
		return true;
	}

	void IVulkanRenderer::Shutdown()
	{
		_clear();
		IVulkanTexture2DManagement::Instance().Clear();
		IVulkanTexture2DManagement::Instance().Terminate();
	}

	void IVulkanRenderer::RenderFrame(std::uint32_t frameindex)
	{
		
	}

	bool IVulkanRenderer::_init_pipelines()
	{

		// main
		auto mainMeshVS = IVulkanBase::Base().CreateShaderMoudle(std::filesystem::path{ IEngineTools::GetRunStdPath() / MainMeshVertexShaderPath }.string());
		auto mainMeshFS = IVulkanBase::Base().CreateShaderMoudle(std::filesystem::path{ IEngineTools::GetRunStdPath() / MainMeshFragmentShaderPath }.string());
		// 不透明
		{
			IVulkanBase::GraphicsPipelineConfig pipelineConfig{};
			pipelineConfig.VertexShader = mainMeshVS;
			pipelineConfig.FragmentShader = mainMeshFS;
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

	void IVulkanRenderer::_clear()
	{
		
	}
}
