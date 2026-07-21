
cmake_minimum_required(VERSION 3.20)
project(InventEngine2)

if(WIN32)
    set(ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    #添加环境变量
    execute_process(
        COMMAND cmd /c setx INVENT_ENGINE_DIR "${ENGINE_ROOT_DIR}"
        OUTPUT_QUIET
        ERROR_QUIET
    )
    message(STATUS "注册环境变量 : INVENT_ENGINE_DIR: ${ENGINE_ROOT_DIR}")
endif()

option(ENGINE_BUILD_EDITOR "Build the engine with editor functionality" ON)
option(ENGINE_DEVELOPMENT_MODE "Build develop engine with open vulkan validation layer" ON)
set(GAME_NAME "MyGame" CACHE STRING "The name of the output executable")

# 项目分组
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

macro(folder_organize target_name folder_path)
    if(TARGET ${target_name})
        set_target_properties(${target_name} PROPERTIES FOLDER "${folder_path}")
    endif()
endmacro()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()


# 不导出所有符号
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

if(MSVC)
    add_compile_options(/MP)

    # 替换所有 /MT 为 /MD
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    )

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

endif()


# 第三方库
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# 強制關閉 CTest 的內置測試生成
set(BUILD_TESTING OFF CACHE BOOL "Disable CTest" FORCE)

# assimp
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

# freetype
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# bullet3
set(BUILD_BULLET3 OFF CACHE BOOL "Only build bullet2 core libs" FORCE) # 引擎底層通常使用相容性更好的 bullet2 核心
set(BUILD_DEMOS OFF CACHE BOOL "No  demos" FORCE)                       #关闭展示程式
set(BUILD_EXTRAS OFF CACHE BOOL "No extras" FORCE)                     # 關閉額外插件
set(BUILD_UNIT_TESTS OFF CACHE BOOL "No tests" FORCE)                 # 關閉單元測試
set(INSTALL_LIBS OFF CACHE BOOL "No install" FORCE)                   # 關閉安裝指令
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "Force /MD" FORCE)      # 強制 Bullet 使用 /MD，與引擎對齊
set(BUILD_CPU_DEMOS OFF CACHE BOOL "Disable CPU Demos" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Disable Bullet2 Demos" FORCE)

# yaml-cpp 配置
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "No tests" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "No tools" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "No contrib" FORCE)

# 第三方库
add_subdirectory(ThirdParty/Other/bullet3)
add_subdirectory(ThirdParty/Other/assimp)
add_subdirectory(ThirdParty/Other/freetype)
add_subdirectory(ThirdParty/Other/glfw)
add_subdirectory(ThirdParty/Other/yaml-cpp)

set(CMAKE_DEBUG_POSTFIX           "_d")
set(CMAKE_RELEASE_POSTFIX         "")
set(CMAKE_RELWITHDEBINFO_POSTFIX  "_rd")
set(CMAKE_MINSIZEREL_POSTFIX      "_ms")

# 子项目
add_subdirectory(SlangCompiler)
add_subdirectory(Engine)

# 编辑器项目
add_subdirectory(EngineEditor)

option(INVENT_ENGINE_TEST "Build test project" ON)
if(INVENT_ENGINE_TEST)
    # 测试项目
    add_subdirectory(TestEngine)
endif()

# 项目分组
folder_organize(assimp "ThirdParty")
folder_organize(zlibstatic "ThirdParty")
folder_organize(freetype "ThirdParty")
folder_organize(glfw "ThirdParty")
folder_organize(Bullet3Common "ThirdParty/Bullet")
folder_organize(BulletCollision "ThirdParty/Bullet")
folder_organize(BulletDynamics "ThirdParty/Bullet")
folder_organize(BulletInverseDynamics "ThirdParty/Bullet")
folder_organize(BulletSoftBody "ThirdParty/Bullet")
folder_organize(LinearMath "ThirdParty/Bullet")
folder_organize(yaml-cpp "ThirdParty")

folder_organize(flags "ThirdParty/Internal")
folder_organize(UpdateAssimpLibsDebugSymbolsAndDLLs "ThirdParty/Internal")

if(INVENT_ENGINE_TEST)
    folder_organize(TestEngine "InventTest")
endif()
