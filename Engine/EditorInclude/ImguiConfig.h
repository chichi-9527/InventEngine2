#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace IImgui
{
	inline bool UseImgui{ false };

	using ImguiNewFrameFunc = void(*)();
	using ImguiEndFrameAndFetchDataFunc = void* (*)(std::uint32_t); // return  ImDrawData*   in frame index
	using ImguiRenderDrawDataFunc = void(*)(VkCommandBuffer cmd, void* pCapturedDrawData); // in   ImDrawData*
	

}
