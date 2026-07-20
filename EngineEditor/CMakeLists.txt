
file(GLOB_RECURSE EDITOR_SRCS "src/*.cpp")
file(GLOB_RECURSE EDITOR_INCS "src/*.h")

set(EDITOR_ALL ${EDITOR_SRCS} ${EDITOR_INCS})

add_executable(EngineEditor ${EDITOR_ALL})

# 源文件分组
foreach(FILE ${EDITOR_ALL})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

target_include_directories(EngineEditor
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Imgui_1_92_8
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_link_libraries(EngineEditor
    PRIVATE Engine
    PRIVATE yaml-cpp
)

target_compile_definitions(EngineEditor
    PRIVATE SHOW_LOGS
    PRIVATE "INVENT_EDITOR=1"
    PRIVATE "CURRENT_BUILD_CONFIG=\"$<CONFIG>\""
)

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

