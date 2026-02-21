#pragma once

#include "tool.h"
#include "mmd_plugin.h"
#include <cstdio>
#include <vector>

// Shift-JIS (ANSI) → UTF-8 変換
static std::string ansiToUtf8(const char* ansi) {
    if (!ansi || !ansi[0]) return "";
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
    if (wlen <= 0) return "";
    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, wbuf.data(), wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return "";
    std::vector<char> ubuf(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, ubuf.data(), ulen, nullptr, nullptr);
    return std::string(ubuf.data());
}

// MMDメインウィンドウの子コントロールを列挙する調査ツール。
class EnumerateControlsTool : public ITool {
public:
    std::string name() const override { return "enumerate_controls"; }
    std::string description() const override {
        return "List all child controls of MMD main window with class, text, ID, and position";
    }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"filter_class", {{"type", "string"}, {"description", "Optional: filter by window class name (e.g. 'Button')"}}},
                {"filter_text", {{"type", "string"}, {"description", "Optional: filter by window text substring"}}}
            }}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;

        HWND hwnd = ::getHWND();
        if (!hwnd) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "MMD window not available"}}})},
                {"isError", true}
            };
        }

        std::string filterClass = args.value("filter_class", "");
        std::string filterText = args.value("filter_text", "");

        struct EnumData {
            json controls;
            std::string filterClass;
            std::string filterText;
        } data;
        data.controls = json::array();
        data.filterClass = filterClass;
        data.filterText = filterText;

        EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
            auto* d = reinterpret_cast<EnumData*>(lParam);

            char classNameRaw[256] = {};
            GetClassNameA(child, classNameRaw, sizeof(classNameRaw));
            std::string className = ansiToUtf8(classNameRaw);

            char windowTextRaw[256] = {};
            GetWindowTextA(child, windowTextRaw, sizeof(windowTextRaw));
            std::string windowText = ansiToUtf8(windowTextRaw);

            // フィルタ適用
            if (!d->filterClass.empty()) {
                if (className.find(d->filterClass) == std::string::npos)
                    return TRUE;
            }
            if (!d->filterText.empty()) {
                if (windowText.find(d->filterText) == std::string::npos)
                    return TRUE;
            }

            RECT rect = {};
            GetWindowRect(child, &rect);

            int controlId = GetDlgCtrlID(child);

            json ctrl = {
                {"hwnd", reinterpret_cast<uintptr_t>(child)},
                {"class", className.c_str()},
                {"text", windowText.c_str()},
                {"control_id", controlId},
                {"control_id_hex", "0x" + ([](int id) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%X", id);
                    return std::string(buf);
                })(controlId)},
                {"rect", {
                    {"left", rect.left},
                    {"top", rect.top},
                    {"right", rect.right},
                    {"bottom", rect.bottom}
                }},
                {"visible", (GetWindowLong(child, GWL_STYLE) & WS_VISIBLE) != 0}
            };
            d->controls.push_back(ctrl);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&data));

        json result = {
            {"parent_hwnd", reinterpret_cast<uintptr_t>(hwnd)},
            {"count", data.controls.size()},
            {"controls", data.controls}
        };

        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump(2)}}})},
            {"isError", false}
        };
    }
};
