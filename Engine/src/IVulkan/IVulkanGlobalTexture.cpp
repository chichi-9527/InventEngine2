#include "IVulkan/IVulkanGlobalTexture.h"

#include "ILog.h"
#include "IEngineTools.h"
#include "IMemPool/IMemPool.h"
#include "IThread/IThreadPool.h"
#include "IVulkan/VulkanBase.h"
#include "IVulkan/ITieredImageMemoryManager2.h"
#include "Memory/Memory.h"

#include <dds_image/dds.hpp>

#include <stdexcept>
#include <filesystem>
#include <new>
#include <cstring>
#include <algorithm>

namespace { using TexMemManager = INVENT::ITieredImageMemoryManager2; }

namespace INVENT
{
	using Uint8Vector = std::vector<uint8_t, IMemPoolAllocatorOnlyFixedBlock<uint8_t>>;

	std::uint32_t IVulkanTexture2DManagement::GetDXGIFormatFromTypes(TextureType tex_type, CompressionType& com_type)
	{
		switch (tex_type)
		{
		case TextureType::TYPE_Undefined:
		case TextureType::TYPE_Deffuse:
		case TextureType::TYPE_Emission:
			if (com_type == CompressionType::BC1)
				return DXGI_FORMAT_BC1_UNORM_SRGB;
			if (com_type == CompressionType::BC3)
				return DXGI_FORMAT_BC3_UNORM_SRGB;
			if (com_type == CompressionType::BC6H)
				return DXGI_FORMAT_BC6H_SF16;
			if (com_type == CompressionType::BC7_RGB)
				return DXGI_FORMAT_BC7_UNORM_SRGB;
			if (com_type == CompressionType::BC7_RGBA)
				return DXGI_FORMAT_BC7_UNORM;
			//
			INVENT_LOG_WARNING("deffuse/emission 不能使用 BC4/BC5,将使用默认压缩: BC1");
			com_type = CompressionType::BC1;
			return DXGI_FORMAT_BC1_UNORM_SRGB;
			break;
		case TextureType::TYPE_Normal:
			if (com_type == CompressionType::BC5)
				return DXGI_FORMAT_BC5_UNORM;
			//
			INVENT_LOG_WARNING("normal 不能使用 BC1/BC3/BC4/BC6H/BC7,将使用默认压缩: BC5");
			com_type = CompressionType::BC5;
			return DXGI_FORMAT_BC5_UNORM;
			break;
		case TextureType::TYPE_Roughness:
		case TextureType::TYPE_AO:
		case TextureType::TYPE_Opacity:
		case TextureType::TYPE_Metallic:
			if (com_type == CompressionType::BC4)
				return DXGI_FORMAT_BC4_UNORM;
			//
			INVENT_LOG_WARNING("roughness/ao/opacity/matallic 不能使用 BC1/BC3/BC5/BC6H/BC7,将使用默认压缩: BC4");
			com_type = CompressionType::BC4;
			return DXGI_FORMAT_BC4_UNORM;
			break;
		case TextureType::TYPE_ORM:
			if (com_type == CompressionType::BC7_RGB)
				return DXGI_FORMAT_BC7_UNORM_SRGB;
			if (com_type == CompressionType::BC7_RGBA)
				return DXGI_FORMAT_BC7_UNORM;
			if (com_type == CompressionType::BC3)
				return DXGI_FORMAT_BC3_UNORM_SRGB;
			//
			INVENT_LOG_WARNING("orm 不能使用 BC1/BC4/BC5/BC6H,将使用默认压缩: BC7");
			com_type = CompressionType::BC7_RGB;
			return DXGI_FORMAT_BC7_UNORM_SRGB;
			break;
		case TextureType::TYPE_Specular:
			if (com_type == CompressionType::BC4)
				return DXGI_FORMAT_BC4_UNORM;
			if (com_type == CompressionType::BC6H)
				return DXGI_FORMAT_BC6H_SF16;
			if (com_type == CompressionType::BC7_RGB)
				return DXGI_FORMAT_BC7_UNORM_SRGB;
			if (com_type == CompressionType::BC7_RGBA)
				return DXGI_FORMAT_BC7_UNORM;
			//
			INVENT_LOG_WARNING("specular 不能使用 BC1/BC3/BC5,将使用默认压缩: BC4");
			com_type = CompressionType::BC4;
			return DXGI_FORMAT_BC4_UNORM;
			break;
		case TextureType::TYPE_ClearCoat:
			if (com_type == CompressionType::BC5)
				return DXGI_FORMAT_BC5_UNORM;
			if (com_type == CompressionType::BC6H)
				return DXGI_FORMAT_BC6H_SF16;
			if (com_type == CompressionType::BC7_RGB)
				return DXGI_FORMAT_BC7_UNORM_SRGB;
			if (com_type == CompressionType::BC7_RGBA)
				return DXGI_FORMAT_BC7_UNORM;
			//
			INVENT_LOG_WARNING("clear coat 不能使用 BC1/BC3/BC4,将使用默认压缩: BC5");
			com_type = CompressionType::BC5;
			return DXGI_FORMAT_BC5_UNORM;
			break;
		default:
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
		INVENT_LOG_WARNING("压缩格式不能作用于图片类型;例如 deffuse 不能使用 BC4/BC5.(例如不代表本次错误)");
		return DXGI_FORMAT_UNKNOWN;
	}

	IVulkanTexture2DManagement& IVulkanTexture2DManagement::Instance()
	{
		static IVulkanTexture2DManagement m;
		return m;
	}

	bool IVulkanTexture2DManagement::Init()
	{
		if (_is_valid) return false;

		_transfer_queue = IVulkanBase::Base().GetTransferQueue();
		if (_transfer_queue != VK_NULL_HANDLE)
			INVENT_LOG_TRACE("[IVulkanTexture2DManagement] have transfer queue.");
		else
			INVENT_LOG_TRACE("[IVulkanTexture2DManagement] NO have transfer queue.");

		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
		INVENT_LOG_INFO(std::format("[IVulkanTexture2DManagement] current bindless descriptor count: {}.", textureCount));

		_textures.resize(textureCount, IVulkanTexture2DHandle());
		_textures_data.resize(textureCount);
		_resize_stream_states(textureCount);
		_unreferenced_frames.assign(textureCount, 0);

		_bit_vector_used.ResizeBitCount(textureCount);
		_bit_vector_valid.ResizeBitCount(textureCount);

		_init_other();
		_init_default_image();
		//if (!ITieredImageMemoryManager::Init()) return false;
		if (!TexMemManager::Init()) return false;

		// 流式资源
		_init_transfer_resources();
		_init_mip_feedback(static_cast<std::uint32_t>(textureCount));

		_upload_pool = new IThreadPool();
		if (_upload_pool == nullptr)
		{
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] 传输线程池不可用! 纹理流式加载被禁用.");
			return false;
		}
		else if (_transfer_command_pool == VK_NULL_HANDLE)
		{
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] 无专用传输队列! 纹理流式加载被禁用.");
			return false;
		}
		else
			INVENT_LOG_INFO("[IVulkanTexture2DManagement] 纹理流式加载已启用.");

		_upload_pool->Start();

		_is_valid = true;
		return true;
	}

	void IVulkanTexture2DManagement::Clear()
	{
		if (!_is_valid) return;

		// 1) 先停上传任务 (可能正在创建 image / 读 DDS 数据)
		_stop_upload();
		if (_upload_pool)
		{
			_upload_pool->Shutdown();
			delete _upload_pool;
			_upload_pool = nullptr;
		}

		// 2) 槽位纹理 (流式窗口 image)
		_bit_vector_used.FastForEachOne([this](size_t index) {
			auto& tex = _textures[index];
			if (tex.ImageView != VK_NULL_HANDLE)
				IVulkanBase::Base().DestroyImageView(tex.ImageView);
			if (tex.Image != VK_NULL_HANDLE)
			{
				TexMemManager::DestroyVkImage(tex.Image);
			}
			tex = IVulkanTexture2DHandle{};
			});

		// 3) 上传任务已创建但主线程未消费的窗口 image
		{
			std::lock_guard<std::mutex> lock(_completed_mutex);
			for (auto& c : _completed_uploads)
			{
				if (c.image != VK_NULL_HANDLE)
				{
					TexMemManager::DestroyVkImage(c.image);
				}
			}
				
			_completed_uploads.clear();
		}
		_pending_acquires.clear();

		// 在途窗口 (stop 路径可能残留; _stop_upload 已排空 fence, 销毁安全)
		for (auto& w : _pending_windows)
		{
			if (w.image != VK_NULL_HANDLE)
			{
				TexMemManager::DestroyVkImage(w.image);
			}
		}
		_pending_windows.clear();
		// 未冲刷完成项 (防御性, 正常路径 _round_done 已被排空)
		for (auto& rd : _round_done)
		{
			for (auto& c : rd)
			{
				if (c.image != VK_NULL_HANDLE)
				{
					TexMemManager::DestroyVkImage(c.image);
				}
			}
			rd.clear();
		}
		_upload_round_index = 0;

		// 4) 帧延迟销毁环中待销毁资源
		{
			std::lock_guard<std::mutex> lock(_destroy_mutex);
			for (auto& ring : _frame_destroy_ring)
			{
				for (auto& it : ring)
				{
					if (it.ImageView != VK_NULL_HANDLE)
						IVulkanBase::Base().DestroyImageView(it.ImageView);
					if (it.Image != VK_NULL_HANDLE)
					{
						TexMemManager::DestroyVkImage(it.Image);
					}
				}
				ring.clear();
			}
		}

		// 5) 默认贴图
		for (auto& dt : _default_textures)
		{
			if (dt.ImageView != VK_NULL_HANDLE)
				IVulkanBase::Base().DestroyImageView(dt.ImageView);
			if (dt.Image != VK_NULL_HANDLE)
				IVulkanBase::Base().UseVmaDestroyImage(dt.Image);
		}
		_default_textures.clear();

		// 6) CPU 侧 DDS 数据 (placement new 的配对析构)
		for (auto& td : _textures_data)
		{
			if (td.Data.data)
			{
				td.Data.data->~Uint8Vector();
				EngineAllocator::Deallocate(td.Data.data);
				td.Data.data = nullptr;
			}
			if (td.Data.offsets)
			{
				td.Data.offsets->clear();
				EngineAllocator::Deallocate(td.Data.offsets);
				td.Data.offsets = nullptr;
			}
			td.Data = {};
			td.Format = VK_FORMAT_UNDEFINED;
			td.Type = TextureType::TYPE_Undefined;
		}

		// 7) 流式资源
		_destroy_mip_feedback();
		_destroy_transfer_resources();
		

		// 8) 复位状态
		_stream_states.reset();
		_stream_states_count = 0;
		_unreferenced_frames.clear();
		_upload_pool = nullptr;
		if (_texture_name_cache) _texture_name_cache->clear();
		if (_texture_handle_name_cache) _texture_handle_name_cache->clear();
		_bit_vector_used.ResetBitToZero();
		_bit_vector_valid.ResetBitToZero();
		_destroy_cursor = 0;
		_frame_counter = 0;

		_is_valid = false;
	}

	void IVulkanTexture2DManagement::Terminate()
	{
		if (_texture_name_cache)
		{
			delete _texture_name_cache;
			_texture_name_cache = nullptr;
		}
		if (_texture_handle_name_cache)
		{
			delete _texture_handle_name_cache;
			_texture_handle_name_cache = nullptr;
		}
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AllocateTextureHandle()
	{
		IHandle handle = _bit_vector_used.FindFirstZero();
		if (!handle.IsValid())
		{
			if (!IVulkanBase::Base().ResizeBindlessDescriptorPoolAndGobalSet())
			{
				return Texture2DHandle{};
			}

			_update_texture_count();
			handle = _bit_vector_used.FindFirstZero();
		}

		_bit_vector_used.SetValue<true>(handle);

		return { static_cast<std::uint32_t>(handle.GetRealIndex()) };
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::AddTexture2D(const std::string& name, const std::string& path,
		TextureType texture_type)
	{
		std::string texName = name;
		if (name.empty())
		{
			INVENT_LOG_WARNING(std::format("[IVulkanTexture2DManagement] name is empty; path : {}", path));
			texName = "Empty";
		}

		Texture2DHandle handle = _find_handle_from_cache(name);
		if (handle.IsValid())
			return handle;
		handle = AllocateTextureHandle();
		if (!handle.IsValid())
		{
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] 纹理数量已达到上限.");
			return handle;
		}

		// 记录纹理类型 (驱逐时用于重绑默认贴图); 主线程结构修改, 无需加锁
		_textures_data[handle.slot].Type = texture_type;
		// 捕获代数: 防止等待中的 DDS 工作线程在槽位销毁/复用后再写入
		const auto gen = _stream_states[handle.slot].Generation.load(std::memory_order_relaxed);

		switch (texture_type)
		{
		case TextureType::TYPE_Undefined:
		case TextureType::TYPE_Deffuse:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::S_White].ImageView);
			break;
		case TextureType::TYPE_Emission:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::S_Black].ImageView);
			break;
		case TextureType::TYPE_Normal:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::NormalBlue].ImageView);
			break;
		case TextureType::TYPE_Roughness:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::U8_128].ImageView);
			break;
		case TextureType::TYPE_AO:
		case TextureType::TYPE_Opacity:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::U_White].ImageView);
			break;
		case TextureType::TYPE_Metallic:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::U_Black].ImageView);
			break;
		case TextureType::TYPE_ORM:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::ORM].ImageView);
			break;
		case TextureType::TYPE_Specular:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::U8_128].ImageView);
			break;
		case TextureType::TYPE_ClearCoat:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::U_Black].ImageView);
			break;
		default:
			IVulkanBase::Base().UpdateBindlessTextureSlot(handle.slot, _default_textures[DefaultTextureType::S_White].ImageView);
			break;
		}

		if (!std::filesystem::exists(path))
		{
			INVENT_LOG_WARNING(std::format("[IVulkanTexture2DManagement] texture is not found; path : {}", std::filesystem::path{ IEngineTools::GetRunStdPath() / path }.string()));
			return handle;
		}

		IEngineTools::Instance().GetWorkThreadPool()->Submit(0, [this, handle, name, path, gen]() {

			auto pool = IEngineTools::Instance().GetMemPoolPool();

			// 共享锁: 与 resize / DestroyTexture2D 的独占锁互斥,
			// 防止本线程读/写元素期间 vector 发生重分配搬移
			std::shared_lock<std::shared_mutex> lock(_textures_mutex);
			auto& s = _stream_states[handle.slot];
			if (s.Generation.load(std::memory_order_relaxed) != gen ||
				s.Destroyed.load(std::memory_order_relaxed))
			{
				return;	// 等待期间槽位已被销毁, 放弃
			}

			auto& td = _textures_data[handle.slot];
			if (!td.Data)
			{
				void* dataptr = EngineAllocator::Allocate(sizeof(ITextureCompresser::Uint8Vector));
				td.Data.data = ::new(dataptr) ITextureCompresser::Uint8Vector{ IMemPoolAllocatorOnlyFixedBlock<uint8_t>(pool) };
				void* offsetptr = EngineAllocator::Allocate(sizeof(ITextureCompresser::Uint32Vector));
				td.Data.offsets = ::new(offsetptr) ITextureCompresser::Uint32Vector{ IMemPoolAllocatorOnlyFixedBlock<uint32_t>(pool) };
			}

			if (!_load_dds_to_compressed_data(path, LoadDDSType::Auto, td.Data, td.Format))
			{
				INVENT_LOG_WARNING(std::format("[IVulkanTexture2DManagement] 读取dds文件出现了错误,路径: {}", std::filesystem::path{ IEngineTools::GetRunStdPath() / path }.string()));
				return;
			}

			// release: 上传任务对 TotalMips 的 acquire 可看到完整 DDS 数据
			s.TotalMips.store(td.Data.mipLevels, std::memory_order_release);

			// 锁内设置 valid: 与销毁路径的独占锁形成全序, 避免销毁后残留脏 valid 位
			_bit_vector_valid.SetValue<true>(IHandle{ handle.slot });
			lock.unlock();

			INVENT_LOG_TRACE(std::format("[IVulkanTexture2DManagement] read dds file done. path: {}", std::filesystem::path{ IEngineTools::GetRunStdPath() / path }.string()));

			// 自动驻留最粗层 (约一个 BC 块, 代价可忽略), 保证贴图不再是占位符;
			// 之后真实需求由视锥裁剪 CS 的反馈或 UpdateTexture2D 驱动
			if (_auto_initial_load)
				_request_retarget(handle.slot, td.Data.mipLevels - 1);
			});

		_insert_name_cache(name, handle);
		return handle;
	}

	void IVulkanTexture2DManagement::UpdateTexture2D(Texture2DHandle& handle, std::uint32_t needMipLevel)
	{
		if (!handle.IsValid() || !_is_valid) return;
		if (handle.slot >= _stream_states_count) return;
		auto& s = _stream_states[handle.slot];
		if (s.Destroyed.load(std::memory_order_relaxed)) return;

		const std::uint32_t total = s.TotalMips.load(std::memory_order_acquire);
		if (total == 0)
		{
			// DDS 尚未就绪; 就绪后会自动驻留最粗层
			return;
		}
		if (needMipLevel >= total) needMipLevel = total - 1;

		_request_retarget(handle.slot, needMipLevel);
	}

	void IVulkanTexture2DManagement::DestroyTexture2D(Texture2DHandle handle)
	{
		if (!handle.IsValid() || handle.slot >= _stream_states_count) return;
		auto& s = _stream_states[handle.slot];
		s.Destroyed.store(true, std::memory_order_release);

		// 等待上传任务结束 (它可能正在读本槽 DDS 数据 / 写窗口 image)
		WaitForUploadIdle();

		{
			std::unique_lock<std::shared_mutex> lock(_textures_mutex);
			// GPU 资源走帧环延迟销毁 (in-flight 帧可能还在采样)
			auto& t = _textures[handle.slot];
			QueueDestroy(t.Image, t.ImageView);
			t = IVulkanTexture2DHandle{};

			// CPU 侧 DDS 数据
			auto& td = _textures_data[handle.slot];
			if (td.Data.data)
			{
				td.Data.data->~Uint8Vector();
				EngineAllocator::Deallocate(td.Data.data);
				td.Data.data = nullptr;
			}
			if (td.Data.offsets)
			{
				td.Data.offsets->clear();
				EngineAllocator::Deallocate(td.Data.offsets);
				td.Data.offsets = nullptr;
			}
			td.Data = {};
			td.Format = VK_FORMAT_UNDEFINED;
			td.Type = TextureType::TYPE_Undefined;
		}

		// 完整复位流式状态 (供槽位复用)
		++_stream_states[handle.slot].Generation;
		s.TotalMips.store(0, std::memory_order_relaxed);
		s.ResidentBase.store(UINT32_MAX, std::memory_order_relaxed);
		s.TargetBase.store(UINT32_MAX, std::memory_order_relaxed);
		s.DesiredMip.store(UINT32_MAX, std::memory_order_relaxed);
		s.Pinned.store(false, std::memory_order_relaxed);
		s.Destroyed.store(false, std::memory_order_relaxed);
		_unreferenced_frames[handle.slot] = 0;

		_bit_vector_valid.SetValue<false>(IHandle{ handle.slot });
		_bit_vector_used.SetValue<false>(IHandle{ handle.slot });

		_remove_name_cache_by_handle(handle);
	}

	void IVulkanTexture2DManagement::QueueDestroy(VkImage image, VkImageView image_view)
	{
		if (image == VK_NULL_HANDLE && image_view == VK_NULL_HANDLE)
			return;
		// 进入当前帧的环槽位, N+1 帧后真正销毁
		std::lock_guard<std::mutex> lock(_destroy_mutex);
		_frame_destroy_ring[_destroy_cursor].push_back({ image, image_view });
	}

	void IVulkanTexture2DManagement::FlushDestroyQueue()
	{
		if (!_is_valid) return;

		++_frame_counter;
		{
			std::lock_guard<std::mutex> lock(_destroy_mutex);
			_destroy_cursor = (_destroy_cursor + 1) % (IVulkan::MAX_FRAMES_IN_FLIGHT + 1);
			auto& items = _frame_destroy_ring[_destroy_cursor];	// 恰好 N+1 帧前入队的资源
			for (auto& it : items)
			{
				if (it.ImageView != VK_NULL_HANDLE)
					IVulkanBase::Base().DestroyImageView(it.ImageView);
				if (it.Image != VK_NULL_HANDLE)
				{
					TexMemManager::DestroyVkImage(it.Image);
				}
			}
			items.clear();
		}

		// 反馈缓冲扩容后的旧缓冲延迟销毁
		while (!_retired_buffers.empty() &&
			_frame_counter - _retired_buffers.front().first > IVulkan::MAX_FRAMES_IN_FLIGHT)
		{
			IVulkanBase::Base().UseVmaDestroyBuffer(_retired_buffers.front().second);
			_retired_buffers.pop_front();
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 每帧主线程处理
	//////////////////////////////////////////////////////////////////////////////////////

	void IVulkanTexture2DManagement::ProcessMipFeedback(std::uint32_t frame_index)
	{
		if (!_is_valid) return;
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;
		if (_mip_readback_mapped[frame_index] == nullptr) return;

		std::uint32_t demote_budget = _demote_per_frame;

		// host-visible 非一致内存读取前需要 invalidate (一致内存为无害空操作)
		IVulkanBase::Base().UseVmaInvalidateAllocationBuffer(_mip_readback_buffers[frame_index]);
		const std::uint32_t* fb = static_cast<const std::uint32_t*>(_mip_readback_mapped[frame_index]);

		const size_t count = std::min(_stream_states_count, static_cast<size_t>(_mip_feedback_count));
		for (std::uint32_t slot = 0; slot < count; ++slot)
		{
			auto& s = _stream_states[slot];
			if (s.Destroyed.load(std::memory_order_relaxed)) continue;

			const std::uint32_t total = s.TotalMips.load(std::memory_order_acquire);
			if (total == 0) continue;

			const std::uint32_t desiredRaw = fb[slot];
			if (desiredRaw == UINT32_MAX)
			{
				// 本轮无任何可见实例引用: 驱逐统计
				// 条件: 已驻留 / 无在途上传 / 未固定 / 连续未引用达到阈值
				if (s.ResidentBase.load(std::memory_order_relaxed) != UINT32_MAX &&
					s.TargetBase.load(std::memory_order_relaxed) == UINT32_MAX &&
					!s.Pinned.load(std::memory_order_relaxed) &&
					++_unreferenced_frames[slot] >= _evict_frame_threshold)
				{
					_evict_texture(slot);
				}
				continue;
			}
			_unreferenced_frames[slot] = 0;

			const std::uint32_t desired = std::min(desiredRaw, total - 1);
			s.DesiredMip.store(desired, std::memory_order_relaxed);

			const std::uint32_t resident = s.ResidentBase.load(std::memory_order_relaxed);
			if (resident == UINT32_MAX)
			{
				_request_retarget(slot, desired);			// 首次驻留 / 驱逐后重新驻留
			}
			else if (desired < resident)
			{
				// 升级: 需要更精细 (允许预取 margin 层)
				const std::uint32_t target = (desired > _upgrade_margin) ? desired - _upgrade_margin : 0;
				_request_retarget(slot, target);
			}
			else if (desired > resident + _demote_hysteresis && demote_budget > 0)
			{
				// 降级: 明显变粗才降 (滞后 + 每帧限额, 防抖动)
				--demote_budget;
				_request_retarget(slot, desired);
			}
		}
	}

	void IVulkanTexture2DManagement::ProcessCompletedUploads()
	{
		if (!_is_valid) return;

		std::vector<CompletedUpload> items;
		{
			std::lock_guard<std::mutex> lock(_completed_mutex);
			items.swap(_completed_uploads);
		}
		if (items.empty()) return;

		auto& base = IVulkanBase::Base();
		for (auto& c : items)
		{
			// 上传期间槽位被销毁: 丢弃窗口 image
			if (c.slot >= _stream_states_count ||
				_stream_states[c.slot].Destroyed.load(std::memory_order_relaxed))
			{
				QueueDestroy(c.image, VK_NULL_HANDLE);
				continue;
			}
			auto& s = _stream_states[c.slot];

			// 清除目标; 若上传期间有更精细的请求覆盖了它则保留, 下批继续
			std::uint32_t t = c.new_base;
			s.TargetBase.compare_exchange_strong(t, UINT32_MAX, std::memory_order_acq_rel);

			VkFormat format = VK_FORMAT_UNDEFINED;
			{
				std::shared_lock<std::shared_mutex> lock(_textures_mutex);
				format = _textures_data[c.slot].Format;
			}

			// 窗口 view: baseMip 恒为 0, 覆盖整张窗口 image
			VkImageView view = base.CreateImageView(c.image, format, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_VIEW_TYPE_2D, c.total_mips - c.new_base);
			if (view == VK_NULL_HANDLE)
			{
				INVENT_LOG_ERROR("[IVulkanTexture2DManagement] [Streaming] 创建窗口 ImageView 失败!");
				QueueDestroy(c.image, VK_NULL_HANDLE);
				continue;
			}

			VkImage oldImage = VK_NULL_HANDLE;
			VkImageView oldView = VK_NULL_HANDLE;
			{
				std::unique_lock<std::shared_mutex> lock(_textures_mutex);
				auto& slot = _textures[c.slot];
				oldImage = slot.Image;
				oldView = slot.ImageView;
				slot.Image = c.image;
				slot.ImageView = view;
				slot.CurrentMaxMipLevel = c.new_base;	// = ResidentBase
			}
			if (oldImage != VK_NULL_HANDLE || oldView != VK_NULL_HANDLE)
				QueueDestroy(oldImage, oldView);		// in-flight 帧可能还在采样旧 view

			s.ResidentBase.store(c.new_base, std::memory_order_release);

			// update-after-bind 描述符更新, 正在执行的帧继续用旧 view (仍存活)
			base.UpdateBindlessTextureSlot(c.slot, view);

			// 本帧命令缓冲开头需要补 acquire barrier
			_pending_acquires.push_back(c);

			// 通知材质: baseMip -> 材质 BDA (先更新描述符再回调, 保证新值对应新 view)
			if (_resident_changed_callback)
				_resident_changed_callback(Texture2DHandle{ c.slot },
					c.new_base, c.total_mips - c.new_base, c.total_mips);
		}
	}

	void IVulkanTexture2DManagement::_evict_texture(std::uint32_t slot)
	{
		auto& s = _stream_states[slot];

		VkImage img = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		{
			std::unique_lock<std::shared_mutex> lock(_textures_mutex);
			auto& t = _textures[slot];
			img = t.Image;
			view = t.ImageView;
			t = IVulkanTexture2DHandle{};	// CurrentMaxMipLevel = UINT32_MAX
		}
		s.ResidentBase.store(UINT32_MAX, std::memory_order_release);
		_unreferenced_frames[slot] = 0;

		if (img != VK_NULL_HANDLE || view != VK_NULL_HANDLE)
			QueueDestroy(img, view);

		// 重绑默认贴图占位
		TextureType type;
		{
			std::shared_lock<std::shared_mutex> lock(_textures_mutex);
			type = _textures_data[slot].Type;
		}
		IVulkanBase::Base().UpdateBindlessTextureSlot(slot,
			_default_textures[_get_default_texture_index(type)].ImageView);

		if (_resident_changed_callback)
			_resident_changed_callback(Texture2DHandle{ slot },
				UINT32_MAX, 0, s.TotalMips.load(std::memory_order_relaxed));
	}

	void IVulkanTexture2DManagement::_begin_pending_windows(const std::vector<std::uint32_t>& slots)
	{
		if (_upload_fences.empty()) return;
		for (std::uint32_t slot : slots)
		{
			if (slot >= _stream_states_count) continue;
			auto& s = _stream_states[slot];
			if (s.Destroyed.load(std::memory_order_relaxed)) continue;
			const std::uint32_t target = s.TargetBase.load(std::memory_order_acquire);
			const std::uint32_t total = s.TotalMips.load(std::memory_order_acquire);
			if (target == UINT32_MAX || total == 0) continue;
			// 已有同槽在途窗口?
			std::size_t wi = SIZE_MAX;
			for (std::size_t i = 0; i < _pending_windows.size(); ++i)
				if (_pending_windows[i].slot == slot) { wi = i; break; }
			if (wi != SIZE_MAX)
			{
				auto& w = _pending_windows[wi];
				if (target < w.base)
				{
					// 更精细的目标: 先排空在途轮次(旧窗口可能有已提交的拷贝), 再放弃重建
					_drain_all_round_fences();
					TexMemManager::DestroyVkImage(w.image);
					_pending_windows.erase(_pending_windows.begin() + wi);
				}
				else
				{
					// 在途窗口已至少满足该目标: 清掉请求即可
					std::uint32_t t = target;
					s.TargetBase.compare_exchange_strong(t, UINT32_MAX, std::memory_order_acq_rel);
					continue;
				}
			}
			else
			{
				const std::uint32_t resident = s.ResidentBase.load(std::memory_order_relaxed);
				if (resident != UINT32_MAX && resident <= target)
				{
					std::uint32_t t = target;
					s.TargetBase.compare_exchange_strong(t, UINT32_MAX, std::memory_order_acq_rel);
					continue;	// 已驻留足够精细
				}
			}
			// 校验 DDS + 计算实际起点 start
			VkFormat format = VK_FORMAT_UNDEFINED;
			std::uint32_t start = target;
			std::uint32_t width = 0, height = 0;
			bool valid = false;
			{
				std::shared_lock<std::shared_mutex> lock(_textures_mutex);
				const auto& td = _textures_data[slot];
				if (td.Data && td.Data.data && td.Data.offsets &&
					td.Data.mipLevels == total && td.Data.offsets->size() == total)
				{
					const auto& offsets = *td.Data.offsets;
					const auto dataSize = static_cast<VkDeviceSize>(td.Data.data->size());
					const VkDeviceSize stagingSize = TexMemManager::GetStagingBufferSize();
					auto mipSize = [&](std::uint32_t m) -> VkDeviceSize {
						return (m + 1 < total) ? (offsets[m + 1] - offsets[m])
							: (dataSize - offsets[m]);
						};
					// 单层就超过整块 staging: 退到更粗的层开始 (超大纹理自然降级, 不重建缓冲)
					while (start + 1 < total && mipSize(start) > stagingSize) ++start;
					valid = (mipSize(start) <= stagingSize);
					format = td.Format;
					width = td.Data.width;
					height = td.Data.height;
				}
			}
			if (!valid)
			{
				INVENT_LOG_WARNING(std::format("[IVulkanTexture2DManagement] [Streaming] slot {} 的 DDS 数据异常, 放弃本次上传.", slot));
				std::uint32_t t = target;
				s.TargetBase.compare_exchange_strong(t, UINT32_MAX);
				continue;
			}
			// 创建窗口 image: 尺寸 = start 层尺寸, 层数 = total - start
			TexMemManager::ICreateImageInfo info{};
			info.ImageFormat = format;
			info.ImageWidth = std::max(1u, width >> start);
			info.ImageHeight = std::max(1u, height >> start);
			info.MipLevels = total - start;
			VkImage image = VK_NULL_HANDLE;
			if (VkResult r = TexMemManager::CreateVkImage(image, info))
			{
				INVENT_LOG_WARNING(std::format("[IVulkanTexture2DManagement] [Streaming] 创建窗口 VkImage 失败(显存预算?), slot {}, VkResult {}.",
					slot, static_cast<std::int32_t>(r)));
				std::uint32_t t = target;
				s.TargetBase.compare_exchange_strong(t, UINT32_MAX);	// 下帧重试
				continue;
			}
			_pending_windows.push_back({ slot, image, format, start, total, start, false });
		}
	}

	void IVulkanTexture2DManagement::_upload_round()
	{
		if (_transfer_command_pool == VK_NULL_HANDLE || _transfer_queue == VK_NULL_HANDLE ||
			_upload_fences.empty() || _pending_windows.empty())
		{
			if (!_pending_windows.empty())
				_abandon_pending_windows("[IVulkanTexture2DManagement] 传输资源不可用");
			return;
		}

		const std::uint32_t poolCount = static_cast<std::uint32_t>(_upload_fences.size());
		const std::uint32_t pool = static_cast<std::uint32_t>(_upload_round_index % poolCount);

		// 1) 等待 N 轮前使用同一块 staging 的传输完成 (顺带释放上一轮命令缓冲 + 冲刷完成项)
		if (!_wait_round_fence(pool))
		{
			_abandon_pending_windows("[IVulkanTexture2DManagement] fence 等待失败");
			return;
		}

		// 2) offset 归零, 开启新一轮
		TexMemManager::ResetStagingBuffer(pool);

		VkCommandBuffer cmd = _begin_transfer_command();
		if (cmd == VK_NULL_HANDLE)
		{
			_abandon_pending_windows("[IVulkanTexture2DManagement] 命令缓冲分配失败");
			return;
		}

		std::vector<CompletedUpload> done;
		bool anyRecorded = false;

		for (std::size_t wi = 0; wi < _pending_windows.size(); )
		{
			auto& w = _pending_windows[wi];

			// 槽位已销毁: 放弃该窗口
			if (w.slot >= _stream_states_count ||
				_stream_states[w.slot].Destroyed.load(std::memory_order_relaxed))
			{
				TexMemManager::DestroyVkImage(w.image);
				w = _pending_windows.back();
				_pending_windows.pop_back();
				continue;
			}

			VkBuffer carveBuffer = VK_NULL_HANDLE;
			VkDeviceSize carveOffset = 0;
			std::uint32_t chunkEnd = 0;
			std::vector<VkBufferImageCopy> regions;
			bool ok = false;
			{
				std::shared_lock<std::shared_mutex> lock(_textures_mutex);
				const auto& td = _textures_data[w.slot];
				if (td.Data && td.Data.data && td.Data.offsets &&
					td.Data.mipLevels == w.total && td.Data.offsets->size() == w.total)
				{
					const auto& offsets = *td.Data.offsets;
					const auto dataSize = static_cast<VkDeviceSize>(td.Data.data->size());
					const VkDeviceSize stagingSize = TexMemManager::GetStagingBufferSize();

					// 本轮 chunk = [cursor, cursor+cnt), DDS 中的连续段
					auto chunkBytes = [&](std::uint32_t cnt) -> VkDeviceSize {
						const std::uint32_t end = w.cursor + cnt;
						return (end < w.total) ? (offsets[end] - offsets[w.cursor])
							: (dataSize - offsets[w.cursor]);
						};
					std::uint32_t cnt = 1;
					while (w.cursor + cnt < w.total && chunkBytes(cnt + 1) <= stagingSize)
						++cnt;

					VkBuffer buf = VK_NULL_HANDLE;
					VkDeviceSize off = 0;
					void* dst = nullptr;
					while (cnt > 0 && !TexMemManager::CreateStagingBuffer(buf, off, pool, chunkBytes(cnt), &dst))
						--cnt;

					if (cnt > 0)
					{
						std::memcpy(dst, td.Data.data->data() + offsets[w.cursor],
							static_cast<size_t>(chunkBytes(cnt)));

						regions.resize(cnt);
						for (std::uint32_t i = 0; i < cnt; ++i)
						{
							const std::uint32_t vm = w.cursor + i;	// 虚拟 mip 层号
							auto& r = regions[i];
							r.bufferOffset = off + static_cast<VkDeviceSize>(offsets[vm] - offsets[w.cursor]);
							r.bufferRowLength = 0;
							r.bufferImageHeight = 0;
							r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, vm - w.base, 0, 1 };
							r.imageOffset = { 0, 0, 0 };
							r.imageExtent = { std::max(1u, td.Data.width >> vm), std::max(1u, td.Data.height >> vm), 1 };
						}
						carveBuffer = buf;
						carveOffset = off;
						chunkEnd = w.cursor + cnt;
						ok = true;
					}
				}
			}

			if (!ok)
			{
				++wi;	// 本轮剩余空间装不下任何一层: 留给下一轮
				continue;
			}

			if (!w.started)
			{
				_transition_image_layout(cmd, w.image, w.format,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					0, VK_REMAINING_MIP_LEVELS);
				w.started = true;
			}

			vkCmdCopyBufferToImage(cmd, carveBuffer, w.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<std::uint32_t>(regions.size()), regions.data());
			anyRecorded = true;

			if (chunkEnd >= w.total)
			{
				_record_release_barrier(cmd, w.image);
				done.push_back({ w.slot, w.base, w.total, w.image });
				w = _pending_windows.back();
				_pending_windows.pop_back();
			}
			else
			{
				w.cursor = chunkEnd;
				++wi;
			}
		}

		auto device = IVulkanBase::Base().GetDevice();

		if (!anyRecorded)
		{
			vkFreeCommandBuffers(device, _transfer_command_pool, 1, &cmd);	// 未提交过, 安全
			if (!_pending_windows.empty())
				_abandon_pending_windows("本轮未装入任何数据(防御性放弃)");
			return;
		}

		// 提交前必须结束录制
		if (VkResult r = vkEndCommandBuffer(cmd))
		{
			INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] [Streaming] vkEndCommandBuffer 失败! VkResult: {}.", static_cast<std::int32_t>(r)));
			vkFreeCommandBuffers(device, _transfer_command_pool, 1, &cmd);	// 非 pending, 安全
			_abandon_pending_windows("vkEndCommandBuffer 失败");
			return;
		}

		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;

		vkResetFences(device, 1, &_upload_fences[pool]);
		if (vkQueueSubmit(_transfer_queue, 1, &si, _upload_fences[pool]) == VK_SUCCESS)
		{
			_upload_fence_used[pool] = true;
			_round_cmds[pool] = cmd;	// pending 中不能立即释放 —— 挂到槽位, fence 信号后由 _wait_round_fence 释放
			++_upload_round_index;
		}
		else
		{
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] [Streaming] 传输队列提交失败!");
			vkFreeCommandBuffers(device, _transfer_command_pool, 1, &cmd);	// 提交失败, 非 pending, 安全
			_abandon_pending_windows("传输队列提交失败");
			return;
		}

		// 完成项挂到本轮: fence 信号后才进入完成队列
		for (auto& d : done)
			_round_done[pool].push_back(d);
	}

	bool IVulkanTexture2DManagement::_wait_round_fence(std::uint32_t pool)
	{
		if (pool >= _upload_fences.size()) return false;
		if (!_upload_fence_used[pool]) return true;	// 该槽位没有在途传输

		auto device = IVulkanBase::Base().GetDevice();
		if (vkWaitForFences(device, 1, &_upload_fences[pool], VK_TRUE, UINT64_MAX))
			return false;

		// fence 已信号 = release barrier 已执行 -> 冲刷完成项安全
		{
			std::lock_guard<std::mutex> lock(_completed_mutex);
			for (auto& c : _round_done[pool])
				_completed_uploads.push_back(c);
		}
		_round_done[pool].clear();
		_upload_fence_used[pool] = false;

		// fence 已信号 -> 本轮命令缓冲不再 pending, 现在释放才合法
		if (pool < _round_cmds.size() && _round_cmds[pool] != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(device, _transfer_command_pool, 1, &_round_cmds[pool]);
			_round_cmds[pool] = VK_NULL_HANDLE;
		}
		return true;
	}

	void IVulkanTexture2DManagement::_drain_all_round_fences()
	{
		for (std::uint32_t p = 0; p < _upload_fences.size(); ++p)
		{
			if (!_wait_round_fence(p))
				INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] [Streaming] 排空传输轮次 {} 的 fence 失败!", p));
		}
	}

	void IVulkanTexture2DManagement::_abandon_pending_windows(const char* reason)
	{
		INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] [Streaming] 放弃 {} 个在途上传窗口: {}.",
			_pending_windows.size(), reason));
		// 先排空在途轮次: 已完成的窗口合法进入完成队列,
		// 之后仍在 pending 的 image 才能安全销毁(GPU 不再引用)
		_drain_all_round_fences();
		for (auto& w : _pending_windows)
		{
			if (w.slot < _stream_states_count)
			{
				// 只清"仍指向本窗口"的目标; 期间被更精细目标覆盖的保留
				std::uint32_t t = w.base;
				_stream_states[w.slot].TargetBase.compare_exchange_strong(t, UINT32_MAX, std::memory_order_acq_rel);
			}
			TexMemManager::DestroyVkImage(w.image);
		}
		_pending_windows.clear();
	}

	std::uint32_t IVulkanTexture2DManagement::_get_default_texture_index(TextureType type)
	{
		switch (type)
		{
		case TextureType::TYPE_Emission:		return DefaultTextureType::S_Black;
		case TextureType::TYPE_Normal:			return DefaultTextureType::NormalBlue;
		case TextureType::TYPE_Roughness:
		case TextureType::TYPE_Specular:		return DefaultTextureType::U8_128;
		case TextureType::TYPE_AO:
		case TextureType::TYPE_Opacity:			return DefaultTextureType::U_White;
		case TextureType::TYPE_Metallic:
		case TextureType::TYPE_ClearCoat:		return DefaultTextureType::U_Black;
		case TextureType::TYPE_ORM:				return DefaultTextureType::ORM;
		default:								return DefaultTextureType::S_White;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 命令缓冲录制辅助
	//////////////////////////////////////////////////////////////////////////////////////

	void IVulkanTexture2DManagement::RecordAcquireBarriers(VkCommandBuffer cmd)
	{
		if (!_is_valid || _pending_acquires.empty() || cmd == VK_NULL_HANDLE) return;
		const bool sameFamily = (_transfer_family_index == _graphics_family_index);

		std::vector<VkImageMemoryBarrier2> bars;
		bars.reserve(_pending_acquires.size());
		for (auto& c : _pending_acquires)
		{
			if (c.slot < _stream_states_count &&
				_stream_states[c.slot].Destroyed.load(std::memory_order_relaxed))
				continue;	// 已销毁, 不再 barrier (image 由帧环保管到销毁)

			VkImageMemoryBarrier2 b{};
			b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;	// acquire 语义
			b.srcAccessMask = 0;
			b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			b.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			b.srcQueueFamilyIndex = sameFamily ? VK_QUEUE_FAMILY_IGNORED : _transfer_family_index;
			b.dstQueueFamilyIndex = sameFamily ? VK_QUEUE_FAMILY_IGNORED : _graphics_family_index;
			b.image = c.image;
			b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
			bars.push_back(b);
		}

		if (!bars.empty())
		{
			VkDependencyInfo di{};
			di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			di.imageMemoryBarrierCount = static_cast<std::uint32_t>(bars.size());
			di.pImageMemoryBarriers = bars.data();
			vkCmdPipelineBarrier2(cmd, &di);
		}
		_pending_acquires.clear();
	}

	void IVulkanTexture2DManagement::ResetMipFeedback(VkCommandBuffer cmd, std::uint32_t frame_index)
	{
		if (!_is_valid || cmd == VK_NULL_HANDLE) return;
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;
		VkBuffer fb = _mip_feedback_buffers[frame_index];
		if (fb == VK_NULL_HANDLE) return;

		// 清为 0xFFFFFFFF = "未引用"
		vkCmdFillBuffer(cmd, fb, 0, VK_WHOLE_SIZE, 0xFFFFFFFFu);

		VkMemoryBarrier2 b{};
		b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.memoryBarrierCount = 1;
		di.pMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &di);
	}

	void IVulkanTexture2DManagement::RecordMipFeedbackCopy(VkCommandBuffer cmd, std::uint32_t frame_index)
	{
		if (!_is_valid || cmd == VK_NULL_HANDLE) return;
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;
		VkBuffer fb = _mip_feedback_buffers[frame_index];
		VkBuffer rb = _mip_readback_buffers[frame_index];
		if (fb == VK_NULL_HANDLE || rb == VK_NULL_HANDLE) return;

		VkMemoryBarrier2 b{};
		b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.memoryBarrierCount = 1;
		di.pMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &di);

		VkBufferCopy region{ 0, 0, static_cast<VkDeviceSize>(_mip_feedback_count) * sizeof(std::uint32_t) };
		vkCmdCopyBuffer(cmd, fb, rb, 1, &region);
	}

	void IVulkanTexture2DManagement::BindMipFeedback(VkDescriptorSet set0, std::uint32_t frame_index)
	{
		if (!_is_valid || set0 == VK_NULL_HANDLE) return;
		if (frame_index >= IVulkan::MAX_FRAMES_IN_FLIGHT) return;
		VkBuffer fb = _mip_feedback_buffers[frame_index];
		if (fb == VK_NULL_HANDLE) return;

		VkDescriptorBufferInfo bi{};
		bi.buffer = fb;
		bi.offset = 0;
		bi.range = VK_WHOLE_SIZE;
		VkWriteDescriptorSet w{};
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.dstSet = set0;
		w.dstBinding = 4;	// set0 binding 4 : 纹理 mip 反馈
		w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		w.descriptorCount = 1;
		w.pBufferInfo = &bi;
		vkUpdateDescriptorSets(IVulkanBase::Base().GetDevice(), 1, &w, 0, nullptr);
	}

	bool IVulkanTexture2DManagement::GetTextureStreamInfo(const Texture2DHandle& handle, TextureStreamInfo& out) const
	{
		out = TextureStreamInfo{};
		if (!handle.IsValid() || handle.slot >= _stream_states_count) return false;

		{
			std::shared_lock<std::shared_mutex> lock(_textures_mutex);
			const auto& td = _textures_data[handle.slot];
			out.Width = td.Data.width;
			out.Height = td.Data.height;
		}
		auto& s = _stream_states[handle.slot];
		out.TotalMips = s.TotalMips.load(std::memory_order_acquire);
		out.ResidentBase = s.ResidentBase.load(std::memory_order_relaxed);

		std::uint32_t md = std::max(out.Width, out.Height);
		while (md > 1) { ++out.Log2MaxDim; md >>= 1; }	// floor(log2(max(w,h)))
		return out.TotalMips != 0;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 上传请求 / 队列 / 线程池任务
	//////////////////////////////////////////////////////////////////////////////////////

	void IVulkanTexture2DManagement::_request_retarget(std::uint32_t slot, std::uint32_t target)
	{
		if (!_is_valid) return;
		if (slot >= _stream_states_count) return;
		auto& s = _stream_states[slot];
		if (s.Destroyed.load(std::memory_order_relaxed)) return;
		if (s.TotalMips.load(std::memory_order_acquire) == 0) return;

		// 只允许被"更精细 (数值更小)"的目标覆盖;
		// 在途的精细目标会压制降级请求 —— 防抖的一部分
		std::uint32_t cur = s.TargetBase.load(std::memory_order_acquire);
		while (true)
		{
			if (cur != UINT32_MAX && cur <= target) return;
			if (s.TargetBase.compare_exchange_weak(cur, target,
				std::memory_order_acq_rel, std::memory_order_acquire))
				break;
		}

		{
			std::lock_guard<std::mutex> lock(_upload_mutex);
			if (_upload_queued_set.insert(slot).second)
				_upload_queue.push_back(slot);
		}
		_kick_upload_task();
	}

	void IVulkanTexture2DManagement::_kick_upload_task()
	{
		if (_upload_pool == nullptr || _transfer_command_pool == VK_NULL_HANDLE || _upload_fences.empty()) return;

		{
			// 标志的设置与队列判空都在同一把锁内, 保证无漏唤醒
			std::lock_guard<std::mutex> lock(_upload_mutex);
			if (_upload_task_running || _upload_stop.load(std::memory_order_relaxed)) return;
			_upload_task_running = true;
		}
		// 上传任务: 取空队列 -> 整批处理 -> 退出 (非常驻, 不阻塞等待)
		// 优先级 0: 流式上传对延迟敏感
		_upload_pool->Submit(0, [this]() { _upload_task_func(); });
	}

	void IVulkanTexture2DManagement::_upload_task_func()
	{
		for (;;)
		{
			try
			{
				while (!_upload_stop.load(std::memory_order_relaxed))
				{
					std::vector<std::uint32_t> newSlots;
					{
						std::lock_guard<std::mutex> lock(_upload_mutex);
						if (!_upload_queue.empty())
						{
							newSlots.assign(_upload_queue.begin(), _upload_queue.end());
							_upload_queue.clear();
							_upload_queued_set.clear();
						}
					}
					if (!newSlots.empty())
						_begin_pending_windows(newSlots);
					if (!_pending_windows.empty())
						_upload_round();			// 一轮 = 装满至多一块 staging 的传输
					else if (newSlots.empty())
						break;						// 队列与窗口都空
				}
			}
			catch (const std::exception& e)
			{
				INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] [Streaming] 上传任务异常: {}", e.what()));
				_abandon_pending_windows("任务异常");
			}
			catch (...)
			{
				INVENT_LOG_ERROR("[IVulkanTexture2DManagement] [Streaming] 上传任务未知异常.");
				_abandon_pending_windows("任务未知异常");
			}
			// ---------- 排空在途轮次 (等待所有 fence + 冲刷完成项) ----------
			_drain_all_round_fences();
			// 锁内决定是否真正退出:
			// "队列空 => 清 running" 与 "_kick 的检查" 在同一把锁下原子进行,
			// 保证退出瞬间到达的新请求不可能被漏掉(要么被本任务看到, 要么 kick 到新任务)

			{
				std::lock_guard<std::mutex> lock(_upload_mutex);
				if (_upload_queue.empty() || _upload_stop.load(std::memory_order_relaxed))
				{
					_upload_task_running = false;
					_upload_idle_cv.notify_all();
					return;
				}
			}
			// 排空期间又来了新请求: 回去继续干
		}
		
	}

	void IVulkanTexture2DManagement::_stop_upload()
	{
		_upload_stop.store(true, std::memory_order_relaxed);

		if (_upload_pool && _upload_pool->IsRunning())
		{
			// 等正在跑的任务自然退出 (它会在循环顶看到 stop)
			std::unique_lock<std::mutex> lock(_upload_mutex);
			_upload_idle_cv.wait(lock, [this] { return !_upload_task_running; });
		}

		std::lock_guard<std::mutex> lock(_upload_mutex);
		_upload_task_running = false;
		_upload_queue.clear();
		_upload_queued_set.clear();
		_upload_stop.store(false, std::memory_order_relaxed);
	}

	void IVulkanTexture2DManagement::WaitForUploadIdle()
	{
		if (_upload_pool == nullptr || !_upload_pool->IsRunning())
		{
			// 线程池已停/未启动: 任务不会再执行, 直接复位
			std::lock_guard<std::mutex> lock(_upload_mutex);
			_upload_task_running = false;
			_upload_queue.clear();
			_upload_queued_set.clear();
			return;
		}
		std::unique_lock<std::mutex> lock(_upload_mutex);
		_upload_idle_cv.wait(lock, [this] { return !_upload_task_running; });
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 批处理: 窗口 image 创建 + 尾巴上传 (传输队列)
	//////////////////////////////////////////////////////////////////////////////////////

	VkCommandBuffer IVulkanTexture2DManagement::_begin_transfer_command()
	{
		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandPool = _transfer_command_pool;
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

	void IVulkanTexture2DManagement::_record_release_barrier(VkCommandBuffer cmd, VkImage image)
	{
		// 同一队列族时 QFI 填 IGNORED, barrier 自动退化为普通屏障
		const bool sameFamily = (_transfer_family_index == _graphics_family_index);

		VkImageMemoryBarrier2 b{};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_NONE;	// release 语义
		b.dstAccessMask = 0;
		b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b.srcQueueFamilyIndex = sameFamily ? VK_QUEUE_FAMILY_IGNORED : _transfer_family_index;
		b.dstQueueFamilyIndex = sameFamily ? VK_QUEUE_FAMILY_IGNORED : _graphics_family_index;
		b.image = image;
		b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };

		VkDependencyInfo di{};
		di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		di.imageMemoryBarrierCount = 1;
		di.pImageMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &di);
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 流式资源初始化/销毁
	//////////////////////////////////////////////////////////////////////////////////////

	void IVulkanTexture2DManagement::_init_transfer_resources()
	{
		auto& base = IVulkanBase::Base();
		_graphics_family_index = base.GetQueueFamilyIndices().GraphicsFamily;
		_transfer_family_index = base.GetQueueFamilyIndices().TransferFamily;

		if (_transfer_queue == VK_NULL_HANDLE)
		{
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] 无专用传输队列, 纹理流式加载被禁用!");
			return;
		}

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		poolInfo.queueFamilyIndex = _transfer_family_index;
		if (vkCreateCommandPool(base.GetDevice(), &poolInfo, nullptr, &_transfer_command_pool))
		{
			_transfer_command_pool = VK_NULL_HANDLE;
			INVENT_LOG_ERROR("[IVulkanTexture2DManagement] failed to create transfer command pool!");
			return;
		}

		const std::uint32_t poolCount = TexMemManager::GetStagingPoolCount();
		_upload_fences.assign(poolCount, VK_NULL_HANDLE);
		_upload_fence_used.assign(poolCount, false);
		_round_done.resize(poolCount);
		_round_cmds.assign(poolCount, VK_NULL_HANDLE);		// ★ 新增

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;		// 初始已信号(未用过的槽位无需等待)
		for (std::uint32_t i = 0; i < poolCount; ++i)
		{
			if (vkCreateFence(base.GetDevice(), &fenceInfo, nullptr, &_upload_fences[i]))
				INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] failed to create upload fence {}!", i));
		}
	}

	void IVulkanTexture2DManagement::_destroy_transfer_resources()
	{
		auto device = IVulkanBase::Base().GetDevice();
		for (auto& f : _upload_fences)
			if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
		_upload_fences.clear();
		_upload_fence_used.clear();
		_round_cmds.clear();			// ★ Clear 前 _stop_upload 已排空全部 fence, 此时必为空
		for (auto& rd : _round_done) rd.clear();
		_round_done.clear();
		if (_transfer_command_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(device, _transfer_command_pool, nullptr);
			_transfer_command_pool = VK_NULL_HANDLE;
		}
	}

	void IVulkanTexture2DManagement::_init_mip_feedback(std::uint32_t count)
	{
		_mip_feedback_count = count;
		VkDeviceSize size = static_cast<VkDeviceSize>(count) * sizeof(std::uint32_t);
		if (size == 0) return;

		auto& base = IVulkanBase::Base();
		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			// device local: CS atomicMin 写 + fill 清零 + 拷出
			if (VkResult r = base.UseVmaCreateBuffer(size,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				0,	// 纯 GPU 存取
				_mip_feedback_buffers[i]))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] 创建 mip 反馈缓冲失败! VkResult: {}.", static_cast<std::int32_t>(r)));
				_mip_feedback_buffers[i] = VK_NULL_HANDLE;
				continue;
			}
			// host visible 读回: 帧末拷贝, 主线程下下帧读取
			if (VkResult r = base.UseVmaCreateBuffer(size,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				_mip_readback_buffers[i], &_mip_readback_mapped[i]))
			{
				INVENT_LOG_ERROR(std::format("[IVulkanTexture2DManagement] 创建 mip 读回缓冲失败! VkResult: {}.", static_cast<std::int32_t>(r)));
				_mip_readback_buffers[i] = VK_NULL_HANDLE;
				_mip_readback_mapped[i] = nullptr;
				continue;
			}
			std::memset(_mip_readback_mapped[i], 0xFF, static_cast<size_t>(size));	// 初始 = 未引用
		}
	}

	void IVulkanTexture2DManagement::_resize_mip_feedback(std::uint32_t count)
	{
		// in-flight 帧的 set0 仍引用旧缓冲 -> 延迟销毁;
		// set0 每帧重新绑定 (BindMipFeedback), 新帧自动使用新缓冲
		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (_mip_feedback_buffers[i] != VK_NULL_HANDLE)
			{
				_retired_buffers.emplace_back(_frame_counter, _mip_feedback_buffers[i]);
				_mip_feedback_buffers[i] = VK_NULL_HANDLE;
			}
			if (_mip_readback_buffers[i] != VK_NULL_HANDLE)
			{
				_retired_buffers.emplace_back(_frame_counter, _mip_readback_buffers[i]);
				_mip_readback_buffers[i] = VK_NULL_HANDLE;
				_mip_readback_mapped[i] = nullptr;
			}
		}
		_init_mip_feedback(count);
	}

	void IVulkanTexture2DManagement::_destroy_mip_feedback()
	{
		for (std::uint32_t i = 0; i < IVulkan::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (_mip_feedback_buffers[i] != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyBuffer(_mip_feedback_buffers[i]);
				_mip_feedback_buffers[i] = VK_NULL_HANDLE;
			}
			if (_mip_readback_buffers[i] != VK_NULL_HANDLE)
			{
				IVulkanBase::Base().UseVmaDestroyBuffer(_mip_readback_buffers[i]);
				_mip_readback_buffers[i] = VK_NULL_HANDLE;
				_mip_readback_mapped[i] = nullptr;
			}
		}
		for (auto& [f, b] : _retired_buffers)
			IVulkanBase::Base().UseVmaDestroyBuffer(b);
		_retired_buffers.clear();
		_mip_feedback_count = 0;
	}

	void IVulkanTexture2DManagement::_resize_stream_states(size_t count)
	{
		// 含 atomic 的结构不可放入会搬移的容器, 用数组 + 手动迁移
		auto next = std::make_unique<TextureStreamState[]>(count);
		const size_t n = std::min(count, _stream_states_count);
		for (size_t i = 0; i < n; ++i)
		{
			auto& d = next[i];
			auto& s = _stream_states[i];
			d.TotalMips.store(s.TotalMips.load(std::memory_order_relaxed));
			d.ResidentBase.store(s.ResidentBase.load(std::memory_order_relaxed));
			d.TargetBase.store(s.TargetBase.load(std::memory_order_relaxed));
			d.DesiredMip.store(s.DesiredMip.load(std::memory_order_relaxed));
			d.Generation.store(s.Generation.load(std::memory_order_relaxed));
			d.Pinned.store(s.Pinned.load(std::memory_order_relaxed));
			d.Destroyed.store(s.Destroyed.load(std::memory_order_relaxed));
		}
		_stream_states = std::move(next);
		_stream_states_count = count;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	////////////// 原有功能
	//////////////////////////////////////////////////////////////////////////////////////

	void IVulkanTexture2DManagement::_init_default_image()
	{
		_default_textures.resize(DefaultTextureType::DefaultCount);

		uint32_t whitePixel = 0xFFFFFFFF;
		uint32_t blackPixel = 0xFF000000;
		uint32_t normalPixel = 0xFFFF8080;
		uint32_t ormPixel = 0xFF0080FF;
		uint8_t r128Pixel = 0x80;

		constexpr VkDeviceSize stagingBufferSize = sizeof(uint32_t) * 4 + sizeof(uint8_t) * 3;

		VkBuffer stagingBuffer;
		void* data;
		if (VkResult res = IVulkanBase::Base().UseVmaCreateBuffer(stagingBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			stagingBuffer,
			&data))
		{
			throw std::runtime_error("failed to load staging buffer! : _init_default_image");
		}

		uint32_t pixels[] = { whitePixel,blackPixel,normalPixel,ormPixel };
		uint8_t pixels2[] = { r128Pixel };
		memcpy(data, pixels, sizeof(uint32_t) * 4);
		memcpy(reinterpret_cast<std::byte*>(data) + (sizeof(uint32_t) * 4), pixels2, sizeof(uint8_t));

		if (!IVulkanBase::Base().UseVmaFlushAllocationBuffer(stagingBuffer))
		{
			throw std::runtime_error("failed to flush buffer allocation!");
		}

		VkDeviceSize offset{ 0 };
		// white
		VkImage whiteImageSRGB;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			whiteImageSRGB);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			whiteImageSRGB,
			VK_FORMAT_R8G8B8A8_SRGB,
			1,
			1,
			1,
			offset);
		VkImageView whiteImageViewSRGB = IVulkanBase::Base().CreateImageView(whiteImageSRGB,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (whiteImageViewSRGB == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::S_White] = { whiteImageSRGB,whiteImageViewSRGB,0 };
		VkImage whiteImageUNORM;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			whiteImageUNORM);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			whiteImageUNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			1,
			1,
			1,
			offset);
		VkImageView whiteImageViewUNORM = IVulkanBase::Base().CreateImageView(whiteImageUNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (whiteImageViewUNORM == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::U_White] = { whiteImageUNORM,whiteImageViewUNORM,0 };
		offset += sizeof(uint32_t);

		// black
		VkImage blackImageSRGB;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			blackImageSRGB);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			blackImageSRGB,
			VK_FORMAT_R8G8B8A8_SRGB,
			1,
			1,
			1,
			offset);
		VkImageView blackImageViewSRGB = IVulkanBase::Base().CreateImageView(blackImageSRGB,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (blackImageViewSRGB == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::S_Black] = { blackImageSRGB,blackImageViewSRGB,0 };
		VkImage blackImageUNORM;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			blackImageUNORM);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			blackImageUNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			1,
			1,
			1,
			offset);
		VkImageView blackImageViewUNORM = IVulkanBase::Base().CreateImageView(blackImageUNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (blackImageViewUNORM == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::U_Black] = { blackImageUNORM,blackImageViewUNORM,0 };
		offset += sizeof(uint32_t);

		// normal
		VkImage normalImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			normalImage);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			normalImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			1,
			1,
			1,
			offset);
		VkImageView normalImageView = IVulkanBase::Base().CreateImageView(normalImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (normalImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::NormalBlue] = { normalImage,normalImageView,0 };
		offset += sizeof(uint32_t);

		// ORM
		VkImage ORMImage;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			ORMImage);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			ORMImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			1,
			1,
			1,
			offset);
		VkImageView ORMImageView = IVulkanBase::Base().CreateImageView(ORMImage,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (ORMImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::ORM] = { ORMImage,ORMImageView,0 };
		offset += sizeof(uint32_t);

		// u8
		VkImage R128Image;
		IVulkanBase::Base().UseVmaCreateImage(1,
			1,
			1,
			VK_FORMAT_R8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			R128Image);
		_upload_texture_and_generate_mipmaps(stagingBuffer,
			R128Image,
			VK_FORMAT_R8_UNORM,
			1,
			1,
			1,
			offset);
		VkImageView R128ImageView = IVulkanBase::Base().CreateImageView(R128Image,
			VK_FORMAT_R8_UNORM,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_VIEW_TYPE_2D,
			1);
		if (R128ImageView == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to create texture image view! white pixel");
		}
		_default_textures[DefaultTextureType::U8_128] = { R128Image,R128ImageView,0 };

		IVulkanBase::Base().UseVmaDestroyBuffer(stagingBuffer);
	}

	void IVulkanTexture2DManagement::_init_other()
	{
		_texture_name_cache = new TextureNameMap(64,
			std::hash<std::string>(),
			std::equal_to<std::string>(),
			IMemPoolAllocatorOnlyFixedBlock< std::pair<const std::string, Texture2DHandle>>(IEngineTools::Instance().GetMemPoolPool()));
		_texture_handle_name_cache = new TextureHandleNameMap(64,
			std::hash<std::uint32_t>(),
			std::equal_to<std::uint32_t>(),
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::uint32_t, std::string>>(IEngineTools::Instance().GetMemPoolPool()));
	}

	void IVulkanTexture2DManagement::_insert_name_cache(const std::string& name, Texture2DHandle handle)
	{
		std::unique_lock<std::shared_mutex> lock(_cache_mutex);
		_texture_name_cache->insert({ name, handle });
		_texture_handle_name_cache->insert({ handle.slot, name });
	}

	void IVulkanTexture2DManagement::_remove_name_cache_by_handle(const Texture2DHandle& handle)
	{
		if (!handle.IsValid())
			return;
		std::unique_lock<std::shared_mutex> lock(_cache_mutex);
		auto it = _texture_handle_name_cache->find(handle.slot);
		if (it != _texture_handle_name_cache->end())
		{
			_texture_name_cache->erase(it->second);   // 删除正向表项
			_texture_handle_name_cache->erase(it);    // 删除反向表项
		}
	}

	void IVulkanTexture2DManagement::_update_texture_count()
	{
		auto textureCount = static_cast<size_t>(IVulkanBase::Base().GetCurrentBindlessDescriptorCount());
		INVENT_LOG_INFO(std::format("[IVulkanTexture2DManagement] updated! current bindless descriptor count: {}.", textureCount));

		// 上传任务正在访问 _textures_data / _stream_states, 必须等它停下才能 resize
		WaitForUploadIdle();

		{
			std::unique_lock<std::shared_mutex> lock(_textures_mutex);	// 与 DDS 工作线程互斥
			_bit_vector_used.ResizeBitCount(textureCount);
			_bit_vector_valid.ResizeBitCount(textureCount);
			_textures.resize(textureCount, IVulkanTexture2DHandle());
			_textures_data.resize(textureCount);
			_resize_stream_states(textureCount);
			_unreferenced_frames.resize(textureCount, 0);
		}

		_resize_mip_feedback(static_cast<std::uint32_t>(textureCount));
	}

	void IVulkanTexture2DManagement::_transition_image_layout(VkCommandBuffer commond_buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t base_mip_level, uint32_t level_count)
	{
		VkAccessFlags srcAccess = 0;
		VkAccessFlags dstAccess = 0;
		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcAccess = 0;
			dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
			dstAccess = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstAccess = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			srcAccess = VK_ACCESS_SHADER_READ_BIT;
			dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}

		VkImageMemoryBarrier2 barrier2{};
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier2.oldLayout = old_layout;
		barrier2.newLayout = new_layout;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.image = image;
		barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, base_mip_level, level_count, 0, 1 };
		barrier2.srcStageMask = srcStage;
		barrier2.srcAccessMask = srcAccess;
		barrier2.dstStageMask = dstStage;
		barrier2.dstAccessMask = dstAccess;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &barrier2;

		vkCmdPipelineBarrier2(commond_buffer, &depInfo);
	}

	void IVulkanTexture2DManagement::_upload_texture_and_generate_mipmaps(VkBuffer staging_buffer,
		VkImage tex_image,
		VkFormat trans_format,
		uint32_t width,
		uint32_t height,
		uint32_t level_count,
		VkDeviceSize buffer_offset,
		VkImageLayout initial_layout)
	{
		auto cmd = IVulkanBase::Base().BeginSingleTimeCommands();

		_transition_image_layout(cmd,
			tex_image,
			trans_format,
			initial_layout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			level_count);

		VkBufferImageCopy region{};
		region.bufferOffset = buffer_offset;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;   // 目標是 Level 0
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd, staging_buffer, tex_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for (uint32_t i = 1; i < level_count; ++i)
		{
			_transition_image_layout(cmd,
				tex_image,
				trans_format,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				i - 1,
				1);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd,
				tex_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				tex_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit, VK_FILTER_LINEAR);

			_transition_image_layout(cmd,
				tex_image,
				trans_format,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				i - 1,
				1);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		} // for end

		_transition_image_layout(cmd,
			tex_image,
			trans_format,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			level_count - 1,
			1);
		IVulkanBase::Base().EndSingleTimeCommands(cmd);
	}

	bool IVulkanTexture2DManagement::_load_dds_to_compressed_data(const std::string& filepath,
		LoadDDSType type,
		ITextureCompresser::CompressedTextureData& out,
		VkFormat& format)
	{
		dds::Image image;
		auto result = dds::readFile(filepath, &image);
		if (result != dds::ReadResult::Success)
		{
			return false;
		}

		if (!out) return false;
		switch (type)
		{
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::Auto:
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC1:
			if (image.format != DXGI_FORMAT_BC1_UNORM_SRGB && image.format != DXGI_FORMAT_BC1_UNORM) return false;
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC3:
			if (image.format != DXGI_FORMAT_BC3_UNORM_SRGB && image.format != DXGI_FORMAT_BC3_UNORM) return false;
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC4:
			if (image.format != DXGI_FORMAT_BC4_SNORM && image.format != DXGI_FORMAT_BC4_UNORM) return false;
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC5:
			if (image.format != DXGI_FORMAT_BC5_SNORM && image.format != DXGI_FORMAT_BC5_UNORM) return false;
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC6H:
			if (image.format != DXGI_FORMAT_BC6H_UF16 && image.format != DXGI_FORMAT_BC6H_SF16) return false;
			break;
		case INVENT::IVulkanTexture2DManagement::LoadDDSType::BC7:
			if (image.format != DXGI_FORMAT_BC7_UNORM_SRGB && image.format != DXGI_FORMAT_BC7_UNORM) return false;
			break;
		default:
			return false;
		}

		if (image.arraySize != 1) return false;

		auto getTypeFromformat = [](DXGI_FORMAT format)-> VkFormat {
			switch (format)
			{
			case DXGI_FORMAT_BC1_UNORM:						return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			case DXGI_FORMAT_BC1_UNORM_SRGB:				return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
			case DXGI_FORMAT_BC3_UNORM:						return VK_FORMAT_BC3_UNORM_BLOCK;
			case DXGI_FORMAT_BC3_UNORM_SRGB:				return VK_FORMAT_BC3_SRGB_BLOCK;
			case DXGI_FORMAT_BC4_UNORM:						return VK_FORMAT_BC4_UNORM_BLOCK;
			case DXGI_FORMAT_BC4_SNORM:						return VK_FORMAT_BC4_SNORM_BLOCK;
			case DXGI_FORMAT_BC5_UNORM:						return VK_FORMAT_BC5_UNORM_BLOCK;
			case DXGI_FORMAT_BC5_SNORM:						return VK_FORMAT_BC5_SNORM_BLOCK;
			case DXGI_FORMAT_BC6H_UF16:						return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case DXGI_FORMAT_BC6H_SF16:						return VK_FORMAT_BC6H_SFLOAT_BLOCK;
			case DXGI_FORMAT_BC7_UNORM:						return VK_FORMAT_BC7_UNORM_BLOCK;
			case DXGI_FORMAT_BC7_UNORM_SRGB:				return VK_FORMAT_BC7_SRGB_BLOCK;
			default:
				INVENT_LOG_WARNING("[IVulkanTexture2DManagement] This dds file type is not can analysis.");
				return VK_FORMAT_UNDEFINED;
				break;
			}
			};

		format = getTypeFromformat(image.format);
		if (format == VK_FORMAT_UNDEFINED) return false;

		out.width = image.width;
		out.height = image.height;
		out.mipLevels = image.numMips;
		out.offsets->resize(image.mipmaps.size());
		size_t dataSize = 0;
		for (size_t i = 0; i < image.mipmaps.size(); ++i)
		{
			(*out.offsets)[i] = static_cast<uint32_t>(dataSize);
			dataSize += image.mipmaps[i].size();
		}
		out.data->resize(dataSize);
		memcpy(out.data->data(), image.data.get(), dataSize);

		return true;
	}

	IVulkanTexture2DManagement::Texture2DHandle IVulkanTexture2DManagement::_find_handle_from_cache(const std::string& name) const
	{
		auto iter = _texture_name_cache->find(name);
		if (iter != _texture_name_cache->end())
		{
			return iter->second;
		}
		return Texture2DHandle{};
	}

}
