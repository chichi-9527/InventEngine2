#pragma once


#include <cstdint>
#include <string>
#include <atomic>

struct GLFWwindow;
struct GLFWmonitor;
struct GLFWvidmode;

namespace INVENT
{
	class IWindow
	{
	public:
		struct WindowSize 
		{
			uint32_t Width{ 800 };
			uint32_t Height{ 600 };
			bool Changed{ false };
		};

		IWindow() = default;
		virtual ~IWindow() = default;

		bool InitWindow(uint32_t width = 800,
			uint32_t height = 600,
			std::string title = "Editor",
			bool is_resizable = true);
		void Terminate();

		virtual bool Start() = 0;

		const WindowSize GetCurrentWindowSize() const noexcept
		{
			return _window_size.load(std::memory_order_acquire);
		}
	private:

		void _framebuffer_size_callback(int width, int height);
		void _register_key_callback(int key, int scancode, int action, int mods);
		void _cursor_position_callback(double xpos, double ypos);
		void _mouse_button_callback(int button, int action, int mods);
		void _scroll_callback(double xoffset, double yoffset);
		void _cursor_enter_callback(int entered);

	protected:
		static inline GLFWwindow* _glfw_window = nullptr;
		static inline GLFWmonitor* _glfw_monitor = nullptr;
		static inline const GLFWvidmode* _glfw_vidmode = nullptr;

		std::atomic<WindowSize> _window_size{};

		bool _is_resizable = true;

	};
}
