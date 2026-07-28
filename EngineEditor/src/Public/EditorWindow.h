#pragma once

#include "IWindow.h"
#include <string>

class EditorWindow : public INVENT::IWindow
{
public:
	EditorWindow() = default;
	virtual ~EditorWindow() = default;

	virtual bool Start() override;

	void Begin();
	void Tick(float delta);
	void End();

	static bool SeleteGame();
	static bool LoadGame();

private:
	static std::string _get_dll_path();

};
