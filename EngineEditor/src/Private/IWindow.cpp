#include "IWindow.h"

#include <ILog.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif // _WIN32

namespace INVENT
{
	bool IWindow::InitWindow(uint32_t width, uint32_t height, std::string title, bool is_resizable)
	{
		this->_window_size.store({ width,height,true });
		this->_is_resizable = is_resizable;

		glfwSetErrorCallback([](int error, const char* description) {
			INVENT_LOG_ERROR(std::format("[GLFW] description : {} \n\terrorcode : {}", description, error));
			});

		// init glfw
		if (GLFW_FALSE == glfwInit())
		{
			INVENT_LOG_ERROR("[IWindow] GLFW init error!");
			return false;
		}
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		if (GLFW_FALSE == glfwVulkanSupported())
		{
			INVENT_LOG_ERROR("[IWindow] Vulkan is not supported!");
			return false;
		}
		// 是否可拉伸
		glfwWindowHint(GLFW_RESIZABLE, is_resizable);

		this->_glfw_monitor = glfwGetPrimaryMonitor();
		if (_glfw_monitor == nullptr)
		{
			INVENT_LOG_ERROR("[IWindow] no primary monitor!");
			return false;
		}
		this->_glfw_vidmode = glfwGetVideoMode(_glfw_monitor);
		this->_glfw_window = glfwCreateWindow(static_cast<int>(width),
			static_cast<int>(height),
			title.c_str(),
			_glfw_monitor, nullptr);
		if (_glfw_window == nullptr)
		{
			INVENT_LOG_ERROR("[IWindow] Failed to create a glfw window!");
			return false;
		}

		glfwSetWindowUserPointer(_glfw_window, this);

		glfwSetFramebufferSizeCallback(_glfw_window, [](GLFWwindow* window, int width, int height) {
			if (IWindow* iwindow = reinterpret_cast<IWindow*>(glfwGetWindowUserPointer(window)))
			{
				WindowSize size{ static_cast<uint32_t>(width), static_cast<uint32_t>(width) , true };
				iwindow->_window_size.store(size, std::memory_order_release);
			}
			});
		glfwSetKeyCallback(_glfw_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (IWindow* iwindow = (IWindow*)glfwGetWindowUserPointer(window))
			{
				iwindow->_register_key_callback(key, scancode, action, mods);
			}
			});
		glfwSetCursorPosCallback(_glfw_window, [](GLFWwindow* window, double xpos, double ypos) {
			if (IWindow* iwindow = (IWindow*)glfwGetWindowUserPointer(window))
			{
				iwindow->_cursor_position_callback(xpos, ypos);
			}
			});
		glfwSetMouseButtonCallback(_glfw_window, [](GLFWwindow* window, int button, int action, int mods) {
			if (IWindow* iwindow = (IWindow*)glfwGetWindowUserPointer(window))
			{
				iwindow->_mouse_button_callback(button, action, mods);
			}
			});
		glfwSetScrollCallback(_glfw_window, [](GLFWwindow* window, double xoffset, double yoffset) {
			if (IWindow* iwindow = (IWindow*)glfwGetWindowUserPointer(window))
			{
				iwindow->_scroll_callback(xoffset, yoffset);
			}
			});
		glfwSetCursorEnterCallback(_glfw_window, [](GLFWwindow* window, int entered) {
			if (IWindow* iwindow = (IWindow*)glfwGetWindowUserPointer(window))
			{
				iwindow->_cursor_enter_callback(entered);
			}
			});

		return true;
	}

	void IWindow::Terminate()
	{
		glfwTerminate();
	}

	void IWindow::_framebuffer_size_callback(int width, int height)
	{}
	void IWindow::_register_key_callback(int key, int scancode, int action, int mods)
	{}
	void IWindow::_cursor_position_callback(double xpos, double ypos)
	{}
	void IWindow::_mouse_button_callback(int button, int action, int mods)
	{}
	void IWindow::_scroll_callback(double xoffset, double yoffset)
	{}
	void IWindow::_cursor_enter_callback(int entered)
	{}
}
