#pragma once

#include "IRenderThreadBase.h"

#include <thread>
#include <atomic>
#include <memory>

namespace INVENT
{
	class IRenderThread : public IRenderThreadBase
	{
		friend class IRenderThreadBase;

		IRenderThread() = default;
	public:
		~IRenderThread() override = default;

		void SetCreateSurfaceFunction(CreateSurfaceFunc func) override { _create_surface = func; }
		void SetImguiVkInitFunction(ImguiVkInitFunc func) override { _imgui_init_func = func; }
		void SetImguiVkShutdownFunction(ImguiVkShutdownFunc func) override { _imgui_shutdown_func = func; }
		void SetImguiNewFrameFunc(IImgui::ImguiNewFrameFunc func) override;
		void SetImguiEndFrameAndFetchDataFunc(IImgui::ImguiEndFrameAndFetchDataFunc func) override;
		void SetImguiRenderDrawDataFunc(IImgui::ImguiRenderDrawDataFunc func) override;

		bool Init() override;
		bool Start() override;
		void Shutdown() override;

	private:
		bool _init_vulkan();
		void _shutdown_vulkan();
		void _render_loop();

		bool _init_imgui();

	private:
		CreateSurfaceFunc _create_surface = nullptr;
		ImguiVkInitFunc _imgui_init_func = nullptr;
		ImguiVkShutdownFunc _imgui_shutdown_func = nullptr;
		std::thread _thread;
		std::atomic_bool _running{ false };
	};
}
