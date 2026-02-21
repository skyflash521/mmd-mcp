cmake_minimum_required(VERSION 4.1)
project(mmd_mcp)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.7
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(httplib json)

if(DEFINED ENV{DXSDK_DIR})
    set(DXSDK_DIR "$ENV{DXSDK_DIR}")
else()
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")
endif()

add_library(mmd_mcp SHARED main.cpp mcp_server.cpp)

option(MMD_MCP_DEBUG "Enable debug tools (dump_camera_region etc.)" OFF)

target_compile_definitions(mmd_mcp PRIVATE
    WIN32_LEAN_AND_MEAN
    _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
    _HAS_DEPRECATED_RESULT_OF=1
    _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING
    $<$<BOOL:${MMD_MCP_DEBUG}>:MMD_MCP_DEBUG>
)

target_include_directories(mmd_mcp PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${DXSDK_DIR}/Include"
)

target_link_directories(mmd_mcp PRIVATE "${PROJECT_SOURCE_DIR}/lib")

target_link_libraries(mmd_mcp PRIVATE
    MMDPlugin/MMDPlugin
    "${DXSDK_DIR}/Lib/x64/d3dx9.lib"
    "${DXSDK_DIR}/Lib/x64/d3d9.lib"
    httplib
    nlohmann_json::nlohmann_json
)

add_executable(test_mcp tests/test_mcp.cpp mcp_server.cpp)
target_compile_definitions(test_mcp PRIVATE WIN32_LEAN_AND_MEAN MMD_MCP_TEST)
target_include_directories(test_mcp PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(test_mcp PRIVATE httplib nlohmann_json::nlohmann_json)

add_executable(test_frame_tools tests/test_frame_tools.cpp)
target_compile_definitions(test_frame_tools PRIVATE WIN32_LEAN_AND_MEAN MMD_MCP_TEST)
target_include_directories(test_frame_tools PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(test_frame_tools PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_camera_tools tests/test_camera_tools.cpp)
target_compile_definitions(test_camera_tools PRIVATE WIN32_LEAN_AND_MEAN MMD_MCP_TEST)
target_include_directories(test_camera_tools PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(test_camera_tools PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_integration tests/test_integration.cpp)
target_compile_definitions(test_integration PRIVATE WIN32_LEAN_AND_MEAN)
target_include_directories(test_integration PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(test_integration PRIVATE httplib nlohmann_json::nlohmann_json)

add_executable(test_common tests/test_common.cpp)
target_compile_definitions(test_common PRIVATE WIN32_LEAN_AND_MEAN MMD_MCP_TEST)
target_include_directories(test_common PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(test_common PRIVATE nlohmann_json::nlohmann_json)

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
