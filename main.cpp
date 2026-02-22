#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <type_traits>

#include "mmd_plugin.h"
#include "mcp_server.h"
#include "tools/ping_tool.h"
#include "tools/timeline/timeline_accessor.h"
#include "tools/timeline/timeline_tool.h"
#include "tools/camera/camera_accessor.h"
#include "tools/camera/camera_tool.h"
#ifdef MMD_MCP_DEBUG
#include "tools/debug/dump_camera_region_tool.h"
#include "tools/debug/enumerate_controls_tool.h"
#endif

class MyPlugin : public MMDPluginDLL4 {
public:
    const char* getPluginTitle() const override {
        return "MMD MCP";
    }

    void start() override {
        server_.registerTool(std::make_unique<PingTool>());
        server_.registerTool(std::make_unique<GetCurrentFrameTool>(&timeline_accessor_));
        server_.registerTool(std::make_unique<SetCurrentFrameTool>(&timeline_accessor_));
        server_.registerTool(std::make_unique<GetCameraKeyframesTool>(&camera_accessor_));
        server_.registerTool(std::make_unique<CreateCameraKeyframesTool>(&camera_accessor_));
        server_.registerTool(std::make_unique<UpdateCameraKeyframesTool>(&camera_accessor_));
        server_.registerTool(std::make_unique<DeleteCameraKeyframesTool>(&camera_accessor_));
#ifdef MMD_MCP_DEBUG
        server_.registerTool(std::make_unique<DumpCameraRegionTool>());
        server_.registerTool(std::make_unique<EnumerateControlsTool>());
#endif
        server_.start();
    }

    void stop() override {
        server_.stop();
    }

private:
    MmdTimelineAccessor timeline_accessor_;
    MmdCameraAccessor camera_accessor_;
    McpServer server_;
};

static MyPlugin g_plugin;

extern "C" MMD_PLUGIN_API int version() {
    return 4;
}

extern "C" MMD_PLUGIN_API MMDPluginDLL4* create4(IDirect3DDevice9* device) {
    g_plugin.start();
    return &g_plugin;
}

extern "C" MMD_PLUGIN_API void destroy4(MMDPluginDLL4* p) {
    g_plugin.stop();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("mmd_mcp: DLL Attached\n");
    }
    return TRUE;
}
