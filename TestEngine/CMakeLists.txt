
file(GLOB_RECURSE TEST_SRCS "src/Private/*.cpp")
file(GLOB_RECURSE TEST_INCS "src/Public/*.h")

set(TEST_ALL_FILES ${TEST_SRCS} ${TEST_INCS})

add_executable(TestEngine 
    ${TEST_ALL_FILES}
)

# 源文件分组
foreach(FILE ${TEST_ALL_FILES})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

set_target_properties(TestEngine PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_include_directories(TestEngine 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Public
)

target_link_libraries(TestEngine 
    PRIVATE Engine
)

#sdk 目录
set(SDK_DIR_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/bin")

add_custom_command(TARGET TestEngine POST_BUILD
    #指令 复制 Engine.dll 到 Test 目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Engine>
    $<TARGET_FILE_DIR:TestEngine>
)

add_custom_command(TARGET TestEngine POST_BUILD
    # copy dll files to Test directory
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SDK_DIR_BIN}"
    $<TARGET_FILE_DIR:TestEngine>
)