#include "EditorWindow.h"

#include "EditorConfig.h"

#include <ILog.h>
#include <IGameModule.h>

#include <filesystem>
#include <chrono>

#include <yaml-cpp/yaml.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif // _WIN32

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"
#endif // _WIN32

namespace Editor
{
#if WITH_EDITOR
    const constexpr char* RegistryPath = "Config/Projects/projects.yaml";
    struct GameProjectInfo {
        std::string name;
        std::string path;
    };
    static std::vector<GameProjectInfo> RegisteredProjects;
    static size_t GameIndex{ 0 };
#else
    const constexpr char* RegistryPath = "Config/GameConfig/game_config.yaml";
    struct GameProjectInfo {
        std::string name;
    };
    static GameProjectInfo GameInfo{};
#endif // WITH_EDITOR

    HMODULE hGameDLL = nullptr;
    CreateGameModuleFunc  pfnCreateGame = nullptr;
    DestroyGameModuleFunc pfnDestroyGame = nullptr;
    IGameModule* GameModuleInstance = nullptr;
}

bool EditorWindow::Start()
{
    //
    if (!GetGameProjects())
    {
        return false;
    }
//#if !WITH_EDITOR
    if (!LoadGame())
    {
        return false;
    }
//#endif

    Begin();

    // start render loop

    float deltaTime = 0.0f;
    auto lastFrame = std::chrono::high_resolution_clock::now();
    // main loop
    while (!glfwWindowShouldClose(_glfw_window))
    {
        auto currentFrame = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(currentFrame - lastFrame).count();
        lastFrame = currentFrame;

        Tick(deltaTime);


        glfwPollEvents();
    }

    End();

    UnLoadGame();
    return true;
}

void EditorWindow::Begin()
{
    Editor::GameModuleInstance->BeginPlay();
}

void EditorWindow::Tick(float delta)
{
    Editor::GameModuleInstance->Tick(delta);
}

void EditorWindow::End()
{
    Editor::GameModuleInstance->EndPlay();
}

bool EditorWindow::GetGameProjects()
{
    if (!std::filesystem::exists(Editor::RegistryPath))
    {
        INVENT_LOG_ERROR(std::format("[Game] not find config file : {}", Editor::RegistryPath));
        return false;
    }
    try
    {
        YAML::Node root = YAML::LoadFile(Editor::RegistryPath);
#if WITH_EDITOR
        for (const auto& node : root)
        {
            Editor::GameProjectInfo& proj = Editor::RegisteredProjects.emplace_back();
            proj.name = node["name"].as<std::string>();
            proj.path = node["path"].as<std::string>();
        }
#else
        GameInfo.name = root["name"].as<std::string>();
#endif // WITH_EDITOR
    }
    catch (...)
    {
        INVENT_LOG_ERROR(std::format("[Editor] 解析项目清单失败 : {}", Editor::RegistryPath));
        return false;
    }

#if WITH_EDITOR
    // 获取到全部游戏项目
    INVENT_LOG_INFO(std::format("[Editor] 获取到游戏项目数量: {}.", Editor::RegisteredProjects.size()));
    for (auto& game : Editor::RegisteredProjects)
    {
        INVENT_LOG_INFO(std::format("[Editor] 获取到游戏项目:\n\t name : {}. path : {}", game.name, game.path));
    }
#else
    if (Editor::GameInfo.name.empty())
    {
        INVENT_LOG_ERROR(std::format("[Game] 找不到游戏文件 配置文件路径 : {}", Editor::RegistryPath));
        return false;
    }
#endif

    return true;
}

bool EditorWindow::LoadGame()
{
    std::string dllPath = EditorWindow::_get_dll_path();
    if (!std::filesystem::exists(dllPath))
    {
        INVENT_LOG_ERROR(std::format("[Game] 找不到游戏动态库: {}. 请先编译该游戏项目！", dllPath));
        return false;
    }

    Editor::hGameDLL = LoadLibraryA(dllPath.c_str());
    if (!Editor::hGameDLL)
    {
        INVENT_LOG_ERROR(std::format("[Game] 無法加載 DLL，錯誤代碼: {}.", GetLastError()));
        return false;
    }
    Editor::pfnCreateGame = (CreateGameModuleFunc)GetProcAddress(Editor::hGameDLL, "CreateGameModule");
    Editor::pfnDestroyGame = (DestroyGameModuleFunc)GetProcAddress(Editor::hGameDLL, "DestroyGameModule");
    if (!Editor::pfnCreateGame || !Editor::pfnDestroyGame)
    {
        INVENT_LOG_ERROR("[Game] 遊戲 DLL 格式不正確，找不到 Create/Destroy 導出符號！");
        FreeLibrary(Editor::hGameDLL);
        return false;
    }
    // 实例化游戏模组
    Editor::GameModuleInstance = Editor::pfnCreateGame();
    if (!Editor::GameModuleInstance)
    {
        INVENT_LOG_ERROR("[Game] 游戏模组实例失败! ");
        return false;
    }

    INVENT_LOG_INFO("[Game] 游戏已加载.");
    return true;
}

void EditorWindow::UnLoadGame()
{
    if (Editor::GameModuleInstance && Editor::pfnDestroyGame)
    {
        Editor::pfnDestroyGame(Editor::GameModuleInstance);
        Editor::GameModuleInstance = nullptr;
    }
    if (Editor::hGameDLL)
    {
        FreeLibrary(Editor::hGameDLL);
        Editor::hGameDLL = nullptr;
    }
    INVENT_LOG_INFO("[Game] 外部遊戲模組已安全卸載。");
}

std::string EditorWindow::_get_dll_path()
{
#if WITH_EDITOR
    const auto& game = Editor::RegisteredProjects[Editor::GameIndex];
    std::string path = game.path;
    std::string dllPath = path + "/lib/" + CURRENT_BUILD_CONFIG + "/" + game.name + ".dll";
#else
    std::string dllPath = std::string("./") + Editor::GameInfo.name + ".dll";
#endif

    return dllPath;
}
