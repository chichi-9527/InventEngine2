
file(GLOB_RECURSE STARTER_SRCS "src/*.cpp")
file(GLOB_RECURSE STARTER_INCS "src/*.h")

set(STARTER_ALL ${STARTER_SRCS} ${STARTER_INCS})

add_executable(EngineStarter ${STARTER_ALL})

set_target_properties(EngineStarter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/EngineStarter"
)

target_precompile_headers(EngineStarter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/starter_pch.h"
)

# 源文件分组
foreach(FILE ${STARTER_ALL})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK")
set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/Lua")
set(ENGINE_EDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Engine/EditorInclude")

target_include_directories(EngineStarter
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Private
    PRIVATE ${LUA_DIR}/include
    PRIVATE ${GLOBAL_VULKAN_SDK_INC}
    PRIVATE ${ENGINE_EDITOR_INCLUDE_DIR}
)

target_link_libraries(EngineStarter
    PRIVATE Engine
    PRIVATE SlangCompiler
    PRIVATE glfw
    PRIVATE yaml-cpp
    PRIVATE ${GLOBAL_VULKAN_SDK_LIB}/vulkan-1.lib
    PRIVATE ${LUA_DIR}/lib/lua54.lib
)

target_compile_definitions(EngineStarter
    PRIVATE SHOW_LOGS
    PRIVATE "CURRENT_BUILD_CONFIG=\"$<CONFIG>\""
    PRIVATE "INVENT_EDITOR=0"
)

set_target_properties(EngineStarter PROPERTIES
    OUTPUT_NAME "GameStarter"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# sdk 目录
set(SDK_DIR_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/bin")

add_custom_command(TARGET EngineStarter POST_BUILD
    # 复制 Engine.dll 到 EngineStarter 目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Engine>
    $<TARGET_FILE_DIR:EngineStarter>

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang.dll"
    "$<TARGET_FILE_DIR:EngineStarter>/slang.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slangd.dll"
    "$<TARGET_FILE_DIR:EngineStarter>/slangd.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang-compiler.dll"
    "$<TARGET_FILE_DIR:EngineStarter>/slang-compiler.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang-glslang.dll"
    "$<TARGET_FILE_DIR:EngineStarter>/slang-glslang.dll"

    # 复制 SDK dll 文件到 EngineStarter 目录
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SDK_DIR_BIN}"
    $<TARGET_FILE_DIR:EngineStarter>

    # 复制 GameConfig 到 EngineStarter 目录
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../Config/GameConfig"
    "$<TARGET_FILE_DIR:EngineStarter>/Config/GameConfig"

    # 复制 Shaders 到 EngineStarter 目录
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../Config/Shaders"
    "$<TARGET_FILE_DIR:EngineStarter>/Config/Shaders"
)
