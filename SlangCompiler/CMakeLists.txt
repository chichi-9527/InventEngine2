
file(GLOB_RECURSE SLANG_SRCS "src/*.cpp")
file(GLOB_RECURSE SLANG_INCS "include/*.h" "include/*.hpp")

set(SLANG_ALL_FILES ${SLANG_SRCS} ${SLANG_INCS})

add_library(SlangCompiler STATIC ${SLANG_ALL_FILES})

if (MSVC)
    target_compile_options(SlangCompiler PRIVATE "/utf-8")
endif()

# 源文件分组
foreach(FILE ${SLANG_ALL_FILES})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

target_compile_definitions(SlangCompiler 
    PRIVATE SHOW_LOGS
)

target_include_directories(SlangCompiler 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${GLOBAL_VULKAN_SDK_INC} # vulkan sdk 头文件目录(slang and vulkan headers)
)

target_link_libraries(SlangCompiler 
    PRIVATE Engine
    PRIVATE ${GLOBAL_VULKAN_SDK_LIB}/slang.lib
    PRIVATE ${GLOBAL_VULKAN_SDK_LIB}/slang-compiler.lib
)
