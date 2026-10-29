#include "EditorWindow.h"

#include "EditorConfig.h"

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

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"
#endif // _WIN32

namespace Editor
{
    const constexpr char* RegistryPath = "Config/Projects/projects.yaml";
    struct GameProjectInfo {
        std::string name;
        std::string path;
    };
    static std::vector<GameProjectInfo> RegisteredProjects;
    static size_t GameIndex{ 0 };

    HMODULE hGameDLL = nullptr;
    CreateGameModuleFunc  pfnCreateGame = nullptr;
    DestroyGameModuleFunc pfnDestroyGame = nullptr;
    IGameModule* GameModuleInstance = nullptr;


    static std::vector<ImDrawData*> CapturedDrawDatas;
    static std::vector<VkImageView> CachedImageViews;
    static std::vector<ImTextureID> ScenceTextureIDs;
    static ImGuiContext* ImguiContext = nullptr;

    static void CopyImDrawData(ImDrawData*& dst, const ImDrawData* src)
    {
        if (!src || !src->Valid) return;

        // 逐字段手动拷贝 ImDrawData，但不拷贝 CmdLists（避免混入 ImGui 内部指针）
        dst->Valid = src->Valid;
        dst->TotalIdxCount = src->TotalIdxCount;
        dst->TotalVtxCount = src->TotalVtxCount;
        dst->DisplayPos = src->DisplayPos;
        dst->DisplaySize = src->DisplaySize;
        dst->FramebufferScale = src->FramebufferScale;
        dst->OwnerViewport = src->OwnerViewport;
        dst->Textures = src->Textures;

        // 深拷贝每个 ImDrawList，只将克隆的列表加入 CmdLists
        for (int i = 0; i < src->CmdListsCount; i++)
        {
            ImDrawList* cloned_list = IM_NEW(ImDrawList)(src->CmdLists[i]->_Data);
            cloned_list->CmdBuffer = src->CmdLists[i]->CmdBuffer;
            cloned_list->IdxBuffer = src->CmdLists[i]->IdxBuffer;
            cloned_list->VtxBuffer = src->CmdLists[i]->VtxBuffer;
            cloned_list->Flags = src->CmdLists[i]->Flags;

            dst->CmdLists.push_back(cloned_list);
        }

        // 确保 CmdListsCount 与实际 Size 一致
        dst->CmdListsCount = dst->CmdLists.Size;
    }
}

bool EditorWindow::Start()
{
    INVENT::IEngine::Instatnce().Init();
    if (!INVENT::EngineAllocator::Init() || !INVENT::EngineAllocator::IsReady()) return false;
    Editor::ImguiContext = ImGui::CreateContext();
    Editor::CapturedDrawDatas.resize(INVENT::IRenderThreadBase::Instance().MaxFrameInFlight(), nullptr);
    Editor::CachedImageViews.resize(INVENT::IRenderThreadBase::Instance().MaxFrameInFlight(), VK_NULL_HANDLE);
    Editor::ScenceTextureIDs.resize(INVENT::IRenderThreadBase::Instance().MaxFrameInFlight(), ImTextureID{ 0 });

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
    INVENT::IRenderThreadBase::Instance().SetImguiVkInitFunction(
        [](const INVENT::IRenderThreadBase::ImguiVkInitInfo& info)->bool {
            if (Editor::ImguiContext)
            {
                ImGui::SetCurrentContext(Editor::ImguiContext);
            }
            else
            {
                INVENT_LOG_ERROR("[Editor] Imgui context is nullptr!");
                return false;
            }

            // 1. 填充官方要求的初始化資訊
            ImGui_ImplVulkan_InitInfo init_info{};
            init_info.Instance = info.instance;
            init_info.PhysicalDevice = info.physicalDevice;
            init_info.Device = info.logicalDevice;
            init_info.QueueFamily = info.queueFamilyIndex;
            init_info.Queue = info.graphicsQueue;
            init_info.DescriptorPool = info.imguiDescriptorPool;
            init_info.MinImageCount = info.minImageCount;
            init_info.ImageCount = info.imageCount;
            init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            // 2. 關鍵：設定 Vulkan 1.3 動態渲染
            init_info.UseDynamicRendering = true;

            static VkFormat color_format;
            color_format = info.colorAttachmentFormat;

            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = info.depthAttachmentFormat;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.stencilAttachmentFormat = info.stencilAttachmentFormat;

            return ImGui_ImplVulkan_Init(&init_info);
        }
    );
    INVENT::IRenderThreadBase::Instance().SetImguiVkShutdownFunction(
        []() {
            ImGui::SetCurrentContext(Editor::ImguiContext);
            ImGui_ImplVulkan_Shutdown();
        }
    );
    INVENT::IRenderThreadBase::Instance().SetImguiNewFrameFunc(
        []() {
            ImGui::SetCurrentContext(Editor::ImguiContext);

            ImGui_ImplVulkan_NewFrame();
            auto& engine = INVENT::IEngine::Instatnce();
            ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(engine.GetWindowWidth()), static_cast<float>(engine.GetWindowHeight()));

            ImGui::NewFrame();
        }
    );
    INVENT::IRenderThreadBase::Instance().SetImguiEndFrameAndFetchDataFunc(
        [](std::uint32_t frame_index, void* image_view, void* sampler)->void* {
            ImGui::SetCurrentContext(Editor::ImguiContext);

            VkImageView currentView = reinterpret_cast<VkImageView>(image_view);
            VkSampler Sampler = reinterpret_cast<VkSampler>(sampler);

            if (currentView != VK_NULL_HANDLE && Sampler != VK_NULL_HANDLE)
            {
                // 如果這一幀的 ImageView 變了
                if (Editor::CachedImageViews[frame_index] != currentView)
                {
                    // 釋放舊的 ImGui 描述符（防止 Pool 記憶體爆炸）
                    if (Editor::ScenceTextureIDs[frame_index] != 0)
                    {
                        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(Editor::ScenceTextureIDs[frame_index]));
                        Editor::ScenceTextureIDs[frame_index] = ImTextureID{ 0 };
                    }

                    VkDescriptorSet newSet = ImGui_ImplVulkan_AddTexture(Sampler, currentView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    Editor::ScenceTextureIDs[frame_index] = reinterpret_cast<ImTextureID>(newSet);
                    Editor::CachedImageViews[frame_index] = currentView;
                }
            }

            // ---- 繪製編輯器的 Scene Viewport ----
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("Scene Viewport");

            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            static ImVec2 lastSize = { 0, 0 };
            if (viewportSize.x != lastSize.x || viewportSize.y != lastSize.y)
            {
                if (viewportSize.x > 0 && viewportSize.y > 0)
                {
                    INVENT::IRenderThreadBase::Instance().OnViewportResize(static_cast<std::uint32_t>(viewportSize.x), static_cast<std::uint32_t>(viewportSize.y));
                    lastSize = viewportSize;

                }
            }

            if (Editor::ScenceTextureIDs[frame_index] != ImTextureID{ 0 })
            {
                ImGui::Image(Editor::ScenceTextureIDs[frame_index], viewportSize, ImVec2(0, 0), ImVec2(1, 1));
            }
            else
            {
                ImGui::Text("Viewport Ready. Waiting for Engine Active Frame...");
            }

            ImGui::Button("If you see this, ImGui works!");

            ImGui::End();
            ImGui::PopStyleVar();

            // 結束狀態機並執行深拷貝
            ImGui::Render();
            ImDrawData* rawData = ImGui::GetDrawData();

            if (Editor::CapturedDrawDatas.empty()) return nullptr;

            ImDrawData* frameDrawData = Editor::CapturedDrawDatas[frame_index];
            if (frameDrawData)
            {
                auto& CmdLists = frameDrawData->CmdLists;
                for (int i = 0; i < CmdLists.Size; i++)
                {
                    IM_DELETE(CmdLists[i]);
                }
                IM_DELETE(frameDrawData);
                Editor::CapturedDrawDatas[frame_index] = nullptr;
            }

            Editor::CapturedDrawDatas[frame_index] = IM_NEW(ImDrawData)();
            Editor::CopyImDrawData(Editor::CapturedDrawDatas[frame_index], rawData);

            return static_cast<void*>(Editor::CapturedDrawDatas[frame_index]);
        }
    );
    INVENT::IRenderThreadBase::Instance().SetImguiRenderDrawDataFunc(
        [](VkCommandBuffer cmd, void* pCapturedDrawData) {
            ImGui::SetCurrentContext(Editor::ImguiContext);
            //INVENT_LOG_DEBUG("ImguiRenderDrawData.");

            if (!pCapturedDrawData) return;
            ImDrawData* drawData = static_cast<ImDrawData*>(pCapturedDrawData);
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
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

    // delete imgui
    for (auto frameDrawData : Editor::CapturedDrawDatas)
    {
        if (frameDrawData)
        {
            auto& CmdLists = frameDrawData->CmdLists;
            for (int i = 0; i < CmdLists.Size; i++)
            {
                IM_DELETE(CmdLists[i]);
            }
            IM_DELETE(frameDrawData);
        }
    }
    Editor::CapturedDrawDatas.clear();
    ImGui::DestroyContext(Editor::ImguiContext);

//#if !WITH_EDITOR
    End();
    UnLoadGame();
//#endif

    INVENT::EngineAllocator::Clear();
    INVENT::IEngine::Instatnce().Shutdown();
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
        for (const auto& node : root)
        {
            Editor::GameProjectInfo& proj = Editor::RegisteredProjects.emplace_back();
            proj.name = node["name"].as<std::string>();
            proj.path = node["path"].as<std::string>();
        }
    }
    catch (...)
    {
        INVENT_LOG_ERROR(std::format("[Editor] 解析项目清单失败 : {}", Editor::RegistryPath));
        return false;
    }

    // 获取到全部游戏项目
    INVENT_LOG_INFO(std::format("[Editor] 获取到游戏项目数量: {}. 项目: ", Editor::RegisteredProjects.size()));
    std::uint32_t num = 0;
    for (auto& game : Editor::RegisteredProjects)
    {
        INVENT_LOG_INFO(std::format("\t {}. name : {}. path : {}", ++num, game.name, game.path));
    }

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

void EditorWindow::_framebuffer_size_callback(int width, int height)
{
    INVENT::IEngine::Instatnce().ChangeWindowSize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
}

void EditorWindow::_register_key_callback(int key, int scancode, int action, int mods)
{

}

void EditorWindow::_cursor_position_callback(double xpos, double ypos)
{
    // imgui
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(xpos), static_cast<float>(ypos));
}

void EditorWindow::_mouse_button_callback(int button, int action, int mods)
{

    // imgui
    ImGuiIO& io = ImGui::GetIO();
    int imgui_button = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT) imgui_button = 0;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) imgui_button = 1;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) imgui_button = 2;
    io.AddMouseButtonEvent(imgui_button, action == GLFW_PRESS);
}

void EditorWindow::_scroll_callback(double xoffset, double yoffset)
{

    // imgui
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
}

void EditorWindow::_cursor_enter_callback(int entered)
{}

std::string EditorWindow::_get_dll_path()
{
    const auto& game = Editor::RegisteredProjects[Editor::GameIndex];
    std::string path = game.path;
    std::string dllPath = path + "/lib/" + CURRENT_BUILD_CONFIG + "/" + game.name + ".dll";

    return dllPath;
}

bool EditorWindow::_get_and_set_required_instance_extensions()
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
