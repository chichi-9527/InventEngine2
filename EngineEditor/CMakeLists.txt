
file(GLOB_RECURSE EDITOR_SRCS "src/*.cpp")
file(GLOB_RECURSE EDITOR_INCS "src/*.h")
file(GLOB_RECURSE IMGUI_FILES
    "Imgui_1_92_8/*.h"
    "Imgui_1_92_8/*.cpp"
)

set(EDITOR_ALL ${EDITOR_SRCS} ${EDITOR_INCS} ${IMGUI_FILES})

add_executable(EngineEditor ${EDITOR_ALL})

target_precompile_headers(EngineEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Private/editor_pch.h"
)

set(EXCLUDE_PCH_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/*.cpp"
)

# 源文件分组
foreach(FILE ${EDITOR_ALL})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK")
set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/Lua")

target_include_directories(EngineEditor
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Private
    PRIVATE ${LUA_DIR}/include # lua 头文件目录
    PRIVATE ${SDK_DIR}/include # sdk 头文件目录
)

target_link_libraries(EngineEditor
    PRIVATE Engine
    PRIVATE glfw
    PRIVATE yaml-cpp
    PRIVATE ${SDK_DIR}/lib/vulkan-1.lib
    PRIVATE ${LUA_DIR}/lib/lua54.lib
)

target_compile_definitions(EngineEditor
    PRIVATE SHOW_LOGS
    PRIVATE "CURRENT_BUILD_CONFIG=\"$<CONFIG>\""
)

if(ENGINE_BUILD_EDITOR)
    set_target_properties(EngineEditor PROPERTIES OUTPUT_NAME "EngineEditor")
    target_compile_definitions(EngineEditor PRIVATE "INVENT_EDITOR=1")
else()
    set_target_properties(EngineEditor PROPERTIES OUTPUT_NAME "${GAME_NAME}")
    target_compile_definitions(EngineEditor PRIVATE "INVENT_EDITOR=0")
endif()

set_target_properties(EngineEditor PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

#sdk 目录
set(SDK_DIR_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/bin")

add_custom_command(TARGET EngineEditor POST_BUILD
    #指令 复制 Engine.dll 到 EngineEditor 目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Engine>
    $<TARGET_FILE_DIR:EngineEditor>
)

add_custom_command(TARGET EngineEditor POST_BUILD
    # copy dll files to EngineEditor directory
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SDK_DIR_BIN}"
    $<TARGET_FILE_DIR:EngineEditor>
)

