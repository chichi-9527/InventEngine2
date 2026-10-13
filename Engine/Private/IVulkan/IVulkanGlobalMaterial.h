#pragma once

#include "IBitArray.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>

namespace INVENT
{

	enum IMaterialTextureReadyFlags : std::uint16_t
	{
		ReadyBitAllNotReady = 0,
		ReadyBitDeffuse = 1 << 0,
		ReadyBitNormal = 1 << 1,
		ReadyBitSpecular = 1 << 2,
		ReadyBitEmission = 1 << 3,
		ReadyBitAo = 1 << 4,
		ReadyBitOpacity = 1 << 5,
		ReadyBitRoughness = 1 << 6,
		ReadyBitClearCoat = 1 << 7
	};
	enum IMaterialAttributeFlag : std::uint16_t
	{
		UnDefined = 0,
		Metal = 1,
		NonMetal = 2,
	};

	struct Material
	{
		// 材质属性 IMaterialAttributeFlag
		std::uint16_t MaterialAttribute = IMaterialAttributeFlag::UnDefined;
		// 贴图标记（是否准备好）
		std::uint16_t textureReadyFlags = IMaterialTextureReadyFlags::ReadyBitAllNotReady;

		// 
		std::uint16_t DiffuseTextureId{ 0 };
		// 法线贴图
		std::uint16_t NormalTextureId{ 0 };
		// 镜面反射贴图
		std::uint16_t SpecularTextureId{ 0 };
		// 自发光
		std::uint16_t EmissionTextureId{ 0 };
		// 环境遮挡
		std::uint16_t AoTextureId{ 0 };
		// 不透明度
		std::uint16_t OpacityTextureId{ 0 };
		// 粗糙度
		std::uint16_t RoughnessTextureId{ 0 };
		// 透明涂层
		std::uint16_t ClearCoatTextureId{ 0 };

		// 
		std::uint8_t padding[12];
	};
	static_assert(sizeof(Material) == 32);
	// shader 侧 Material 布局必须与此逐字节一致 (BDA 裸指针加载不受 std430 对齐约束, 全靠两侧声明一致)

	/*
	* 全局材质管理 (CPU 权威副本 + 脏标记 + 帧首批量上传):
	*
	*   - GPU 侧: 多个固定容量 device-local VkBuffer (STORAGE + BDA)
	*     单 buffer 满时新建同尺寸 buffer (_add_one_buffer), 位图定位空槽
	*     槽位永不移动 / buffer 永不重分配 -> BDA 永久稳定,
	*     actor 创建 instance 时 GetMaterialAddress 查询一次存入 InstanceData.MaterialAddress
	*
	*   - CPU 侧: 每 buffer 一份镜像 (权威副本); Add/Update 写镜像 + 标脏, 任意线程可调
	*     (动画组件改帧 = UpdateMaterial 换 TextureId)
	*
	*   - FlushDirtyMaterials(cmd, frame): 渲染线程帧首调用, 录入本帧命令缓冲
	*     [staging 拷贝 -> vkCmdCopyBuffer -> 读屏障], 在所有使用材质的 CS/绘制之前;
	*     staging 按 MAX_FRAMES_IN_FLIGHT 分槽, BeginRender 的 fence 等待保证复用安全
	*
	*   - 销毁: 槽位回收, GPU 数据为墓碑; 引用它的 instance 由上层负责失效 (管理类不追踪反向引用)
	*
	*   - 与其他管理类零依赖; actor 是唯一粘合层
	*/
	class IVulkanGlobalMaterial
	{
		static constexpr std::uint32_t INVALID_32BIT = UINT32_MAX;
		struct ManagerData
		{
			std::uint32_t bufferID{ INVALID_32BIT };
			IHandle materialID{};
		};

		IVulkanGlobalMaterial() = default;
	public:
		~IVulkanGlobalMaterial() = default;

		using IGMHandle = std::uint32_t;
		static constexpr IGMHandle INVALID_IGM_HANDLE = INVALID_32BIT;

		static IVulkanGlobalMaterial& Instance();

		bool Init();
		void Clear();

		/// <summary> 写入 CPU 镜像并标脏, 下次 FlushDirtyMaterials 上传 </summary>
		/// <returns> 失败返回 INVALID_IGM_HANDLE </returns>
		IGMHandle AddMaterial(const Material& m);
		/// <summary> 原地更新 (地址不变), 写镜像并标脏; 动画换帧/参数修改走这里 </summary>
		void UpdateMaterial(IGMHandle handle, const Material& m);
		void DestoryMaterial(IGMHandle handle);

		// ===== BDA (actor 创建 instance 时查询一次, 地址永久稳定) =====
		/// <returns> 材质 GPU 地址; 无效返回 0 </returns>
		VkDeviceAddress GetMaterialAddress(IGMHandle handle) const;

		// CPU 权威副本读取 (编辑器/actor 缓存初始值)
		bool GetMaterial(IGMHandle handle, Material& out) const;

		/// <summary>
		/// 帧首上传脏材质 (渲染线程, 录制命令缓冲时):
		/// 录入 [staging 拷贝 + buffer 拷贝 + transfer->shader 读屏障]
		/// 必须在本帧所有读取材质的 CS/绘制之前调用
		/// </summary>
		void FlushDirtyMaterials(VkCommandBuffer cmd, std::uint32_t frame_index);

		bool IsValid() const { return _is_valid; }
		std::size_t GetBufferCount() const { return _buffer_datas.size(); }

	private:
		bool _add_one_buffer();
		bool _ensure_frame_staging(std::uint32_t frame_index, VkDeviceSize need);
		void _mark_dirty_nolock(IGMHandle handle);

	private:
		// 材质 buffer 族 (每槽一份, 槽 = 帧)
		struct BufferData
		{
			VkBuffer GpuBuffer = VK_NULL_HANDLE;		// device local
			VkDeviceAddress GpuAddress{ 0 };			// BDA (永久稳定)
			std::vector<Material> CpuMirror;			// 权威副本 (DEF_MATERIAL_COUNT_PER_BUFFER 份)
			IBitVectorSafe* Slots = nullptr;			// 槽位占用位图
		};
		std::vector<BufferData> _buffer_datas;

		// 每帧槽 staging (host visible + coherent; fence 等待保证复用安全)
		struct FrameStaging
		{
			VkBuffer Buffer = VK_NULL_HANDLE;
			void* Mapped = nullptr;
			VkDeviceSize Capacity{ 0 };
		};
		std::vector<FrameStaging> _frame_stagings;		// MAX_FRAMES_IN_FLIGHT

		std::vector<ManagerData> _datas;
		std::vector<IGMHandle> _free_handles;

		mutable std::mutex _mutex;						// 保护 _buffer_datas/_datas/镜像写
		std::mutex _dirty_mutex;						// 保护脏集
		std::vector<IGMHandle> _dirty_handles;			// 待上传
		std::vector<bool> _dirty_flags;					// 按 handle 去重

		bool _is_valid = false;
	};

}
