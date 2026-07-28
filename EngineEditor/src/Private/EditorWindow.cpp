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
    return true;
}

void EditorWindow::Begin()
{}

void EditorWindow::Tick(float delta)
{}

void EditorWindow::End()
{}

bool EditorWindow::SeleteGame()
{
    if (!std::filesystem::exists(Editor::RegistryPath))
    {
        INVENT_LOG_ERROR(std::format("[Editor] not find config file : {}", Editor::RegistryPath));
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
        INVENT_LOG_INFO(std::format("[Editor] 获取到游戏项目: name : {}. path : {}", game.name, game.path));
    }
#else
    if (Editor::GameInfo.name.empty())
    {
        INVENT_LOG_ERROR(std::format("[Editor] 找不到游戏文件 配置文件路径 : {}", Editor::RegistryPath));
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
        INVENT_LOG_ERROR(std::format("[Editor] 找不到游戏动态库: {}. 请先编译该游戏项目！", dllPath));
        return false;
    }
}

std::string EditorWindow::_get_dll_path()
{
#if WITH_EDITOR
    const auto& game = Editor::RegisteredProjects[Editor::GameIndex];
    std::string path = game.path;
    std::string dllPath = path + "/lib/" + CURRENT_BUILD_CONFIG + "/" + game.name + ".dll";
#else

#endif
}
