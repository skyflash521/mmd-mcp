#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <type_traits>

#include "mmd_plugin.h"

class MyPlugin : public MMDPluginDLL4 {
public:
    const char* getPluginTitle() const override {
        return "My MMD Plugin";
    }

    void start() override {
        MessageBoxA(NULL, "Plugin Start", "Debug", MB_OK);
    }

    void stop() override {
    }
};

static MyPlugin g_plugin;

extern "C" MMD_PLUGIN_API MMDPluginDLL4* create4(IDirect3DDevice9* device) {
    return &g_plugin;
}

extern "C" MMD_PLUGIN_API void destroy4(MMDPluginDLL4* p) {
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("mmd_mcp: DLL Attached\n");
    }
    return TRUE;
}
