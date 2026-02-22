#pragma once

// Windowsでassert失敗時のダイアログを抑制し、stderrに出力する
#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>

inline void suppressWindowsDialogs() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}
#else
inline void suppressWindowsDialogs() {}
#endif
