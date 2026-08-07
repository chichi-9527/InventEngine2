#pragma once

#include "IWindow.h"
#include <string>

class EditorWindow : public INVENT::IWindow
{
public:
	EditorWindow() = default;
	virtual ~EditorWindow() = default;

	virtual bool Start() override;

protected:

	void Begin();
	void Tick(float delta);
	void End();

	static bool GetGameProjects();
	static bool LoadGame();
	static void UnLoadGame();

private:
	static std::string _get_dll_path();
	bool _get_and_set_required_instance_extensions();
};
