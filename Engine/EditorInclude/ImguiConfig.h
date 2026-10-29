#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace IImgui
{
	inline bool UseImgui{ false };

	using ImguiNewFrameFunc = void(*)();
	// 參數 1: frameIndex (0 或 1)
   // 參數 2: 當前幀 3D 離屏渲染的 VkImageView (強轉為 void*)
   // 參數 3: 你的 Bindless 全局靜態採樣器 VkSampler (強轉為 void*)
   // 返回值: 深度拷貝出來的 ImDrawData* (強轉為 void*)
	using ImguiEndFrameAndFetchDataFunc = void* (*)(std::uint32_t, void*, void*);
	using ImguiRenderDrawDataFunc = void(*)(VkCommandBuffer cmd, void* pCapturedDrawData); // in   ImDrawData*
	using ImguiClearDescriptorsBeforeReSizeFunc = void(*)();
	

}
