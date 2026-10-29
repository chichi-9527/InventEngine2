#include "StarterWindow.h"

#include <ILog.h>
#include <IGameModule.h>
#include <IRenderThreadBase.h>
#include <IEngine.h>
#include <Memory/Memory.h>

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

namespace Starter
{

    const constexpr char* RegistryPath = "Config/GameConfig/game_config.yaml";
    struct GameProjectInfo {
        std::string name;
    };
    static GameProjectInfo GameInfo{};


    HMODULE hGameDLL = nullptr;
    CreateGameModuleFunc  pfnCreateGame = nullptr;
    DestroyGameModuleFunc pfnDestroyGame = nullptr;
    IGameModule* GameModuleInstance = nullptr;

}

bool StarterWindow::Start()
{
    INVENT::IEngine::Instatnce().Init();
    if (!INVENT::EngineAllocator::Init() || !INVENT::EngineAllocator::IsReady()) return false;

    //
    if (!GetGameProjects())
    {
        return false;
    }

    if (!_get_and_set_required_instance_extensions()) return false;
    // start render loop
    INVENT::IRenderThreadBase::Instance().SetCreateSurfaceFunction(
        [](VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)->VkResult
        {
            return glfwCreateWindowSurface(instance, IWindow::_glfw_window, allocator, surface);
        }
    );

    INVENT::IEngine::Instatnce().ChangeWindowSize(this->_window_size.Width, this->_window_size.Height);
    if (!INVENT::IRenderThreadBase::Instance().Init()) // init imgui
    {
        INVENT::IRenderThreadBase::Instance().Shutdown();
        Terminate();
        return false;
    }

    if (!INVENT::IRenderThreadBase::Instance().Start())
    {
        INVENT::IRenderThreadBase::Instance().Shutdown();
        Terminate();
        return false;
    }

    if (!LoadGame())
    {
        return false;
    }
    Begin();

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

    INVENT::IEngine::Instatnce().EngineExit();
    INVENT::IRenderThreadBase::Instance().Shutdown();

    End();
    UnLoadGame();

    INVENT::EngineAllocator::Clear();
    INVENT::IEngine::Instatnce().Shutdown();
    return true;
}

void StarterWindow::Begin()
{
    Starter::GameModuleInstance->BeginPlay();
}

void StarterWindow::Tick(float delta)
{
    Starter::GameModuleInstance->Tick(delta);
}

void StarterWindow::End()
{
    Starter::GameModuleInstance->EndPlay();
}

bool StarterWindow::GetGameProjects()
{
    if (!std::filesystem::exists(Starter::RegistryPath))
    {
        INVENT_LOG_ERROR(std::format("[Game] not find config file : {}", Starter::RegistryPath));
        return false;
    }
    try
    {
        YAML::Node root = YAML::LoadFile(Starter::RegistryPath);

        Starter::GameInfo.name = root["name"].as<std::string>();

    }
    catch (...)
    {
        INVENT_LOG_ERROR(std::format("[Starter] 解析项目清单失败 : {}", Starter::RegistryPath));
        return false;
    }

#if WITH_Starter
    // 获取到全部游戏项目
    INVENT_LOG_INFO(std::format("[Starter] 获取到游戏项目数量: {}. 项目: ", Starter::RegisteredProjects.size()));
    std::uint32_t num = 0;
    for (auto& game : Starter::RegisteredProjects)
    {
        INVENT_LOG_INFO(std::format("\t {}. name : {}. path : {}", ++num, game.name, game.path));
    }
#else
    if (Starter::GameInfo.name.empty())
    {
        INVENT_LOG_ERROR(std::format("[Game] 找不到游戏文件 配置文件路径 : {}", Starter::RegistryPath));
        return false;
    }
#endif

    return true;
}

bool StarterWindow::LoadGame()
{
    std::string dllPath = StarterWindow::_get_dll_path();
    if (!std::filesystem::exists(dllPath))
    {
        INVENT_LOG_ERROR(std::format("[Game] 找不到游戏动态库: {}. 请先编译该游戏项目！", dllPath));
        return false;
    }

    Starter::hGameDLL = LoadLibraryA(dllPath.c_str());
    if (!Starter::hGameDLL)
    {
        INVENT_LOG_ERROR(std::format("[Game] 無法加載 DLL，錯誤代碼: {}.", GetLastError()));
        return false;
    }
    Starter::pfnCreateGame = (CreateGameModuleFunc)GetProcAddress(Starter::hGameDLL, "CreateGameModule");
    Starter::pfnDestroyGame = (DestroyGameModuleFunc)GetProcAddress(Starter::hGameDLL, "DestroyGameModule");
    if (!Starter::pfnCreateGame || !Starter::pfnDestroyGame)
    {
        INVENT_LOG_ERROR("[Game] 遊戲 DLL 格式不正確，找不到 Create/Destroy 導出符號！");
        FreeLibrary(Starter::hGameDLL);
        return false;
    }
    // 实例化游戏模组
    Starter::GameModuleInstance = Starter::pfnCreateGame();
    if (!Starter::GameModuleInstance)
    {
        INVENT_LOG_ERROR("[Game] 游戏模组实例失败! ");
        return false;
    }

    INVENT_LOG_INFO("[Game] 游戏已加载.");
    return true;
}

void StarterWindow::UnLoadGame()
{
    if (Starter::GameModuleInstance && Starter::pfnDestroyGame)
    {
        Starter::pfnDestroyGame(Starter::GameModuleInstance);
        Starter::GameModuleInstance = nullptr;
    }
    if (Starter::hGameDLL)
    {
        FreeLibrary(Starter::hGameDLL);
        Starter::hGameDLL = nullptr;
    }
    INVENT_LOG_INFO("[Game] 外部遊戲模組已安全卸載。");
}

void StarterWindow::_framebuffer_size_callback(int width, int height)
{
    INVENT::IEngine::Instatnce().ChangeWindowSize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
}

void StarterWindow::_register_key_callback(int key, int scancode, int action, int mods)
{

}

void StarterWindow::_cursor_position_callback(double xpos, double ypos)
{
    

}

void StarterWindow::_mouse_button_callback(int button, int action, int mods)
{

    
}

void StarterWindow::_scroll_callback(double xoffset, double yoffset)
{

    
}

void StarterWindow::_cursor_enter_callback(int entered)
{}

std::string StarterWindow::_get_dll_path()
{
    std::string dllPath = std::string("./") + Starter::GameInfo.name + ".dll";
    return dllPath;
}

bool StarterWindow::_get_and_set_required_instance_extensions()
{
    uint32_t extensionCount = 0;
    const char** extensionNames;
    extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (!extensionNames)
    {
        INVENT_LOG_ERROR("[Window] 返回 GLFW 所需的 Vulkan 实例扩展失败！");
        return false;
    }
    for (size_t i = 0; i < extensionCount; i++)
    {
        INVENT::IRenderThreadBase::Instance().SetVulkanInstanceExtension(extensionNames[i]);
    }
    return true;
}
