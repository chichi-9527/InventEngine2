#include "IVulkan/IBuffer.h"

#include "IVulkan/VulkanBase.h"
#include "ILog.h"

namespace INVENT
{
	bool IBaseBuffer::InitUploadContext()
	{
		if (_s_ctx.IsValid()) return true;
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE) return false;

		VkCommandPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		pci.queueFamilyIndex = base.GetQueueFamilyIndices().GraphicsFamily;
		if (vkCreateCommandPool(base.GetDevice(), &pci, nullptr, &_s_ctx.CmdPool))
		{
			INVENT_LOG_ERROR("[IVertexBuffer] failed to create upload command pool!");
			return false;
		}
		VkFenceCreateInfo fci{};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(base.GetDevice(), &fci, nullptr, &_s_ctx.Fence))
		{
			INVENT_LOG_ERROR("[IVertexBuffer] failed to create upload fence!");
			vkDestroyCommandPool(base.GetDevice(), _s_ctx.CmdPool, nullptr);
			_s_ctx.CmdPool = VK_NULL_HANDLE;
			return false;
		}
		_s_ctx.Queue = base.GetGraphicsQueue();
		INVENT_LOG_INFO("[IVertexBuffer] upload context ready (graphics queue).");
		return true;
	}

	void IBaseBuffer::DestroyUploadContext()
	{
		if (!_s_ctx.IsValid()) return;
		auto device = IVulkanBase::Base().GetDevice();
		if (_s_ctx.Fence != VK_NULL_HANDLE) { vkDestroyFence(device, _s_ctx.Fence, nullptr); _s_ctx.Fence = VK_NULL_HANDLE; }
		if (_s_ctx.CmdPool != VK_NULL_HANDLE) { vkDestroyCommandPool(device, _s_ctx.CmdPool, nullptr); _s_ctx.CmdPool = VK_NULL_HANDLE; }
		_s_ctx.Queue = VK_NULL_HANDLE;
	}

	VkCommandBuffer IBaseBuffer::_begin_one_time_cmd()
	{
		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandPool = _s_ctx.CmdPool;
		ai.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(IVulkanBase::Base().GetDevice(), &ai, &cmd))
			return VK_NULL_HANDLE;
		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &bi);
		return cmd;
	}

	bool IBaseBuffer::_end_one_time_cmd(VkCommandBuffer cmd)
	{
		if (vkEndCommandBuffer(cmd))
		{
			vkFreeCommandBuffers(IVulkanBase::Base().GetDevice(), _s_ctx.CmdPool, 1, &cmd);
			return false;
		}
		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;
		auto device = IVulkanBase::Base().GetDevice();
		vkResetFences(device, 1, &_s_ctx.Fence);
		if (VkResult r = vkQueueSubmit(_s_ctx.Queue, 1, &si, _s_ctx.Fence))
		{
			INVENT_LOG_ERROR(std::format("[IVertexBuffer] 提交失敗! VkResult: {}.", static_cast<std::int32_t>(r)));
		}
		else if (VkResult r = vkWaitForFences(device, 1, &_s_ctx.Fence, VK_TRUE, UINT64_MAX))
		{
			INVENT_LOG_ERROR(std::format("[IVertexBuffer] fence 等待失敗! VkResult: {}.", static_cast<std::int32_t>(r)));
		}
		else
		{
			vkFreeCommandBuffers(device, _s_ctx.CmdPool, 1, &cmd);
			return true;		// 拷貝完成
		}
		vkFreeCommandBuffers(device, _s_ctx.CmdPool, 1, &cmd);
		return false;
	}

	void IBaseBuffer::_record_visibility_barrier(VkCommandBuffer cmd, VkBuffer buffer)
	{
		VkBufferMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		// 消費: BDA 頂點拉取 (vertex/compute shader) + 傳統管線綁定 (vertex input)
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
			| VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
			| VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = buffer;
		barrier.size = VK_WHOLE_SIZE;
		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.bufferMemoryBarrierCount = 1;
		di.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmd, &di);
	}
}
