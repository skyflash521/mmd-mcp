#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <type_traits>

#include "mmd_plugin.h"
#include "mcp_server.h"

class MyPlugin : public MMDPluginDLL4 {
public:
    const char* getPluginTitle() const override {
        return "MMD MCP";
    }

    void start() override {
        server_.start();
    }

    void stop() override {
        server_.stop();
    }

private:
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
