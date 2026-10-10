#pragma once

#include "VulkanConfig.h"
#include "IBuffer.h"

#include <string>
#include <vector>
#include <array>
#include <memory_resource>
#include <functional>
#include <memory>

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

		struct DefMeshData 
		{
			IBaseBuffer::Bhandle vertexHandle{ IBaseBuffer::INVALID_VHANDLE };
			IBaseBuffer::Bhandle indexHandle{ IBaseBuffer::INVALID_VHANDLE };
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
			std::uint32_t recordIndex{ INVALID_HANDLE };
			VkDeviceAddress newVertexAddress{ 0 };
			VkDeviceAddress newIndexAddress{ 0 };
		};
		using AddressChangedCallback = std::function<void(const std::shared_ptr<std::vector<MovedData>>)>;

	public:
		static IVulkanGlobalVerticesIndices& Instance();

		bool Init();
		void Destroy();

		GVIHandle AddVertexIndexData(const std::string& name, const IVertex* vertices, std::uint32_t vertex_count, const void* indices, std::uint32_t index_count);
		void ReleaseVertexIndexData(GVIHandle& handle);

		GVIHandle GetVertexIndexData(std::uint32_t record_index) const;
		// 默认几何: recordIndex 恒 INVALID_HANDLE
		GVIHandle GetDefVertexIndexData(DefMeshVertHandle h) const;

		std::shared_ptr<std::vector<MovedData>> Defragmanet();

		void SetVertexAddressChangedCallback(AddressChangedCallback callback)
		{
			_address_changed_callback = std::move(callback);
		}
		VkDeviceSize GetSharedBytes() const { return _shared_bytes; }
		void LogStatus() const;
	private:

		bool _init_def_meshes();

		GVIHandle _make_handle(std::uint32_t record_index) const;

		bool _is_budget_ok(VkDeviceSize bytes) const;
		std::uint32_t _acquire_record_slot();

		template<typename B>
		void _defragment_family(std::vector<B*>& family,
			B* VertexIndexDataRecord::* buf_member,
			IBaseBuffer::Bhandle VertexIndexDataRecord::* handle_member,
			std::vector<std::uint32_t>& touched);
	private:
		IBuffer<IVertex>* _default_vertex_buffer = nullptr;
		std::vector<IBuffer<IVertex>*> _vertex_buffers;
		IBuffer<std::uint16_t>* _default_index_buffer = nullptr;
		std::vector<IBuffer<std::uint16_t>*> _index_16_buffers;
		std::vector<IBuffer<std::uint32_t>*> _index_32_buffers;
		std::array<DefMeshData, static_cast<std::size_t>(DefMeshVertHandle::DefCount)> _def_meshes;

		VkDeviceSize _shared_bytes{ 0 };

		std::vector<VertexIndexDataRecord*> _records;
		std::vector<std::uint32_t> _free_records;

		VertexIndexNameMap* _name_cache = nullptr;

		AddressChangedCallback  _address_changed_callback;
		mutable std::mutex _mutex;
	};

	template<typename B>
	inline void IVulkanGlobalVerticesIndices::_defragment_family(std::vector<B*>& family, B* VertexIndexDataRecord::* buf_member, IBaseBuffer::Bhandle VertexIndexDataRecord::* handle_member, std::vector<std::uint32_t>& touched)
	{
		for (B* victim : family)
		{
			if (!victim->CheckDefragment()) continue;
			// 1) 收集 victim 内的存活记录
			std::vector<std::uint32_t> recs;
			for (std::uint32_t i = 0; i < _records.size(); ++i)
				if (_records[i] != nullptr && (_records[i]->*buf_member) == victim)
					recs.push_back(i);
			if (recs.empty())
			{
				victim->Reset();	// 全是洞: 纯记账清空
				continue;
			}
			// 2) 模拟装箱 (大->小, 排除 victim 自身) —— all-or-nothing, 装不下则零字节移动
			auto seg_count = [&](std::uint32_t ri) -> std::uint32_t
				{
					auto* r = _records[ri];
					return (r->*buf_member)->_get_datas()[r->*handle_member].Count;
				};
			std::sort(recs.begin(), recs.end(),
				[&](std::uint32_t a, std::uint32_t b) { return seg_count(a) > seg_count(b); });
			const auto victim_pos = static_cast<std::size_t>(
				std::find(family.begin(), family.end(), victim) - family.begin());
			std::vector<std::uint32_t> sim_free(family.size(), 0);
			for (std::size_t i = 0; i < family.size(); ++i)
				sim_free[i] = family[i]->GetCanAllocateCount();
			struct Plan { std::uint32_t recIdx; std::size_t destPos; };
			std::vector<Plan> plan;
			bool ok = true;
			for (std::uint32_t ri : recs)
			{
				const std::uint32_t count = seg_count(ri);
				std::size_t dest = SIZE_MAX;
				for (std::size_t i = 0; i < family.size(); ++i)
				{
					if (i == victim_pos) continue;	// 绝不能搬回 victim 自身
					if (sim_free[i] >= count) { dest = i; sim_free[i] -= count; break; }
				}
				if (dest == SIZE_MAX) { ok = false; break; }
				plan.push_back({ ri, dest });
			}
			if (!ok)
			{
				INVENT_LOG_TRACE("[GVI] 整理装箱失败, 该 buffer 本轮跳过.");
				continue;
			}
			// 3) GPU 搬移: graphics 队列一次性命令缓冲
			//    EndSingleTimeCommands 内部 vkQueueWaitIdle —— 返回时拷贝完成且飞行帧已排空,
			//    victim Reset 后的空间复用因此安全
			auto& base = IVulkanBase::Base();
			VkCommandBuffer cmd = base.BeginSingleTimeCommands();
			struct Commit { std::uint32_t recIdx; B* dest; IBaseBuffer::Bhandle newH; };
			std::vector<Commit> commits;
			std::vector<VkBuffer> written;
			constexpr VkDeviceSize elem = sizeof(typename B::pointType);
			for (const auto& p : plan)
			{
				auto* rec = _records[p.recIdx];
				const auto& src = (rec->*buf_member)->_get_datas()[rec->*handle_member];
				B* dest = family[p.destPos];
				const auto newH = dest->_add_datas_range(src.Count);	// 只分配记账, 数据由下方拷贝填入
				if (newH == IBaseBuffer::INVALID_VHANDLE) { ok = false; break; }	// 理论不可达(模拟已保证)
				const auto& dst = dest->_get_datas()[newH];
				VkBufferCopy copy{};
				copy.srcOffset = static_cast<VkDeviceSize>(src.Offset) * elem;
				copy.dstOffset = static_cast<VkDeviceSize>(dst.Offset) * elem;
				copy.size = static_cast<VkDeviceSize>(src.Count) * elem;
				vkCmdCopyBuffer(cmd, (rec->*buf_member)->_get_buffer(), dest->_get_buffer(), 1, &copy);
				if (std::find(written.begin(), written.end(), dest->_get_buffer()) == written.end())
					written.push_back(dest->_get_buffer());
				commits.push_back({ p.recIdx, dest, newH });
			}
			if (ok)
			{
				for (VkBuffer b : written)
					IBaseBuffer::RecordVisibilityBarrier(cmd, b);	// 拷贝写 -> 顶点/索引读取
				base.EndSingleTimeCommands(cmd);	// 同步等待
				// 4) 落账: 记录指向新位置 (recordIndex 不变, 只有地址变)
				for (const auto& c : commits)
				{
					auto* rec = _records[c.recIdx];
					rec->*buf_member = c.dest;
					rec->*handle_member = c.newH;
					if (std::find(touched.begin(), touched.end(), c.recIdx) == touched.end())
						touched.push_back(c.recIdx);
				}
				victim->Reset();	// 旧段随 Reset 全部作废 (字节保留至复用 = 墓碑)
				INVENT_LOG_INFO(std::format("[GVI] buffer 整理完成: 搬移 {} 份记录.", commits.size()));
			}
			else
			{
				// 部分已 AddDatasRange: 留下的是幽灵分配 (墓碑, 空间浪费但无数据损坏), 数据原样在 victim
				base.EndSingleTimeCommands(cmd);
			}
		}
	}
}
