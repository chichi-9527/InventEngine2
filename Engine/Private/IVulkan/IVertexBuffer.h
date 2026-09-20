#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace INVENT
{
	enum VertexOptionalFlags : std::uint32_t
	{
		HasNone = 0,
		HasUV = 1u << 0,
		HasNormal = 1u << 1,
		HasTangent = 1u << 2,
		HasBitangent = 1u << 3,
		HasColor = 1u << 4,
		HasBones = 1u << 5
	};

	struct alignas(16) IVertex
	{
		// position (12)
		glm::vec3 position;
		// (4)
		VertexOptionalFlags flags = VertexOptionalFlags::HasNone;

		//----------------------------------------------------

		// uv (8)
		glm::vec2 uv;
		// normal (6)
		glm::u16vec3 normal;
		// (2)
		std::uint16_t _pad0{ 0 };

		//----------------------------------------------------

		// tangent 切线 (6)
		glm::u16vec3 tangent;
		// bitangent 双切线 (6)
		glm::u16vec3 bitangent;
		// color (4)
		glm::u8vec4 color{ 255u };

		//-----------------------------------------------------

		// bone index (8)
		glm::u16vec4 bones{ 0u };
		// bone weights (8)
		glm::u16vec4 weights{ 0u };

	};
	static_assert(sizeof(IVertex) == 64);
	static_assert(offsetof(IVertex, uv) == 16);
	static_assert(offsetof(IVertex, normal) == 24);
	static_assert(offsetof(IVertex, tangent) == 32);
	static_assert(offsetof(IVertex, bitangent) == 38);
	static_assert(offsetof(IVertex, color) == 44);
	static_assert(offsetof(IVertex, bones) == 48);
	static_assert(offsetof(IVertex, weights) == 56);

	class IVertexBuffer
	{
		friend class IVulkanGlobalVertices;

		struct VerticesData
		{
			VkDeviceAddress BaseAddress{ 0 };
			std::uint32_t Count{ 0 };
			std::uint32_t Offset{ 0 };
		};

	public:
		static constexpr std::uint32_t INVALID_VHANDLE = UINT32_MAX;
		using Vhandle = std::uint32_t;

	public:
		IVertexBuffer(std::uint32_t vertex_count = 0);
		~IVertexBuffer();

		/// <returns> 失败返回 UNIT32_MAX </returns>
		Vhandle AddVertices(const IVertex* vertices, std::uint32_t count);
		void DestoryVertices(Vhandle handle);

		void Reset();
		bool CheckDefragment() const;

		std::uint32_t GetMaxVertexCount() const { return _vertex_count; }
		std::uint32_t GetUsedCount() const { return _used_count; }
		std::uint32_t GetCanAllocateCount() const { return _vertex_count - _offset; }
	private:
		const std::vector<VerticesData>& _get_datas() const { return _datas; }
		const std::unordered_set<Vhandle>& _get_used_handles() const { return _used_handles; }
		VkDeviceAddress _get_device_address() const { return _device_address; }
		VkBuffer _get_buffer() const { return _buffer; }
		void* _get_mapped_data() const { return _mapped_data; }

	private:
		VkBuffer _buffer = VK_NULL_HANDLE;
		void* _mapped_data = nullptr;
		VkDeviceAddress _device_address{ 0 };

		std::vector<VerticesData> _datas;
		std::unordered_set<Vhandle> _used_handles;

		std::uint32_t _vertex_count{ 0 };
		std::uint32_t _used_count{ 0 };
		std::uint32_t _offset{ 0 };
	};
}
