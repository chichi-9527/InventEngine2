#pragma once

#include "IVulkanGlobalTexture.h"

#include <cstdint>

namespace INVENT
{

	enum IMaterialTextureReadyFlags : std::uint32_t
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
	enum IMaterialAttributeFlag : std::uint32_t
	{
		UnDefined = 0,
		Metal = 1,
		NonMetal = 2,
	};
	struct Material
	{
		// 
		uint32_t DiffuseTextureId{ 0 };
		// 法线贴图
		uint32_t NormalTextureId{ 0 };
		// 镜面反射贴图
		uint32_t SpecularTextureId{ 0 };
		// 自发光
		uint32_t EmissionTextureId{ 0 };
		// 环境遮挡
		uint32_t AoTextureId{ 0 };
		// 不透明度
		uint32_t OpacityTextureId{ 0 };
		// 粗糙度
		uint32_t RoughnessTextureId{ 0 };
		// 透明涂层
		uint32_t ClearCoatTextureId{ 0 };

		// 材质属性 IMaterialAttributeFlag
		uint32_t MaterialAttribute = IMaterialAttributeFlag::UnDefined;
		// 贴图标记（是否准备好）
		uint32_t textureReadyFlags = IMaterialTextureReadyFlags::ReadyBitAllNotReady;
	};

}
