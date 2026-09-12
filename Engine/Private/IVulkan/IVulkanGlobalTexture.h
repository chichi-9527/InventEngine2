#pragma once

#include "IBitArray.h"
#include "ITextureCompresser.h"
#include "IVulkan/ITieredImageMemoryManager.h"
#include "IVulkan/VulkanConfig.h"

#include <cstdint>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <cmath>
#include <array>
#include <deque>
#include <vector>
#include <functional>
#include <condition_variable>
#include <unordered_set>
#include <utility>
#include <shared_mutex>

#include <vulkan/vulkan.h>

namespace INVENT
{
	template<typename T>
	class IMemPoolAllocatorOnlyFixedBlock;
	class IThreadPool;

	/*
	* 纹理流式加载 (Texture Streaming) :
	*
	*   1. DDS 数据在 CPU 侧常驻 (内存池), VkImage 只是"驻留窗口"
	*      —— 只按 [base, total) 这段 mip 链分配显存, 节省显存
	*      例如 2048x2048 (12 mip) 只驻留 mip2~11 时, VkImage 实际是 512x512 / 10 层
	*
	*   2. 每帧视锥裁剪 CS 通过 InterlockedMin 写 g_textureDesiredMip[slot] (set0 binding4)
	*      帧末拷贝到 readback 缓冲, CPU 下下帧读取 -> 生成升级/降级请求
	*
	*   3. 请求进入队列后由工作线程池异步处理:
	*      创建窗口 image -> staging 拷贝 -> 专用传输队列提交 + release barrier -> 完成队列
	*
	*   4. 主线程下一帧 ProcessCompletedUploads:
	*      acquire barrier + 新 ImageView + bindless 描述符更新 + 回调材质(写入 baseMip 到 BDA)
	*      旧 image/view 走帧环延迟销毁 (in-flight 帧可能还在采样)
	*
	*   5. 采样: 隐式导数采样无需任何改动(窗口尺寸自动适配 LOD);
	*      显式 LOD 采样需要材质 BDA 里的 baseMip 换算: SampleLevel(tex, uv, max(lod - baseMip, 0))
	*
	* ===== 每帧调用契约 =====
	*   [主线程, vkWaitForFences 之后]
	*       ProcessMipFeedback(frame_index);     // 读反馈 -> 升/降级/驱逐决策
	*       ProcessCompletedUploads();           // 装配新窗口 + 材质回调
	*       FlushDestroyQueue();                 // 帧环延迟销毁 (每帧恰好一次!)
	*   [分配 set0 后]
	*       BindMipFeedback(set0, frame_index);  // set0 binding4
	*   [录制命令缓冲]
	*       RecordAcquireBarriers(cmd);          // 必须在所有使用新纹理的命令之前
	*       ResetMipFeedback(cmd, frame_index);  // fill 0xFF
	*       ... 视锥裁剪 + mip 计算 CS dispatch ...
	*       RecordMipFeedbackCopy(cmd, frame_index);
	*       ... GPU 驱动间接绘制 ...
	*/
	class IVulkanTexture2DManagement
	{
	public:
		struct PendingDestroy
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView ImageView = VK_NULL_HANDLE;
		};

		// .hdr stb image stbi_loadf
		/*
		* SRGB:  擴散貼圖 (Diffuse Map) 基礎顏色貼圖 (Albedo Map) 反射貼圖 (Specular Map)
		* UNORM: 法線貼圖 (Normal Map) 粗糙度貼圖 (Roughness Map) 金屬度貼圖 (Metalic Map) 遮蔽貼圖 (AO Map)
		*/
		enum class TextureType : uint32_t
		{
			TYPE_Undefined = 0,
			TYPE_Deffuse,			// diffuse
			TYPE_Emission,			// emission
			TYPE_Normal,			// normal BC5
			TYPE_Roughness,			// roughness
			TYPE_AO,				// ao
			TYPE_Opacity,			// opacity
			TYPE_Metallic,			// metallic
			TYPE_ORM,				// R=AO, G=Roughness, B=Metallic
			TYPE_Specular,			// specular
			TYPE_ClearCoat			// clear coat
		};

		enum DefaultTextureType : uint32_t
		{
			S_White = 0,		// R, G, B, A (Identity)
			U_White,			// R, G, B, A (Identity)
			S_Black,			// R, G, B, A (Identity)
			U_Black,			// R, G, B, A (Identity)
			NormalBlue,			// R, G, ONE, ONE (扩展)
			ORM,				// R, G, B, A (Identity)
			U8_128,				// R, R, R, ONE (扩展)
			DefaultCount
		};

		enum class CompressionType : uint32_t {
			BC1 = 2,
			BC3 = 4,
			BC4 = 5,
			BC5 = 6,
			BC6H = 7,
			BC7_RGB = 8,
			BC7_RGBA = 9
		};

		struct Texture2DHandle
		{
			std::uint32_t slot{ UINT32_MAX };

			bool IsValid() const noexcept { return slot != UINT32_MAX; }
		};

		struct IVulkanTexture2DHandle
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView ImageView = VK_NULL_HANDLE;
			// 流式语义: 当前窗口 image 的 mip0 对应的"虚拟 mip 层号"(即 ResidentBase)
			// 数值越小越精细; UINT32_MAX = 未驻留
			std::uint32_t CurrentMaxMipLevel{ UINT32_MAX };

			bool IsValid() const noexcept
			{
				return CurrentMaxMipLevel != UINT32_MAX;
			}
		};

		// ===== 流式信息查询 =====
		struct TextureStreamInfo
		{
			std::uint32_t Width{ 0 };
			std::uint32_t Height{ 0 };
			std::uint32_t TotalMips{ 0 };						// 虚拟总层数 (0 = DDS 未就绪)
			std::uint32_t Log2MaxDim{ 0 };						// floor(log2(max(w,h))), 供裁剪 CS 计算期望 mip
			std::uint32_t ResidentBase{ UINT32_MAX };			// 当前窗口基点; UINT32_MAX = 未驻留
		};

		// 材质驻留变化通知 (主线程调用):
		//   new_base_mip    : 窗口基点(虚拟 mip 层号); UINT32_MAX = 已驱逐, 回到默认贴图
		//   image_mip_count : 窗口 image 自身层数 (显式 LOD 上限)
		//   total_mips      : 虚拟总层数
		// 材质在回调中把 base_mip 写入自身 BDA 数据并标脏
		using TextureResidentChangedCallback = std::function<void(Texture2DHandle,
			std::uint32_t /*new_base_mip*/, std::uint32_t /*image_mip_count*/, std::uint32_t /*total_mips*/)>;

	private:

		enum class LoadDDSType : uint32_t
		{
			Auto,
			BC1,
			BC3,
			BC4,
			BC5,
			BC6H,
			BC7
		};

		using TextureNameMap = std::unordered_map < std::string,
			Texture2DHandle,
			std::hash<std::string>,
			std::equal_to<std::string>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, Texture2DHandle>>>;
		using TextureHandleNameMap = std::unordered_map<
			std::uint32_t,
			std::string,
			std::hash<std::uint32_t>,
			std::equal_to<std::uint32_t>,
			IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::uint32_t, std::string>>>;

		// ===== 内部数据结构 (先声明, 成员引用) =====

		struct TextureData
		{
			ITextureCompresser::CompressedTextureData Data;
			VkFormat Format = VK_FORMAT_UNDEFINED;
			TextureType Type = TextureType::TYPE_Undefined;	// 驱逐时重绑默认贴图用
		};

		// 每槽流式状态 (多线程原子访问); 约定: mip 数值越小 = 越精细
		struct TextureStreamState
		{
			std::atomic<std::uint32_t> TotalMips{ 0 };				// 0 = DDS 未就绪/槽位空
			std::atomic<std::uint32_t> ResidentBase{ UINT32_MAX };	// 已驻留窗口基点
			std::atomic<std::uint32_t> TargetBase{ UINT32_MAX };	// 排队/上传中的目标
			std::atomic<std::uint32_t> DesiredMip{ UINT32_MAX };	// 最新 GPU 反馈
			std::atomic<std::uint32_t> Generation{ 0 };				// 销毁/复用代数 (防止 DDS 工作线程在销毁后写入)
			std::atomic<bool> Pinned{ false };						// 固定不驱逐
			std::atomic<bool> Destroyed{ false };
		};

		// 上传线程 -> 主线程 的完成项
		struct CompletedUpload
		{
			std::uint32_t slot{ UINT32_MAX };
			std::uint32_t new_base{ 0 };
			std::uint32_t total_mips{ 0 };
			VkImage image{ VK_NULL_HANDLE };
		};

		// 上传专用 staging (上传任务私有; 批次开头才可能重建, 上一批 fence 已等待)
		struct UploadStaging
		{
			VkBuffer Buffer = VK_NULL_HANDLE;
			void* Mapped = nullptr;
			VkDeviceSize Size = 0;
		};

		IVulkanTexture2DManagement() = default;
	public:
		~IVulkanTexture2DManagement() = default;

		// STATIC

		static std::uint32_t GetDXGIFormatFromTypes(TextureType tex_type, CompressionType& com_type);

		///////////////////

		static IVulkanTexture2DManagement& Instance();
		bool Init();
		// 清除所有标识符，释放所有 VkImage/VkImageView (含流式资源)
		void Clear();
		// 释放 CPU 内存，必须先调用 Clear 否则会造成 GPU 内存泄露
		// 在调用 IEngine::Shutdown() 之前调用
		void Terminate();

		Texture2DHandle AllocateTextureHandle();
		/// <param name="name"> 唯一纹理名,每次添加纹理会判断是否存在此纹理 </param>
		/// <param name="path"> dds 文件路径,只会加载2d纹理的 BC1/3/4/5/6H/7 压缩方式的纹理,否则加载失败,但会返回有效句柄 </param>
		/// <returns> 失败时会返回无效的句柄 </returns>
		Texture2DHandle AddTexture2D(const std::string& name, const std::string& path, TextureType texture_type);

		/// <summary>
		/// 请求纹理驻留到指定的最精细虚拟 mip 层 (0 = 最高清)
		/// 升级: 异步上传更精细的窗口; 明显变粗: 异步换更小的窗口; 都在后台完成
		/// </summary>
		/// <param name="needMipLevel"> 期望最精细虚拟 mip 层号 </param>
		void UpdateTexture2D(Texture2DHandle& handle, std::uint32_t needMipLevel);

		void DestroyTexture2D(Texture2DHandle handle);

		bool IsValid() const { return _is_valid; }

		void QueueDestroy(VkImage image, VkImageView image_view);
		// 每帧恰好调用一次! (帧环游标依赖帧号推进)
		void FlushDestroyQueue();

		///////////////////
		// ===== 纹理流式加载 =====

		void SetTextureResidentChangedCallback(TextureResidentChangedCallback callback)
		{
			_resident_changed_callback = std::move(callback);
		}

		/// <summary> 固定纹理不被驱逐 (天空盒/UI 等不参与实例裁剪 CS 的纹理必须 pin) </summary>
		void SetTexturePinned(const Texture2DHandle& handle, bool pinned)
		{
			if (handle.IsValid() && handle.slot < _stream_states_count)
				_stream_states[handle.slot].Pinned.store(pinned, std::memory_order_relaxed);
		}

		/// <returns> 当前窗口基点(虚拟 mip 层号); UINT32_MAX = 未驻留 </returns>
		std::uint32_t GetResidentMipLevel(const Texture2DHandle& handle) const
		{
			if (!handle.IsValid() || handle.slot >= _stream_states_count) return UINT32_MAX;
			return _stream_states[handle.slot].ResidentBase.load(std::memory_order_relaxed);
		}

		bool IsUploadInFlight(const Texture2DHandle& handle) const
		{
			if (!handle.IsValid() || handle.slot >= _stream_states_count) return false;
			return _stream_states[handle.slot].TargetBase.load(std::memory_order_relaxed) != UINT32_MAX;
		}

		bool GetTextureStreamInfo(const Texture2DHandle& handle, TextureStreamInfo& out) const;

		// 等待上传队列完全清空 (场景切换/描述符扩容前调用)
		void WaitForUploadIdle();

		// ---- 每帧主线程 (vkWaitForFences 之后) ----
		void ProcessMipFeedback(std::uint32_t frame_index);
		void ProcessCompletedUploads();

		// ---- 命令缓冲录制 ----
		void RecordAcquireBarriers(VkCommandBuffer cmd);
		void ResetMipFeedback(VkCommandBuffer cmd, std::uint32_t frame_index);
		void RecordMipFeedbackCopy(VkCommandBuffer cmd, std::uint32_t frame_index);
		void BindMipFeedback(VkDescriptorSet set0, std::uint32_t frame_index);

	private:
		void _init_default_image();
		void _init_other();
		void _insert_name_cache(const std::string& name, Texture2DHandle handle);
		void _remove_name_cache_by_handle(const Texture2DHandle& handle);
		void _update_texture_count();
		void _transition_image_layout(VkCommandBuffer commond_buffer,
			VkImage image,
			VkFormat format,
			VkImageLayout old_layout,
			VkImageLayout new_layout,
			uint32_t base_mip_level = 0,
			uint32_t level_count = 1);
		void _upload_texture_and_generate_mipmaps(VkBuffer staging_buffer,
			VkImage tex_image,
			VkFormat trans_format,
			uint32_t width,
			uint32_t height,
			uint32_t level_count = 1,
			VkDeviceSize buffer_offset = 0,
			VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED);

		bool _load_dds_to_compressed_data(const std::string& filepath,
			LoadDDSType type,
			ITextureCompresser::CompressedTextureData& out,
			VkFormat& format);

		Texture2DHandle _find_handle_from_cache(const std::string& name) const;

		// ===== 流式内部 =====
		void _init_transfer_resources();
		void _destroy_transfer_resources();
		void _init_mip_feedback(std::uint32_t count);
		void _resize_mip_feedback(std::uint32_t count);
		void _destroy_mip_feedback();
		void _resize_stream_states(size_t count);
		void _request_retarget(std::uint32_t slot, std::uint32_t target);
		void _kick_upload_task();						// 投递上传任务到工作线程池
		void _upload_task_func();						// 上传任务体 (线程池线程执行)
		void _process_upload_batch(const std::vector<std::uint32_t>& slots);
		VkCommandBuffer _begin_transfer_command();
		void _submit_transfer_and_wait(VkCommandBuffer cmd);
		bool _ensure_upload_staging(VkDeviceSize need);
		void _record_release_barrier(VkCommandBuffer cmd, VkImage image);	// 传输队列 -> 图形队列 所有权释放
		void _stop_upload();
		void _evict_texture(std::uint32_t slot);
		static std::uint32_t _get_default_texture_index(TextureType type);

	private:
		std::vector<IVulkanTexture2DHandle> _default_textures;
		std::vector<IVulkanTexture2DHandle> _textures;
		std::vector<TextureData> _textures_data;

		VkQueue _transfer_queue = VK_NULL_HANDLE;

		IBitVectorSafe _bit_vector_used{};
		IBitVectorSafe _bit_vector_valid{};

		mutable	std::shared_mutex _textures_mutex;	// 保护 _textures/_textures_data 的结构变化 (resize/销毁)
		mutable	std::shared_mutex _cache_mutex;

		TextureNameMap* _texture_name_cache = nullptr;
		TextureHandleNameMap* _texture_handle_name_cache = nullptr;

		// ===== 流式状态 =====
		std::unique_ptr<TextureStreamState[]> _stream_states;
		size_t _stream_states_count = 0;
		std::vector<std::uint16_t> _unreferenced_frames;	// 主线程: 驱逐统计 (连续未引用帧数)

		// ===== 上传队列 (生产者: 主线程/DDS工作线程; 消费者: 线程池上传任务) =====
		std::mutex _upload_mutex;						// 保护 _upload_queue/_upload_queued_set/_upload_task_running
		std::condition_variable _upload_idle_cv;		// WaitForUploadIdle / _stop_upload 用
		std::deque<std::uint32_t> _upload_queue;
		std::unordered_set<std::uint32_t> _upload_queued_set;
		bool _upload_task_running = false;				// 是否有上传任务在跑/已投递 (仅 _upload_mutex 内访问)
		std::atomic<bool> _upload_stop{ false };
		IThreadPool* _upload_pool = nullptr;			// 工作线程池 (Init 时捕获)

		// ===== 完成队列 (上传任务 -> 主线程) =====
		std::mutex _completed_mutex;
		std::vector<CompletedUpload> _completed_uploads;

		// acquire 待处理 (主线程私有, ProcessCompletedUploads 填充)
		std::vector<CompletedUpload> _pending_acquires;

		// ===== 传输资源 =====
		VkCommandPool _transfer_command_pool = VK_NULL_HANDLE;
		VkFence _upload_fence = VK_NULL_HANDLE;
		std::uint32_t _transfer_family_index = UINT32_MAX;
		std::uint32_t _graphics_family_index = UINT32_MAX;
		UploadStaging _upload_staging;

		// ===== GPU mip 反馈 (每帧独立的 device-local 缓冲 + host-visible 读回) =====
		std::uint32_t _mip_feedback_count = 0;
		std::array<VkBuffer, IVulkan::MAX_FRAMES_IN_FLIGHT> _mip_feedback_buffers{};
		std::array<VkBuffer, IVulkan::MAX_FRAMES_IN_FLIGHT> _mip_readback_buffers{};
		std::array<void*, IVulkan::MAX_FRAMES_IN_FLIGHT> _mip_readback_mapped{};
		std::deque<std::pair<std::uint64_t, VkBuffer>> _retired_buffers;	// 扩容时旧缓冲延迟销毁

		// ===== 帧延迟销毁环 (in-flight 帧可能仍引用旧 image/view) =====
		std::mutex _destroy_mutex;
		std::array<std::vector<PendingDestroy>, IVulkan::MAX_FRAMES_IN_FLIGHT + 1> _frame_destroy_ring;
		std::uint32_t _destroy_cursor = 0;
		std::uint64_t _frame_counter = 0;

		TextureResidentChangedCallback _resident_changed_callback;

		// ===== 策略参数 =====
		std::uint32_t _demote_hysteresis = 2;			// 降级滞后层数 (防抖)
		std::uint32_t _demote_per_frame = 4;			// 每帧降级上限
		std::uint32_t _upgrade_margin = 0;				// 升级预取层数 (0 = 精确按需)
		std::uint32_t _evict_frame_threshold = 600;		// 连续未引用帧数 -> 驱逐 (60fps 约 10 秒)
		VkDeviceSize _upload_batch_bytes_limit = 32ull * 1024 * 1024;	// 单批 staging 预算
		bool _auto_initial_load = true;					// DDS 就绪后自动驻留最粗层 (约一个 BC 块)

		bool _is_valid = false;
	};
}
