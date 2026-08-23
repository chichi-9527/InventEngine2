
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

set(EXCLUDE_PCH_FILES_EDITOR
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_impl_vulkan.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8/imgui.cpp"
)

set_source_files_properties(${EXCLUDE_PCH_FILES_EDITOR}
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
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
set(ENGINE_EDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Engine/EditorInclude")

target_include_directories(EngineEditor
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Private
    PRIVATE ${LUA_DIR}/include # lua 头文件目录
    PRIVATE ${GLOBAL_VULKAN_SDK_INC} # vulkan sdk 头文件目录(slang and vulkan headers)
    PRIVATE ${ENGINE_EDITOR_INCLUDE_DIR} # engine editor 头文件目录
)

target_link_libraries(EngineEditor
    PRIVATE Engine
    PRIVATE SlangCompiler
    PRIVATE glfw
    PRIVATE yaml-cpp
    PRIVATE ${GLOBAL_VULKAN_SDK_LIB}/vulkan-1.lib
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

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang.dll"
    "$<TARGET_FILE_DIR:EngineEditor>/slang.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slangd.dll"
    "$<TARGET_FILE_DIR:EngineEditor>/slangd.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang-compiler.dll"
    "$<TARGET_FILE_DIR:EngineEditor>/slang-compiler.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLOBAL_VULKAN_SDK_BIN}/slang-glslang.dll"
    "$<TARGET_FILE_DIR:EngineEditor>/slang-glslang.dll"

    # copy dll files to EngineEditor directory
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SDK_DIR_BIN}"
    $<TARGET_FILE_DIR:EngineEditor>
)

if(ENGINE_BUILD_EDITOR)
    add_custom_command(TARGET EngineEditor POST_BUILD
        
        # copy config files to EngineEditor directory
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../Config"
        "$<TARGET_FILE_DIR:EngineEditor>/Config"
    )
else()
    add_custom_command(TARGET EngineEditor POST_BUILD
        

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../Config/GameConfig"
        "$<TARGET_FILE_DIR:EngineEditor>/Config/GameConfig"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../Config/Shaders"
        "$<TARGET_FILE_DIR:EngineEditor>/Config/Shaders"
    )
endif()
