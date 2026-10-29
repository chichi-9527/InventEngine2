#pragma once

#include "IWindow.h"
#include <string>

class StarterWindow : public INVENT::IWindow
{
public:
	StarterWindow() = default;
	virtual ~StarterWindow() = default;

	virtual bool Start() override;

protected:

	void Begin();
	void Tick(float delta);
	void End();

	static bool GetGameProjects();
	static bool LoadGame();
	static void UnLoadGame();

protected:
	virtual void _framebuffer_size_callback(int width, int height) override;
	virtual void _register_key_callback(int key, int scancode, int action, int mods) override;
	virtual void _cursor_position_callback(double xpos, double ypos) override;
	virtual void _mouse_button_callback(int button, int action, int mods) override;
	virtual void _scroll_callback(double xoffset, double yoffset) override;
	virtual void _cursor_enter_callback(int entered) override;

private:
	static std::string _get_dll_path();
	bool _get_and_set_required_instance_extensions();

private:

};
