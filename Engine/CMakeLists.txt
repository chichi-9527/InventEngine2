

file(GLOB_RECURSE ENGINE_SRCS "src/*.cpp")
file(GLOB_RECURSE ENGINE_INCS 
        "include/*.h" 
        "include/*.hpp"
        "include/*.ipp"
        "Private/*.h"
        "Private/*.hpp"
        "Private/*.ipp"
)

set(ENGINE_ALL_FILES ${ENGINE_SRCS} ${ENGINE_INCS})

add_library(Engine SHARED ${ENGINE_ALL_FILES}) 

if (MSVC)
    target_compile_options(Engine PUBLIC "/utf-8")
endif()

# 预编译头
target_precompile_headers(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/engine_pch.h"
)

set(EXCLUDE_PCH_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ILog.cpp"
)

set_source_files_properties(${EXCLUDE_PCH_FILES}
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# 源文件分组
foreach(FILE ${ENGINE_ALL_FILES})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

target_compile_definitions(Engine 
    PRIVATE EXPORTING_ENGINE
    PRIVATE SHOW_LOGS
)
if(ENGINE_DEVELOPMENT_MODE)
        target_compile_definitions(Engine PRIVATE ENGINE_DEVELOPMENT_MODE)
endif()

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK")
set(LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/SDK/Lua")
set(ONE_HEANER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/OneHeader")
set(BULLET3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/Other/bullet3/src")
set(YAML_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/Other/yaml-cpp")

target_include_directories(Engine 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include  # 公共头文件目录
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private # 私有头文件目录
    PRIVATE ${SDK_DIR}/include # sdk 头文件目录
    PRIVATE ${LUA_DIR}/include # lua 头文件目录
    PRIVATE ${ONE_HEANER_DIR} # 一头文件目录
    PRIVATE ${BULLET3_SRC_DIR} # bullet3 源文件目录
    PRIVATE ${YAML_CPP_DIR}/include # yaml-cpp 头文件目录
)

target_link_libraries(Engine 
    PRIVATE SlangCompiler
    PRIVATE assimp
    PRIVATE freetype
    PRIVATE yaml-cpp
    PRIVATE ${SDK_DIR}/lib/vulkan-1.lib
    PRIVATE ${LUA_DIR}/lib/lua54.lib

    # bullet3
    PRIVATE BulletDynamics
    PRIVATE BulletCollision
    PRIVATE LinearMath
)
