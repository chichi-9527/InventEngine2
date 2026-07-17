

file(GLOB_RECURSE ENGINE_SRCS "src/*.cpp")
file(GLOB_RECURSE ENGINE_INCS 
        "include/Public/*.h" 
        "include/Public/*.hpp"
        "include/Public/*.ipp"
        "include/Private/*.h"
        "include/Private/*.hpp"
        "include/Private/*.ipp"
)

set(ENGINE_ALL_FILES ${ENGINE_SRCS} ${ENGINE_INCS})

add_library(Engine SHARED ${ENGINE_ALL_FILES})

# 源文件分组
foreach(FILE ${ENGINE_ALL_FILES})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

target_compile_definitions(Engine 
    PRIVATE EXPORTING_ENGINE
)

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK")
set(ONE_HEANER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/OneHeader")

target_include_directories(Engine 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/Public  # 公共头文件目录
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/Private # 私有头文件目录
    PRIVATE ${SDK_DIR}/include # sdk 头文件目录
    PRIVATE ${ONE_HEANER_DIR} # 一头文件目录
)

target_link_libraries(Engine 
    PRIVATE SlangCompiler
    PRIVATE assimp
    PRIVATE freetype
    PRIVATE glfw
    PRIVATE ${SDK_DIR}/lib/vulkan-1.lib
)
