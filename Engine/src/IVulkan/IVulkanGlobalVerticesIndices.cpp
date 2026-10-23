#include "IVulkan/IVulkanGlobalVerticesIndices.h"

#include "ILog.h"
#include "IVulkan/VulkanConfig.h"
#include "Memory/Memory.h"
#include "IMemPool/IMemPool.h"
#include "IEngineTools.h"

#include <new>
#include <utility>

namespace INVENT
{
	template<typename T, typename... Args>
	T* NewUseEngineAllocator(Args&& ...args)
	{
		void* ptr = EngineAllocator::Allocate(sizeof(T));
		return ::new(ptr) T(std::forward<Args>(args)...);
	}
	template<typename T>
	void DeleteUseEngineAllocator(T*& ptr)
	{
		if (ptr != nullptr)
		{
			ptr->~T();
			EngineAllocator::Deallocate(ptr);
			ptr = nullptr;
		}
	}

	IVulkanGlobalVerticesIndices& IVulkanGlobalVerticesIndices::Instance()
	{
		static IVulkanGlobalVerticesIndices v;
		return v;
	}

	bool IVulkanGlobalVerticesIndices::Init()
	{
		if (!IBaseBuffer::InitUploadContext()) return false;
		_default_vertex_buffer = NewUseEngineAllocator<IBuffer<IVertex>>(IVulkan::DEF_RESIDENT_VERTEX_BUFFER_COUNT);
		_default_index_buffer = NewUseEngineAllocator<IBuffer<std::uint16_t>>(IVulkan::DEF_RESIDENT_INDEX_BUFFER_COUNT);

		auto memPool = IEngineTools::Instance().GetMemPoolPool();
		if (memPool == nullptr) return false;
		_name_cache = new VertexIndexNameMap(64,
			std::hash<std::string>(),
			std::equal_to<std::string>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, std::uint32_t>>(memPool));

#if 0
		INVENT_LOG_DEBUG("[IVulkanGlobalVertices] Init().");
#endif

		if (!_init_def_meshes()) return false;

		LogStatus();
		return true;
	}

	void IVulkanGlobalVerticesIndices::Destroy()
	{
		DeleteUseEngineAllocator(_default_vertex_buffer);
		DeleteUseEngineAllocator(_default_index_buffer);

		for (auto& buffer : _vertex_buffers)
		{
			DeleteUseEngineAllocator(buffer);
		}
		for (auto& buffer : _index_16_buffers)
		{
			DeleteUseEngineAllocator(buffer);
		}
		for (auto& buffer : _index_32_buffers)
		{
			DeleteUseEngineAllocator(buffer);
		}
		if (_name_cache != nullptr)
		{
			delete _name_cache;
			_name_cache = nullptr;
		}
		for (auto r : _records)
		{
			DeleteUseEngineAllocator(r);
			_records.clear();
		}
		_def_meshes = {};
		_shared_bytes = 0;
		IBaseBuffer::DestroyUploadContext();
	}

	IVulkanGlobalVerticesIndices::GVIHandle IVulkanGlobalVerticesIndices::AddVertexIndexData(const std::string& name, const IVertex* vertices, std::uint32_t vertex_count, const void* indices, std::uint32_t index_count)
	{
		if (name.empty()) return {};

		std::lock_guard<std::mutex> lock(_mutex);

		auto iter = _name_cache->find(name);
		if (iter != _name_cache->end() && _records[iter->second] != nullptr)
		{
			++_records[iter->second]->share;
			return _make_handle(iter->second);
		}

		// add

		if (vertices == nullptr || vertex_count == 0 || indices == nullptr || index_count == 0) return {};

		const bool indexSizeUse32 = (vertex_count > 65535u);

		IBuffer<IVertex>* useVBuffer = nullptr;
		for (auto* vertexBuffer : _vertex_buffers)
		{
			if (vertexBuffer->GetCanAllocateCount() >= vertex_count)
			{
				useVBuffer = vertexBuffer;
				break;
			}
		}

		if (useVBuffer == nullptr)
		{
			const VkDeviceSize bytes = sizeof(IVertex) * IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
			if (!_is_budget_ok(bytes))
			{
				INVENT_LOG_ERROR(std::format("[GVI] 顶点空间不足且预算已满, 添加失败 (可先 Defragment). name: {}", name));
				return {};
			}
			try
			{
				useVBuffer = NewUseEngineAllocator<IBuffer<IVertex>>(IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT);
			}
			catch (const std::exception& e)
			{
				INVENT_LOG_WARNING(std::format("[GVI] 显存预算不足, 拒绝创建新 buffer. {}.", e.what()));
				return {};
			}
			_vertex_buffers.push_back(useVBuffer);
			_shared_bytes += bytes;
			INVENT_LOG_INFO(std::format("[GVI] 创建共享顶点 buffer ({} MB, 目前共享总计 {} MB).",
				static_cast<std::uint64_t>(bytes / (1024 * 1024)),
				static_cast<std::uint64_t>(_shared_bytes / (1024 * 1024))));
		}
		const auto vhandle = useVBuffer->AddDatas(vertices, vertex_count);
		if (vhandle == IBaseBuffer::INVALID_VHANDLE)
		{
			INVENT_LOG_ERROR(std::format("[GVI] mesh 顶点超过单 buffer 容量! name: {}, count: {}.", name, vertex_count));
			return {};
		}

		// index 

		IBaseBuffer::Bhandle ihandle = IBaseBuffer::INVALID_VHANDLE;
		IBuffer<std::uint16_t>* ibuffer16 = nullptr;
		IBuffer<std::uint32_t>* ibuffer32 = nullptr;

		if (indexSizeUse32)
		{
			for (auto* buffer : _index_32_buffers)
			{
				if (buffer->GetCanAllocateCount() >= index_count)
				{
					ibuffer32 = buffer;
					break;
				}
			}
			if (ibuffer32 == nullptr)
			{
				const VkDeviceSize bytes = sizeof(std::uint32_t) * IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
				if (!_is_budget_ok(bytes))
				{
					INVENT_LOG_ERROR(std::format("[GVI] 顶点空间不足且预算已满, 添加失败 (可先 Defragment). name: {}", name));
					return {};
				}
				try
				{
					ibuffer32 = NewUseEngineAllocator<IBuffer<std::uint32_t>>(IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT);
				}
				catch (const std::exception& e)
				{
					INVENT_LOG_WARNING(std::format("[GVI] 显存预算不足, 拒绝创建新 buffer. {}.", e.what()));
					return {};
				}
				_index_32_buffers.push_back(ibuffer32);
				_shared_bytes += bytes;
				INVENT_LOG_INFO(std::format("[GVI] 创建共享索引 uint32 buffer ({} MB, 目前共享总计 {} MB).",
					static_cast<std::uint64_t>(bytes / (1024 * 1024)),
					static_cast<std::uint64_t>(_shared_bytes / (1024 * 1024))));

				ihandle = ibuffer32->AddDatas(static_cast<const std::uint32_t*>(indices), index_count);
			}
		}
		else
		{
			for (auto* buffer : _index_16_buffers)
			{
				if (buffer->GetCanAllocateCount() >= index_count)
				{
					ibuffer16 = buffer;
					break;
				}
			}
			if (ibuffer16 == nullptr)
			{
				const VkDeviceSize bytes = sizeof(std::uint16_t) * IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT;
				if (!_is_budget_ok(bytes))
				{
					INVENT_LOG_ERROR(std::format("[GVI] 顶点空间不足且预算已满, 添加失败 (可先 Defragment). name: {}", name));
					return {};
				}
				try
				{
					ibuffer16 = NewUseEngineAllocator<IBuffer<std::uint16_t>>(IVulkan::DEF_ONE_BUFFER_VERTEX_COUNT);
				}
				catch (const std::exception& e)
				{
					INVENT_LOG_WARNING(std::format("[GVI] 显存预算不足, 拒绝创建新 buffer. {}.", e.what()));
					return {};
				}
				_index_16_buffers.push_back(ibuffer16);
				_shared_bytes += bytes;
				INVENT_LOG_INFO(std::format("[GVI] 创建共享索引 uint16 buffer ({} MB, 目前共享总计 {} MB).",
					static_cast<std::uint64_t>(bytes / (1024 * 1024)),
					static_cast<std::uint64_t>(_shared_bytes / (1024 * 1024))));

				ihandle = ibuffer16->AddDatas(static_cast<const std::uint16_t*>(indices), index_count);
			}
		}
		if (ihandle == IBaseBuffer::INVALID_VHANDLE)
		{
			useVBuffer->DestroyDatas(vhandle);
			INVENT_LOG_ERROR(std::format("[GVI] 索引空间不足, 添加失败. name: {}", name));
			return {};
		}
		const std::uint32_t idx = _acquire_record_slot();
		auto rec = NewUseEngineAllocator<VertexIndexDataRecord>(name, &_string_memory_pool);
		rec->vertexBuffer = useVBuffer;
		rec->vertexHandle = vhandle;
		rec->index16Buffer = ibuffer16;
		rec->index32Buffer = ibuffer32;
		rec->indexHandle = ihandle;
		rec->share = 1u;
		_records[idx] = rec;
		_name_cache->insert({ name, idx });

		return _make_handle(idx);
	}

	void IVulkanGlobalVerticesIndices::ReleaseVertexIndexData(GVIHandle& handle)
	{
		if (!handle.IsValid()) return;

		std::lock_guard<std::mutex> lock(_mutex);
		const auto idx = handle.recordIndex;
		handle = {};

		if (idx >= _records.size() || _records[idx] == nullptr) return;

		VertexIndexDataRecord* rec = _records[idx];
		if (rec->share > 0) --rec->share;
		if (rec->share > 0) return;		// 其他持有方仍在用

		if (rec->vertexBuffer && rec->vertexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			rec->vertexBuffer->DestroyDatas(rec->vertexHandle);
		}
		if (rec->index16Buffer && rec->indexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			rec->index16Buffer->DestroyDatas(rec->indexHandle);
		}
		if (rec->index32Buffer && rec->indexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			rec->index32Buffer->DestroyDatas(rec->indexHandle);
		}

		if (!rec->name.empty()) _name_cache->erase(rec->name.c_str());
		DeleteUseEngineAllocator(rec);
		_records[idx] = nullptr;
		_free_records.push_back(idx);
	}

	IVulkanGlobalVerticesIndices::GVIHandle IVulkanGlobalVerticesIndices::GetVertexIndexData(std::uint32_t record_index) const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (record_index >= _records.size() || _records[record_index] == nullptr) return {};
		return _make_handle(record_index);
	}

	IVulkanGlobalVerticesIndices::GVIHandle IVulkanGlobalVerticesIndices::GetDefVertexIndexData(DefMeshVertHandle h) const
	{
		const auto idx = static_cast<std::size_t>(h);
		const auto& m = _def_meshes[idx];
		GVIHandle out{};
		if (m.vertexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			const auto& d = _default_vertex_buffer->_get_datas()[m.vertexHandle];
			out.vertexAddress = d.BaseAddress;
		}
		if (m.indexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			const auto& d = _default_index_buffer->_get_datas()[m.indexHandle];
			out.indexAddress = d.BaseAddress;
			out.indexCount = d.Count;
		}

		return out;
	}

	template<typename T>
	bool anyVictim(const T& pool)
	{
		for (const auto* b : pool)
			if (b->CheckDefragment()) return true;
		return false;
	}

	std::shared_ptr<std::vector<IVulkanGlobalVerticesIndices::MovedData>> IVulkanGlobalVerticesIndices::Defragmanet()
	{
		auto moved = std::make_shared<std::vector<MovedData>>();

		std::lock_guard<std::mutex> lock(_mutex);

		if (!anyVictim(_vertex_buffers) && !anyVictim(_index_16_buffers) && !anyVictim(_index_32_buffers))
			return moved;

		std::vector<std::uint32_t> touched;
		_defragment_family(_vertex_buffers, &VertexIndexDataRecord::vertexBuffer, &VertexIndexDataRecord::vertexHandle, touched);
		_defragment_family(_index_16_buffers, &VertexIndexDataRecord::index16Buffer, &VertexIndexDataRecord::indexHandle, touched);
		_defragment_family(_index_32_buffers, &VertexIndexDataRecord::index32Buffer, &VertexIndexDataRecord::indexHandle, touched);
		if (touched.empty()) return moved;  // 全是洞的 Reset: 无地址变化, 无需通知
		for (std::uint32_t idx : touched)
		{
			const auto h = _make_handle(idx);	// 地址从记录现取, 必为搬移后的最新值
			moved->push_back({ idx, h.vertexAddress, h.indexAddress });
		}
		if (_address_changed_callback)
			_address_changed_callback(moved);
		return moved;
	}

	void IVulkanGlobalVerticesIndices::LogStatus() const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		auto log_pool = [](const char* tag, const auto& pool)
			{
				std::uint32_t i = 0;
				for (const auto* b : pool)
				{
					const std::uint32_t tail = b->GetMaxVertexCount() - b->GetCanAllocateCount();
					INVENT_LOG_INFO(std::format("[GVI] {} #{}: 容量 {}, 已分配 {}, 存活 {}, 碎片 {}.",
						tag, i++, b->GetMaxVertexCount(), tail, b->GetUsedCount(), tail - b->GetUsedCount()));
				}
			};
		const IBuffer<IVertex>* dv[] = { _default_vertex_buffer };
		const IBuffer<std::uint16_t>* di[] = { _default_index_buffer };
		log_pool("常驻顶点", dv);
		log_pool("常驻索引", di);
		log_pool("动态顶点", _vertex_buffers);
		log_pool("动态索引16", _index_16_buffers);
		log_pool("动态索引32", _index_32_buffers);
	}

	bool IVulkanGlobalVerticesIndices::_init_def_meshes()
	{
		constexpr auto F2 = static_cast<VertexOptionalFlags>(HasUV);
		constexpr auto F3 = static_cast<VertexOptionalFlags>(HasUV | HasNormal);
		auto pack_dir = [](const glm::vec3& d) -> glm::u16vec3
			{
				auto pack = [](float f) -> std::uint16_t
					{
						const int v = static_cast<int>((f * 0.5f + 0.5f) * 65535.0f);
						return static_cast<std::uint16_t>(std::clamp(v, 0, 65535));
					};
				return { pack(d.x), pack(d.y), pack(d.z) };
			};
		// DefQuad: 中心对齐单位四边形 (uv 左上原点; 与精灵约定不符则自行翻转 v)
		{
			IVertex q[4]{};
			const glm::vec3 pos[4] = { {-.5f,-.5f,0.f},{.5f,-.5f,0.f},{.5f,.5f,0.f},{-.5f,.5f,0.f} };
			const glm::vec2 uvs[4] = { {0,0},{1,0},{1,1},{0,1} };
			for (int i = 0; i < 4; ++i) { q[i].position = pos[i]; q[i].flags = F2; q[i].uv = uvs[i]; }
			const std::uint16_t idx[6] = { 0,1,2, 0,2,3 };
			auto& m = _def_meshes[static_cast<std::size_t>(DefMeshVertHandle::DefQuad)];
			m.vertexHandle = _default_vertex_buffer->AddDatas(q, 4);
			m.indexHandle = _default_index_buffer->AddDatas(idx, 6);
		}
		// DefCube: 6 面 x 4 顶点 + 36 索引 (绕序按外向 CCW 编写; 与剔除设置不符请调整)
		{
			IVertex cube[24]{};
			const glm::vec2 fuv[4] = { {0,0},{1,0},{1,1},{0,1} };
			struct Face { glm::vec3 n; glm::vec3 p[4]; };
			constexpr float P = 0.5f;
			const Face faces[6] = {
				{ { 0, 0, 1}, { { -P,-P, P }, {  P,-P, P }, {  P, P, P }, { -P, P, P } } },
				{ { 0, 0,-1}, { {  P,-P,-P }, { -P,-P,-P }, { -P, P,-P }, {  P, P,-P } } },
				{ { 1, 0, 0}, { {  P,-P, P }, {  P,-P,-P }, {  P, P,-P }, {  P, P, P } } },
				{ {-1, 0, 0}, { { -P,-P,-P }, { -P,-P, P }, { -P, P, P }, { -P, P,-P } } },
				{ { 0, 1, 0}, { { -P, P, P }, {  P, P, P }, {  P, P,-P }, { -P, P,-P } } },
				{ { 0,-1, 0}, { { -P,-P,-P }, {  P,-P,-P }, {  P,-P, P }, { -P,-P, P } } },
			};
			std::uint32_t vi = 0;
			for (const auto& f : faces)
			{
				const auto n = pack_dir(f.n);
				for (int c = 0; c < 4; ++c)
				{
					cube[vi].position = f.p[c];
					cube[vi].flags = F3;
					cube[vi].uv = fuv[c];
					cube[vi].normal = n;
					++vi;
				}
			}
			std::uint16_t idx[36];
			for (int f = 0; f < 6; ++f)
			{
				const auto b = static_cast<std::uint16_t>(f * 4);
				idx[f * 6 + 0] = b + 0; idx[f * 6 + 1] = b + 1; idx[f * 6 + 2] = b + 2;
				idx[f * 6 + 3] = b + 0; idx[f * 6 + 4] = b + 2; idx[f * 6 + 5] = b + 3;
			}
			auto& m = _def_meshes[static_cast<std::size_t>(DefMeshVertHandle::DefCube)];
			m.vertexHandle = _default_vertex_buffer->AddDatas(cube, 24);
			m.indexHandle = _default_index_buffer->AddDatas(idx, 36);
		}

		return true;
	}

	IVulkanGlobalVerticesIndices::GVIHandle IVulkanGlobalVerticesIndices::_make_handle(std::uint32_t record_index) const
	{
		auto* rec = _records[record_index];
		GVIHandle h;
		if (rec->vertexBuffer == nullptr || rec->vertexHandle == IBaseBuffer::INVALID_VHANDLE) return h;

		const auto& d = rec->vertexBuffer->_get_datas()[rec->vertexHandle];
		h.vertexAddress = d.BaseAddress;

		if (rec->index16Buffer && rec->indexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			const auto& d = rec->index16Buffer->_get_datas()[rec->vertexHandle];
			h.indexAddress = d.BaseAddress;
			h.indexCount = d.Count;
			h.use32BitIndex = false;
		}
		else if (rec->index32Buffer && rec->indexHandle != IBaseBuffer::INVALID_VHANDLE)
		{
			const auto& d = rec->index32Buffer->_get_datas()[rec->vertexHandle];
			h.indexAddress = d.BaseAddress;
			h.indexCount = d.Count;
			h.use32BitIndex = true;
		}
		else
		{
			return h;
		}

		h.recordIndex = record_index;
		return h;
	}

	bool IVulkanGlobalVerticesIndices::_is_budget_ok(VkDeviceSize bytes) const
	{
		auto allocator = IVulkanBase::Base().GetVmaAllocator();
		if (allocator == nullptr) return false;
		VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
		vmaGetHeapBudgets(allocator, budgets);
		const auto& props = IVulkanBase::Base().GetPhysicalDeviceMemoryProperties();
		for (std::uint32_t heap = 0; heap < props.memoryHeapCount; ++heap)
		{
			if (!(props.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) continue;
			const VkDeviceSize allowed = static_cast<VkDeviceSize>(budgets[heap].budget * IVulkan::MAX_VERTEX_INDEX_BUDGET_RATIO);
			if (_shared_bytes + bytes > allowed) return false;
			if (budgets[heap].usage + bytes > budgets[heap].budget) return false;
			return true;	// 只查第一个 device-local 堆 (顶点/索引 buffer 均为 DEDICATED device-local)
		}
		return true;
	}

	std::uint32_t IVulkanGlobalVerticesIndices::_acquire_record_slot()
	{
		if (!_free_records.empty())
		{
			const std::uint32_t idx = _free_records.back();
			_free_records.pop_back();
			return idx;		// _records[idx] == nullptr (释放时已置空)
		}
		_records.push_back(nullptr);
		return static_cast<std::uint32_t>(_records.size() - 1);
	}


}
