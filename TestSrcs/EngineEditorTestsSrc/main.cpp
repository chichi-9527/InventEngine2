
#include <IGameModule.h>
#include <ILog.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <imgui.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"
#endif // _WIN32

// 記錄遊戲專案資訊的結構體
struct GameProjectInfo {
    std::string name;
    std::string path;
};

// 全域變數，存儲所有讀取到的遊戲項目
std::vector<GameProjectInfo> g_RegisteredProjects;
GameProjectInfo* g_SelectedProject = nullptr;

// 運行時動態載入遊戲 DLL 所需的控制代碼與函數指針 [INDEX_1.3.4]
HMODULE hGameDLL = nullptr;
CreateGameModuleFunc  pfnCreateGame = nullptr;
DestroyGameModuleFunc pfnDestroyGame = nullptr;
IGameModule* g_GameModuleInstance = nullptr;

void LoadProjectRegistry()
{
    g_RegisteredProjects.clear();
    // 讀取項目清單（相對於之前設定的 VS_DEBUGGER_WORKING_DIRECTORY 根目錄）
    std::string registryPath = "EngineEditor/Projects/projects.yaml";

    if (!std::filesystem::exists(registryPath)) return;

    try
    {
        YAML::Node root = YAML::LoadFile(registryPath);
        for (const auto& node : root)
        {
            GameProjectInfo proj;
            proj.name = node["name"].as<std::string>();
            proj.path = node["path"].as<std::string>();
            g_RegisteredProjects.push_back(proj);
        }
    }
    catch (...)
    {
        std::cerr << "讀取項目清單失敗！" << std::endl;
    }
}

bool LoadGameModule(const std::string& dll_path, void* engineContext)
{
    if (!std::filesystem::exists(dll_path))
    {
        std::cerr << "[Editor Error] 找不到遊戲動態庫: " << dll_path << "\n請先編譯該遊戲項目！" << std::endl;
        return false;
    }

    // 1. 動態加載外部 DLL [INDEX_1.3.4]
    hGameDLL = LoadLibraryA(dll_path.c_str());
    if (!hGameDLL)
    {
        std::cerr << "[Editor Error] 無法加載 DLL，錯誤代碼: " << GetLastError() << std::endl;
        return false;
    }

    // 2. 抓取遊戲 DLL 導出的 C 函數指針 [INDEX_1.3.4]
    pfnCreateGame = (CreateGameModuleFunc)GetProcAddress(hGameDLL, "CreateGameModule");
    pfnDestroyGame = (DestroyGameModuleFunc)GetProcAddress(hGameDLL, "DestroyGameModule");

    if (!pfnCreateGame || !pfnDestroyGame)
    {
        std::cerr << "[Editor Error] 遊戲 DLL 格式不正確，找不到 Create/Destroy 導出符號！" << std::endl;
        FreeLibrary(hGameDLL);
            return false;
    }

    // 3. 實例化遊戲模組
    g_GameModuleInstance = pfnCreateGame();
    if (g_GameModuleInstance)
    {
        std::cout << "[Editor] 成功載入遊戲模組: mygame"<< std::endl;

        // 5. 呼叫遊戲的 OnStart，把 Lua 虛擬機正式移交給外部遊戲項目！
        // 外部遊戲此時就能在內部用 sol2 進行 C++ 類別綁定並調用 player.lua 了！
        g_GameModuleInstance->BeginPlay();
        return true;
    }

    return false;
}

void UnloadGameModule()
{
    if (g_GameModuleInstance && pfnDestroyGame)
    {
        g_GameModuleInstance->EndPlay();
        pfnDestroyGame(g_GameModuleInstance);
        g_GameModuleInstance = nullptr;
    }
    if (hGameDLL)
    {
        FreeLibrary(hGameDLL);
        hGameDLL = nullptr;
    }
    std::cout << "[Editor] 外部遊戲模組已安全卸載。" << std::endl;
}

int main()
{
    INVENT::ILog::Init();
    LoadProjectRegistry();
    for (auto& p : g_RegisteredProjects)
    {
        std::cout << "name: " << p.name << " ; path: " << p.path << std::endl;
    }

    std::string dllName = g_RegisteredProjects[0].path;

    std::string dllPath = dllName + "/lib/" + CURRENT_BUILD_CONFIG + "/" + g_RegisteredProjects[0].name + ".dll";

    LoadGameModule(dllPath, nullptr);

    getchar();

    UnloadGameModule();


    return 0;
}
