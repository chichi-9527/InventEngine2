#pragma once

#include "IBitArray.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <shared_mutex>

namespace IVulkan
{
	struct Batch
	{
		std::shared_mutex _rw_mutex;
		INVENT::IBitVectorSafe _valids{};
		std::uint32_t Offset{ 0 };
		std::uint32_t Count{ 0 };
		std::uint32_t UsedHighWaterMark{ 0 };
	};
}

namespace INVENT
{
	class InstanceBuffer
	{
	public:
		enum InstanceUseFlag : std::uint32_t {
			NoUse = 0,
			UseUV = 1 << 0,
			ShowInstance = 1 << 1
		};
		struct alignas(16) InstanceData
		{
			glm::mat4 Model{ 1.0f };
			VkDeviceAddress VertexAddress{ 0 };		// 顶点起始指针
			VkDeviceAddress MaterialAddress{ 0 };	// 材质起始指针
			VkDeviceAddress IndexAddress{ 0 };		// 顶点索引起始指针
			VkDeviceAddress BoneBufferAddress{ 0 };	// 骨骼起始指针
			glm::vec4 UV{ 0.0f,0.0f,1.0f,1.0f };
			glm::vec4 Color{ 1.0f };
			std::uint32_t IndexCount{ 0 };
			std::uint32_t BoneCount{ 0 };
			std::uint32_t ObjectID{ 0 };			// 为0时视为无效，计算着色器使用
			std::uint32_t UseFlags = InstanceUseFlag::NoUse;
		};
		static_assert(alignof(InstanceData) == 16);
		static_assert(sizeof(InstanceData) == 144);
		static_assert(offsetof(InstanceData, Model) == 0);
		static_assert(offsetof(InstanceData, VertexAddress) == 64);
		static_assert(offsetof(InstanceData, UV) == 96);
		static_assert(offsetof(InstanceData, Color) == 112);
		static_assert(offsetof(InstanceData, IndexCount) == 128);
		static_assert(offsetof(InstanceData, UseFlags) == 140);

		struct Handle 
		{
			uint32_t batchIndex{ UINT32_MAX };
			IHandle ihandle{};
			bool IsValid() const noexcept { return batchIndex != UINT32_MAX && ihandle.IsValid(); }
			uint32_t GetInstanceIndex() const noexcept { return static_cast<uint32_t>(ihandle.GetRealIndex()); }
		};
	public:
		/// <param name="other_batches_num">
		/// 除默认批次外的其他批次实体数量，所有批次加在一起等于帧渲染上限
		/// batch_index = 0 代表預設批次然后顺延
		/// </param>
		/// <returns></returns>
		static bool Init(const std::vector<uint32_t>& other_batches = {});
		static void Destroy();

		/// <returns> 失败会返回 UINT32_MAX </returns>
		static uint32_t GetOffset(size_t batch_index);
		/// <returns> 失败会返回 UINT32_MAX </returns>
		static uint32_t GetUsedHighWaterMark(size_t batch_index);

		static Handle AddInstanceData(const InstanceData& data, uint32_t batch_index = 0);
		static void UpdateInstanceData(const InstanceData& data, Handle handle);
		static void FreeInstanceData(Handle handle);

		static void FlushBuffer();

		static VkBuffer GetBuffer() { return _ssbo; }
	private:
		inline static VkBuffer _ssbo = VK_NULL_HANDLE;
		inline static std::vector<IVulkan::Batch*> _batches; // 保存所有批次的偏移
		inline static void* _mapped_data = nullptr;
		inline static uint32_t _current_instance_num{ 0 };
	};
}
