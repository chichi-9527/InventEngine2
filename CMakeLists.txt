
cmake_minimum_required(VERSION 3.20)
project(InventEngine2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 不导出所有符号
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

if(MSVC)
    add_compile_options(/MP)

    # 替换所有 /MT 为 /MD
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    )

    foreach(flag ${CompilerFlags})
        if(${flag} MATCHES "/MT")
            string(REGEX REPLACE "/MT" "/MD" ${flag} "${${flag}}")
        endif()
    endforeach()

endif()


# 第三方库
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

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


# 子项目
add_subdirectory(ThirdParty/Other/bullet3)
add_subdirectory(ThirdParty/Other/assimp)
add_subdirectory(ThirdParty/Other/freetype)
add_subdirectory(ThirdParty/Other/glfw)
add_subdirectory(SlangCompiler)
add_subdirectory(Engine)

# 测试项目
add_subdirectory(TestEngine)