#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <mutex>

namespace IVulkan
{
	struct UploadContext
	{
		VkCommandPool CmdPool{ VK_NULL_HANDLE };		// graphics family, TRANSIENT
		VkFence Fence{ VK_NULL_HANDLE };
		VkQueue Queue{ VK_NULL_HANDLE };
		bool IsValid() const
		{
			return CmdPool != VK_NULL_HANDLE &&
				Fence != VK_NULL_HANDLE &&
				Queue != VK_NULL_HANDLE;
		}
	};
}

namespace INVENT
{

	class IBaseBuffer
	{
	public:
		static constexpr std::uint32_t INVALID_VHANDLE = UINT32_MAX;
		using Bhandle = std::uint32_t;
	public:
		static bool InitUploadContext();
		static void DestroyUploadContext();

		static void RecordVisibilityBarrier(VkCommandBuffer cmd, VkBuffer buffer);

	protected:
		static VkCommandBuffer _begin_one_time_cmd();
		static bool _end_one_time_cmd(VkCommandBuffer cmd);			// 提交 + fence 等待

	protected:
		inline static IVulkan::UploadContext _s_ctx{};
	};

	template<typename T>
	class IBuffer : public IBaseBuffer
	{
		friend class IVulkanGlobalVerticesIndices;

		using pointType = T;

		struct PointTypeData
		{
			VkDeviceAddress BaseAddress{ 0 };
			std::uint32_t Count{ 0 };
			std::uint32_t Offset{ 0 };
		};

	public:
		
		IBuffer(std::uint32_t vertex_count = 0);
		~IBuffer();

		/// <returns> 失败返回 UNIT32_MAX </returns>
		Bhandle AddDatas(const pointType* points, std::uint32_t count);
		void DestroyDatas(Bhandle handle);

		void Reset();
		bool CheckDefragment() const;

		std::uint32_t GetMaxVertexCount() const { return _vertex_count; }
		std::uint32_t GetUsedCount() const { return _used_count; }
		std::uint32_t GetCanAllocateCount() const { return _vertex_count - _offset; }
	private:
		const std::vector<PointTypeData>& _get_datas() const { return _datas; }
		const std::unordered_set<Bhandle>& _get_used_handles() const { return _used_handles; }
		VkDeviceAddress _get_device_address() const { return _device_address; }
		VkBuffer _get_buffer() const { return _buffer; }
		Bhandle _add_datas_range(std::uint32_t count);

		Bhandle _commit_range(std::uint32_t count);

	private:
		
		inline static std::mutex _s_mutex;

		VkBuffer _buffer = VK_NULL_HANDLE;
		VkDeviceAddress _device_address{ 0 };

		std::vector<PointTypeData> _datas;
		std::unordered_set<Bhandle> _used_handles;

		std::uint32_t _vertex_count{ 0 };
		std::uint32_t _used_count{ 0 };
		std::uint32_t _offset{ 0 };
	};
}

#include "IBuffer.ipp"
