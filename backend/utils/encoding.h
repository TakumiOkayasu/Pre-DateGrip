#pragma once

#include <Windows.h>

#include <climits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace velocitydb {

// Convert UTF-8 (std::string) to UTF-16 (std::wstring)
inline std::wstring utf8ToWide(std::string_view str) {
    if (str.empty()) {
        return {};
    }

    if (str.size() > static_cast<size_t>(INT_MAX)) [[unlikely]] {
        throw std::length_error("utf8ToWide: input exceeds INT_MAX");
    }

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (wideLen == 0) {
        return {};
    }

    auto wideStr = std::wstring(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wideStr.data(), wideLen);
    return wideStr;
}

// Convert UTF-16 (wchar_t) to UTF-8 (std::string)
inline std::string wideToUtf8(const wchar_t* wstr, size_t len) {
    if (len == 0 || wstr == nullptr) {
        return {};
    }

    if (len > static_cast<size_t>(INT_MAX)) [[unlikely]] {
        throw std::length_error("wideToUtf8: input exceeds INT_MAX");
    }

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (utf8Len == 0) {
        return {};
    }

    auto utf8Str = std::string(static_cast<size_t>(utf8Len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(len), utf8Str.data(), utf8Len, nullptr, nullptr);
    return utf8Str;
}

}  // namespace velocitydb
