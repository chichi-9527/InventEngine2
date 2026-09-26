#pragma once

#include "VulkanConfig.h"
#include "IBuffer.h"

#include <string>
#include <vector>
#include <memory_resource>
#include <functional>

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

	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class IVulkanGlobalVerticesIndices
	{
		std::pmr::monotonic_buffer_resource _string_memory_buffer{ 8ull * 1024 };
		std::pmr::unsynchronized_pool_resource _string_memory_pool{ &_string_memory_buffer };
		struct VertexIndexDataRecord
		{
			VertexIndexDataRecord(const std::string& n, std::pmr::memory_resource* pool)
				: name(n, pool) {}
			std::pmr::string name;
			IBuffer<IVertex>* vertexBuffer = nullptr;
			IBuffer<std::uint16_t>* index16Buffer = nullptr;
			IBuffer<std::uint32_t>* index32Buffer = nullptr;
			IBaseBuffer::Bhandle vertexHandle{ IBaseBuffer::INVALID_VHANDLE };
			IBaseBuffer::Bhandle indexHandle{ IBaseBuffer::INVALID_VHANDLE };
			std::uint32_t share{ 0 };
		};

		using VertexIndexNameMap = std::unordered_map < std::string,
			std::uint32_t,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, std::uint32_t>>>;

		IVulkanGlobalVerticesIndices() = default;
	public:
		static constexpr std::uint32_t INVALID_HANDLE = UINT32_MAX;

		struct GVIHandle
		{
			std::uint32_t recordIndex{ INVALID_HANDLE };
			std::uint32_t indexCount{ 0 };
			VkDeviceAddress vertexAddress{ 0 };
			VkDeviceAddress indexAddress{ 0 };
			bool IsValid() const
			{
				return recordIndex != INVALID_HANDLE;
			}
		};

		enum class DefMeshVertHandle : std::uint32_t
		{
			DefQuad = 0,
			DefCube,
			DefCount
		};

		struct MovedData
		{
			IBaseBuffer::Bhandle handle{ IBaseBuffer::INVALID_VHANDLE };
			VkDeviceAddress new_address{ 0 };
			std::uint32_t count{ 0 };
		};
		using AddressChangedCallback = std::function<void(const std::vector<MovedData>&)>;

	public:
		static IVulkanGlobalVerticesIndices& Instance();

		bool Init();
		void Destroy();

		GVIHandle AddVertexIndexData(const std::string& name, const IVertex* vertices, std::uint32_t vertex_count, const void* indices, std::uint32_t index_count);
		void ReleaseVertexIndexData(GVIHandle& handle);

		GVIHandle GetVertexIndexData(std::uint32_t record_index) const;
		// 默认几何: recordIndex 恒 INVALID_HANDLE
		GVIHandle GetDefVertexIndexData(DefMeshVertHandle h) const;


		void SetVertexAddressChangedCallback(AddressChangedCallback callback)
		{
			_address_changed_callback = std::move(callback);
		}
	private:

		GVIHandle _make_handle(std::uint32_t record_index) const;

		bool _is_budget_ok(VkDeviceSize bytes) const;
		std::uint32_t _acquire_record_slot();

	private:
		IBuffer<IVertex>* _default_vertex_buffer = nullptr;
		std::vector<IBuffer<IVertex>*> _vertex_buffers;
		IBuffer<std::uint16_t>* _default_index_buffer = nullptr;
		std::vector<IBuffer<std::uint16_t>*> _index_16_buffers;
		std::vector<IBuffer<std::uint32_t>*> _index_32_buffers;

		VkDeviceSize _shared_bytes{ 0 };

		std::vector<VertexIndexDataRecord*> _records;
		std::vector<std::uint32_t> _free_records;

		VertexIndexNameMap* _name_cache = nullptr;

		AddressChangedCallback  _address_changed_callback;
		mutable std::mutex _mutex;
	};
}
