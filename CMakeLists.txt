cmake_minimum_required(VERSION 4.1)
project(mmd_mcp)

set(CMAKE_CXX_STANDARD 20)

if(DEFINED ENV{DXSDK_DIR})
    set(DXSDK_DIR "$ENV{DXSDK_DIR}")
else()
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")
endif()

add_library(mmd_mcp SHARED main.cpp)

target_compile_definitions(mmd_mcp PRIVATE
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
    _HAS_DEPRECATED_RESULT_OF=1
    _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING
)

target_include_directories(mmd_mcp PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${DXSDK_DIR}/Include"
)

target_link_directories(mmd_mcp PRIVATE "${PROJECT_SOURCE_DIR}/lib")

target_link_libraries(mmd_mcp PRIVATE
    MMDPlugin/MMDPlugin
    "${DXSDK_DIR}/Lib/x64/d3dx9.lib"
    "${DXSDK_DIR}/Lib/x64/d3d9.lib"
)

set(MMD_PLUGIN_DIR "" CACHE PATH "MMDプラグインのデプロイ先ディレクトリ")

if(MMD_PLUGIN_DIR)
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:mmd_mcp>"
            "${MMD_PLUGIN_DIR}/mmd_mcp.dll"
        DEPENDS mmd_mcp
        COMMENT "mmd_mcp.dll を ${MMD_PLUGIN_DIR} にデプロイ"
    )
endif()
