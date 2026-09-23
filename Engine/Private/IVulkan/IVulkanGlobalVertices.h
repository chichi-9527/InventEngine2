#pragma once

#include "VulkanConfig.h"
#include "IVulkan/IVertexBuffer.h"

#include <string>
#include <vector>
#include <memory_resource>
#include <functional>

namespace INVENT
{
	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;

	class IVulkanGlobalVertices
	{
		std::pmr::monotonic_buffer_resource _string_memory_buffer{ 8ull * 1024 };
		std::pmr::unsynchronized_pool_resource _string_memory_pool{ &_string_memory_buffer };
		struct VertexDataRecord
		{
			VertexDataRecord(const std::string& n, std::pmr::memory_resource* pool)
				: name(n, pool) {}
			std::pmr::string name;
			std::uint32_t vertexBuffer{ UINT32_MAX };
		};

		using VertexNameMap = std::unordered_map < std::string,
			std::uint32_t,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, std::uint32_t>>>;

		IVulkanGlobalVertices() = default;
	public:
		struct VertexDataInfo
		{
			IVertexBuffer::Vhandle handle{ IVertexBuffer::INVALID_VHANDLE };
			VkDeviceAddress address{ 0 };	// BDA, 直接存进 mesh 数据
			std::uint32_t count{ 0 };
			bool IsValid() const { return handle != IVertexBuffer::INVALID_VHANDLE; }
		};

		enum class DefMeshVertHandle : std::uint32_t
		{
			DefQuad = 0,
			DefCube,
			DefCount
		};

		struct MovedVertexData
		{
			IVertexBuffer::Vhandle handle{ IVertexBuffer::INVALID_VHANDLE };
			VkDeviceAddress new_address{ 0 };
			std::uint32_t count{ 0 };
		};
		using VertexAddressChangedCallback = std::function<void(const std::vector<MovedVertexData>&)>;

	public:
		static IVulkanGlobalVertices& Instance();

		bool Init();
		void Destroy();

		VertexDataInfo AddVertexData(const std::string& name, const IVertex* vertices, std::uint32_t count);
		void ReleaseVertexData(IVertexBuffer::Vhandle handle);


		VertexDataInfo GetDefVertexDataInfo(DefMeshVertHandle h) const;


		void SetVertexAddressChangedCallback(VertexAddressChangedCallback callback)
		{
			_address_changed_callback = std::move(callback);
		}
	private:
		IVertexBuffer* _default_buffer = nullptr;
		std::vector<IVertexBuffer*> _buffers;

		std::vector<VertexDataRecord*> _records;

		VertexAddressChangedCallback  _address_changed_callback;

	};
}
