#include "IVulkan/IVulkanGlobalMaterial.h"

#include "IVulkan/VulkanConfig.h"
#include "IVulkan/VulkanBase.h"
#include "ILog.h"

#include <cstring>

namespace INVENT
{
	IVulkanGlobalMaterial& IVulkanGlobalMaterial::Instance()
	{
		static IVulkanGlobalMaterial m;
		return m;
	}

	bool IVulkanGlobalMaterial::Init()
	{
		if (_is_valid) return false;
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE)
		{
			INVENT_LOG_ERROR("[IVulkanGlobalMaterial] VulkanBase 未初始化!");
			return false;
		}

		if (!_add_one_buffer()) return false;

		_frame_stagings.resize(IVulkan::MAX_FRAMES_IN_FLIGHT);
		_dirty_flags.resize(4096, false);	// 初始去重容量, 随 handle 增长

		_is_valid = true;
		INVENT_LOG_INFO(std::format("[IVulkanGlobalMaterial] init done. 材质槽/buffer: {}.",
			IVulkan::DEF_MATERIAL_COUNT_PER_BUFFER));
		return true;
	}

	void IVulkanGlobalMaterial::Clear()
	{
		if (!_is_valid) return;
		auto& base = IVulkanBase::Base();

		// 契约: 渲染线程已停止 (in-flight 帧排空), 直接销毁安全
		for (auto& bd : _buffer_datas)
		{
			if (bd.GpuBuffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(bd.GpuBuffer);
			if (bd.Slots) { delete bd.Slots; bd.Slots = nullptr; }
			bd.CpuMirror.clear();
		}
		_buffer_datas.clear();

		for (auto& fs : _frame_stagings)
		{
			if (fs.Buffer != VK_NULL_HANDLE) base.UseVmaDestroyBuffer(fs.Buffer);
			fs = FrameStaging{};
		}
		_frame_stagings.clear();

		_datas.clear();
		_free_handles.clear();
		{ std::lock_guard<std::mutex> lk(_dirty_mutex); _dirty_handles.clear(); }
		_dirty_flags.clear();
		_is_valid = false;
	}

	IVulkanGlobalMaterial::IGMHandle IVulkanGlobalMaterial::AddMaterial(const Material& m)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_is_valid) return INVALID_IGM_HANDLE;

		// 1) 位图找空槽
		std::uint32_t bufferID = INVALID_32BIT;
		IHandle slot{};
		for (std::uint32_t i = 0; i < _buffer_datas.size(); ++i)
		{
			IHandle h{ _buffer_datas[i].Slots->FindFirstZero() };
			if (h.IsValid()) { bufferID = i; slot = h; break; }
		}

		// 2) 全满 -> 新建同尺寸 buffer (BDA 追加, 旧地址不受影响)
		if (bufferID == INVALID_32BIT)
		{
			if (!_add_one_buffer()) return INVALID_IGM_HANDLE;
			bufferID = static_cast<std::uint32_t>(_buffer_datas.size() - 1);
			slot = IHandle{ _buffer_datas[bufferID].Slots->FindFirstZero() };
			if (!slot.IsValid()) return INVALID_IGM_HANDLE;	// 理论不可达
		}

		// 3) handle (回收优先)
		IGMHandle handle;
		if (!_free_handles.empty())
		{
			handle = _free_handles.back();
			_free_handles.pop_back();
			_datas[handle] = ManagerData{ bufferID, slot };
		}
		else
		{
			handle = static_cast<IGMHandle>(_datas.size());
			_datas.push_back(ManagerData{ bufferID, slot });
			if (_dirty_flags.size() <= handle)
				_dirty_flags.resize(static_cast<size_t>(handle) + 1, false);
		}

		// 4) 占位 + 写镜像 + 标脏
		_buffer_datas[bufferID].Slots->SetValue<true>(slot);
		_buffer_datas[bufferID].CpuMirror[slot.GetRealIndex()] = m;
		_mark_dirty_nolock(handle);

		return handle;
	}

	void IVulkanGlobalMaterial::UpdateMaterial(IGMHandle handle, const Material& m)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_is_valid || handle >= _datas.size()) return;

		auto& d = _datas[handle];
		if (d.bufferID == INVALID_32BIT || d.bufferID >= _buffer_datas.size()) return;
		if (!_buffer_datas[d.bufferID].Slots->Get(d.materialID)) return;	// 已销毁

		_buffer_datas[d.bufferID].CpuMirror[d.materialID.GetRealIndex()] = m;
		_mark_dirty_nolock(handle);		// 原地更新: 地址不变, instance 无需刷新
	}

	void IVulkanGlobalMaterial::DestoryMaterial(IGMHandle handle)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_is_valid || handle >= _datas.size()) return;

		auto& d = _datas[handle];
		if (d.bufferID == INVALID_32BIT || d.bufferID >= _buffer_datas.size()) return;

		_buffer_datas[d.bufferID].Slots->SetValue<false>(d.materialID);	// 槽位回收
		// GPU 数据保留为墓碑, 槽位复用时被覆盖;
		// 引用此材质的 instance 由上层失效 (管理类不追踪反向引用)
		d = ManagerData{};
		_free_handles.push_back(handle);
	}

	VkDeviceAddress IVulkanGlobalMaterial::GetMaterialAddress(IGMHandle handle) const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_is_valid || handle >= _datas.size()) return 0;
		auto& d = _datas[handle];
		if (d.bufferID == INVALID_32BIT || d.bufferID >= _buffer_datas.size()) return 0;
		return _buffer_datas[d.bufferID].GpuAddress +
			static_cast<VkDeviceAddress>(d.materialID.GetRealIndex()) * sizeof(Material);
	}

	bool IVulkanGlobalMaterial::GetMaterial(IGMHandle handle, Material& out) const
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_is_valid || handle >= _datas.size()) return false;
		auto& d = _datas[handle];
		if (d.bufferID == INVALID_32BIT || d.bufferID >= _buffer_datas.size()) return false;
		if (!_buffer_datas[d.bufferID].Slots->Get(d.materialID)) return false;
		out = _buffer_datas[d.bufferID].CpuMirror[d.materialID.GetRealIndex()];
		return true;
	}

	void IVulkanGlobalMaterial::_mark_dirty_nolock(IGMHandle handle)
	{
		// _mutex 已持有; _dirty_flags 只随 _datas 增长 (Add 内), 此处读安全
		if (handle >= _dirty_flags.size() || _dirty_flags[handle]) return;
		_dirty_flags[handle] = true;
		std::lock_guard<std::mutex> dlock(_dirty_mutex);
		_dirty_handles.push_back(handle);
	}

	void IVulkanGlobalMaterial::FlushDirtyMaterials(VkCommandBuffer cmd, std::uint32_t frame_index)
	{
		if (!_is_valid || cmd == VK_NULL_HANDLE) return;
		if (frame_index >= _frame_stagings.size()) return;

		// 1) 取走脏集
		std::vector<IGMHandle> dirty;
		{
			std::lock_guard<std::mutex> dlock(_dirty_mutex);
			if (_dirty_handles.empty()) return;
			dirty.swap(_dirty_handles);
			// flags 的清位延后到拷贝成功之后 (失败回填用)
		}

		// 2) 锁内: 校验 + 镜像 -> staging + 收集拷贝区域 (按目标 buffer 分组)
		VkDeviceSize used = 0;
		const VkDeviceSize capacity = std::max<VkDeviceSize>(
			IVulkan::DEF_MATERIAL_DIRTY_STAGING_BYTES,
			static_cast<VkDeviceSize>(dirty.size()) * sizeof(Material));	// 单帧全脏也一次传完
		if (!_ensure_frame_staging(frame_index, capacity))
		{
			// staging 失败: 全部回填脏集, 下帧重试
			std::lock_guard<std::mutex> dlock(_dirty_mutex);
			_dirty_handles.insert(_dirty_handles.begin(), dirty.begin(), dirty.end());
			return;
		}

		// (bufferID, [(stagingOff, dstOff)]) — 目标 buffer 分组的拷贝区域
		struct Region { VkDeviceSize StagingOff, DstOff; };
		std::vector<std::pair<std::uint32_t, std::vector<Region>>> groups;
		auto find_group = [&](std::uint32_t id) -> std::vector<Region>&{
			for (auto& g : groups) if (g.first == id) return g.second;
			groups.emplace_back(id, std::vector<Region>{});
			return groups.back().second;
			};

		{
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto h : dirty)
			{
				if (h >= _datas.size()) { continue; }
				auto& d = _datas[h];
				if (d.bufferID == INVALID_32BIT || d.bufferID >= _buffer_datas.size()) { continue; }
				auto& bd = _buffer_datas[d.bufferID];
				if (!bd.Slots->Get(d.materialID)) { continue; }	// 已销毁: 跳过

				const std::uint32_t idx = static_cast<std::uint32_t>(d.materialID.GetRealIndex());
				std::memcpy(static_cast<std::byte*>(_frame_stagings[frame_index].Mapped) + used,
					&bd.CpuMirror[idx], sizeof(Material));
				find_group(d.bufferID).push_back({ used,
					static_cast<VkDeviceSize>(idx) * sizeof(Material) });
				used += sizeof(Material);
			}
		}

		// 全部无效 (都销毁了): 清 flags 返回
		if (used == 0)
		{
			std::lock_guard<std::mutex> dlock(_dirty_mutex);
			for (auto h : dirty) if (h < _dirty_flags.size()) _dirty_flags[h] = false;
			return;
		}

		// 3) 录制: 拷贝 + 读屏障
		VkBuffer staging = _frame_stagings[frame_index].Buffer;
		for (auto& [bufferID, regions] : groups)
		{
			auto& bd = _buffer_datas[bufferID];
			std::vector<VkBufferCopy> copies(regions.size());
			for (size_t i = 0; i < regions.size(); ++i)
			{
				copies[i].srcOffset = regions[i].StagingOff;
				copies[i].dstOffset = regions[i].DstOff;
				copies[i].size = sizeof(Material);
			}
			vkCmdCopyBuffer(cmd, staging, bd.GpuBuffer,
				static_cast<std::uint32_t>(copies.size()), copies.data());

			// transfer 写 -> BDA 读取 (顶点/片元/计算 都可能读材质)
			VkBufferMemoryBarrier2 b{};
			b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
				| VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
				| VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			b.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.buffer = bd.GpuBuffer;
			b.offset = 0;
			b.size = VK_WHOLE_SIZE;

			VkDependencyInfo di{};
			di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			di.bufferMemoryBarrierCount = 1;
			di.pBufferMemoryBarriers = &b;
			vkCmdPipelineBarrier2(cmd, &di);
		}

		// 4) 成功: 清去重 flags
		{
			std::lock_guard<std::mutex> dlock(_dirty_mutex);
			for (auto h : dirty) if (h < _dirty_flags.size()) _dirty_flags[h] = false;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

	bool IVulkanGlobalMaterial::_add_one_buffer()
	{
		auto& base = IVulkanBase::Base();
		if (base.GetDevice() == VK_NULL_HANDLE) return false;

		const VkDeviceSize size = static_cast<VkDeviceSize>(IVulkan::DEF_MATERIAL_COUNT_PER_BUFFER) * sizeof(Material);

		BufferData bd{};
		if (VkResult r = base.UseVmaCreateBuffer(size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			0,		// 纯 GPU 存取 (device local); 帧首脏批量上传
			bd.GpuBuffer))
		{
			INVENT_LOG_ERROR(std::format("[IVulkanGlobalMaterial] 材质 buffer 创建失败! VkResult: {}.", static_cast<std::int32_t>(r)));
			return false;
		}
		bd.GpuAddress = base.GetBufferDeviceAddress(bd.GpuBuffer);
		bd.CpuMirror.resize(IVulkan::DEF_MATERIAL_COUNT_PER_BUFFER);
		bd.Slots = new IBitVectorSafe();
		bd.Slots->ResizeBitCount(IVulkan::DEF_MATERIAL_COUNT_PER_BUFFER);

		_buffer_datas.push_back(std::move(bd));
		INVENT_LOG_INFO(std::format("[IVulkanGlobalMaterial] 新材质 buffer #{} ({} KB, BDA: {:#x}).",
			_buffer_datas.size() - 1,
			static_cast<std::uint64_t>(size / 1024),
			_buffer_datas.back().GpuAddress));
		return true;
	}

	bool IVulkanGlobalMaterial::_ensure_frame_staging(std::uint32_t frame_index, VkDeviceSize need)
	{
		auto& fs = _frame_stagings[frame_index];
		if (fs.Capacity >= need) return true;

		// 扩容安全: 本帧槽的上一次使用是 N-2 帧, BeginRender 的 fence 等待已保证其结束
		auto& base = IVulkanBase::Base();
		if (fs.Buffer != VK_NULL_HANDLE)
		{
			base.UseVmaDestroyBuffer(fs.Buffer);
			fs = FrameStaging{};
		}

		void* mapped = nullptr;
		if (VkResult r = base.UseVmaCreateBuffer(need,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	// ★ 一致内存 (纹理花屏的教训)
			fs.Buffer, &mapped))
		{
			INVENT_LOG_ERROR(std::format("[IVulkanGlobalMaterial] 帧槽 {} staging 扩容失败! VkResult: {}.",
				frame_index, static_cast<std::int32_t>(r)));
			fs.Buffer = VK_NULL_HANDLE;
			return false;
		}
		fs.Mapped = mapped;
		fs.Capacity = need;
		return true;
	}

}
