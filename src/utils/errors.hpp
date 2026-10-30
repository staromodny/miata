#pragma once
#include <windows.h>

#include <format>
#include <string>

#include "message_box.hpp"

[[nodiscard]]
inline auto format_code(const DWORD& error_code, const std::wstring& custom_message = L"") -> std::wstring
{
    LPWSTR message_buffer = nullptr;

    const auto size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&message_buffer), 0, nullptr
    );

    const auto windows_message = size ? std::wstring(message_buffer, size) : L"Unknown";
    LocalFree(message_buffer);

    return std::format(L"{}\n\n0x{:X} - {}", custom_message, error_code, windows_message);
}

[[nodiscard]]
inline int show_last_error(const std::wstring& context)
{
    const auto full_message = format_code(GetLastError(), context.c_str());
    return *show_message_box<wchar_t>(full_message,L"",buttons::OK | icons::Error);
}