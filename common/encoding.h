#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>

// バイト列が有効なUTF-8かどうかを判定する
inline bool isValidUtf8(const char* s) {
    if (!s) return true;
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        if (*p < 0x80) {
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            if ((p[1] & 0xC0) != 0x80) return false;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false;
            p += 4;
        } else {
            return false;
        }
    }
    return true;
}

// Shift-JIS (code page 932) to UTF-8
inline std::string sjisToUtf8(const char* sjis) {
    if (!sjis || sjis[0] == '\0') return {};

    int wlen = MultiByteToWideChar(932, 0, sjis, -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wide(wlen - 1, L'\0');
    MultiByteToWideChar(932, 0, sjis, -1, wide.data(), wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                                    nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return {};
    std::string utf8(ulen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1,
                        utf8.data(), ulen, nullptr, nullptr);
    return utf8;
}

// MMD文字列をUTF-8に変換する
// PMX: UTF-8で格納 → そのまま返す
// PMD: Shift-JISで格納 → 変換する
inline std::string mmdToUtf8(const char* s) {
    if (!s || s[0] == '\0') return {};
    if (isValidUtf8(s)) return std::string(s);
    return sjisToUtf8(s);
}

#else
inline bool isValidUtf8(const char*) { return true; }
inline std::string sjisToUtf8(const char* s) { return s ? std::string(s) : std::string{}; }
inline std::string mmdToUtf8(const char* s) { return s ? std::string(s) : std::string{}; }
#endif
