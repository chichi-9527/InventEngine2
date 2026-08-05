cmake_minimum_required(VERSION 3.20)
project(GAME_TEMPLATE_NAME)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#通过环境变量获取引擎根目录
set(INVENT_ENGINE_ROOT_DIR "$ENV{INVENT_ENGINE_DIR}")
if(NOT INVENT_ENGINE_ROOT_DIR)
    message(FATAL_ERROR "Environment variable INVENT_ENGINE_DIR not found. Please build engine solution first.")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# 使用pip3安装pyyaml
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pip install pyyaml --user
    RESULT_VARIABLE PIP_RESULT
)

if(NOT PIP_RESULT EQUAL 0)
    message(FATAL_ERROR "pip3 install pyyaml error. Please install it manually.")
endif()

# 注册项目到 projects.yaml
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${INVENT_ENGINE_ROOT_DIR}/Config/Projects/register_project.py" "${PROJECT_NAME}" "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE PYTHON_RESULT
)

if(NOT PYTHON_RESULT EQUAL 0)
    message(FATAL_ERROR "python3 注册项目到 projects.yaml 失败。")
endif()

file(GLOB_RECURSE GAME_INCS "src/*.h")
file(GLOB_RECURSE GAME_SRCS "src/*.cpp")
set(GAME_ALL_FILES ${GAME_SRCS} ${GAME_INCS})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

add_library(${PROJECT_NAME} SHARED ${GAME_SRCS} ${GAME_INCS})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC "/utf-8")
endif()

# 源文件分组
foreach(FILE ${GAME_ALL_FILES})
    get_filename_component(FILE_DIR "${FILE}" DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE_DIR}")
    string(REPLACE "/" "\\" REL_DIR "${REL_DIR}")
    source_group("${REL_DIR}" FILES "${FILE}")
endforeach()

target_include_directories(${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/Public"
    PRIVATE "${INVENT_ENGINE_ROOT_DIR}/Engine/include"
    PRIVATE "${INVENT_ENGINE_ROOT_DIR}/ThirdParty/SDK/Lua/include"
    PRIVATE "${INVENT_ENGINE_ROOT_DIR}/ThirdParty/OneHeaner"
    PRIVATE "${INVENT_ENGINE_ROOT_DIR}/ThirdParty/Other/yaml-cpp/include"
)

add_library(InventEngine::Engine SHARED IMPORTED)
set_target_properties(InventEngine::Engine PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;MINSIZEREL;RELWITHDEBINFO"

    IMPORTED_IMPLIB_DEBUG   "${INVENT_ENGINE_ROOT_DIR}/build/lib/Debug/Engine_d.lib"
    IMPORTED_LOCATION_DEBUG "${INVENT_ENGINE_ROOT_DIR}/build/lib/Debug/Engine_d.dll"

    IMPORTED_IMPLIB_RELEASE   "${INVENT_ENGINE_ROOT_DIR}/build/lib/Release/Engine.lib"
    IMPORTED_LOCATION_RELEASE "${INVENT_ENGINE_ROOT_DIR}/build/lib/Release/Engine.dll"

    IMPORTED_IMPLIB_MINSIZEREL   "${INVENT_ENGINE_ROOT_DIR}/build/lib/MinSizeRel/Engine_ms.lib"
    IMPORTED_LOCATION_MINSIZEREL "${INVENT_ENGINE_ROOT_DIR}/build/lib/MinSizeRel/Engine_ms.dll"

    IMPORTED_IMPLIB_RELWITHDEBINFO   "${INVENT_ENGINE_ROOT_DIR}/build/lib/RelWithDebInfo/Engine_rd.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${INVENT_ENGINE_ROOT_DIR}/build/lib/RelWithDebInfo/Engine_rd.dll"
)

add_library(InventEngine::YamlCpp STATIC IMPORTED)
set_target_properties(InventEngine::YamlCpp PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;MINSIZEREL;RELWITHDEBINFO"

    IMPORTED_LOCATION_DEBUG          "${INVENT_ENGINE_ROOT_DIR}/build/lib/Debug/yaml-cpp_Debug.lib"
    IMPORTED_LOCATION_RELEASE        "${INVENT_ENGINE_ROOT_DIR}/build/lib/Release/yaml-cpp.lib"
    IMPORTED_LOCATION_MINSIZEREL     "${INVENT_ENGINE_ROOT_DIR}/build/lib/Release/yaml-cpp_MinsizeRel.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${INVENT_ENGINE_ROOT_DIR}/build/lib/Release/yaml-cpp_RelWithDebugInfo.lib"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE InventEngine::Engine
    PRIVATE InventEngine::YamlCpp
    PRIVATE "${INVENT_ENGINE_ROOT_DIR}/ThirdParty/SDK/Lua/lib/lua54.lib"
)
