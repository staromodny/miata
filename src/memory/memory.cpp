#include "memory.hpp"

#include <TlHelp32.h>
#include <thread>

std::optional<DWORD64> get_module_by_name(DWORD pid, const std::string& target_module) {
    const unique_handle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid) };
    if (!snapshot) {
        return std::nullopt;
    }

    MODULEENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    // First module
    if (!Module32First(snapshot.get(), &entry)) {
        return std::nullopt;
    }

    do {
        if (strcmp(target_module.c_str(), entry.szModule) == 0) {
            return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
        }
    } while (Module32Next(snapshot.get(), &entry));

    return std::nullopt;
}

std::optional<unique_handle> open_process(DWORD pid, DWORD access_flags) {
    const auto handle = OpenProcess(access_flags, FALSE, pid); //If the function fails, the return value is NULL.
    if (handle == nullptr) {
        return std::nullopt;
    }

    return unique_handle{ handle };
}

bool c_game::initialize() {
    static constexpr auto TARGET_NAME = "cs2.exe";
    const unique_handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot)
        return false;

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    //Enumerate running processes
    for (Process32First(snapshot.get(), &entry); Process32Next(snapshot.get(), &entry); ) {
        if (strcmp(TARGET_NAME, entry.szExeFile) == 0) {
            m_process_id = entry.th32ProcessID;
            m_process_handle = *open_process(*m_process_id, PROCESS_VM_READ);
            break;
        }
    }

    if (!m_process_id)
        return false;

    while (m_client_base == 0) {
        m_client_base = get_module_by_name(*m_process_id, "client.dll");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return m_client_base != 0 && m_process_id != 0 && m_process_handle != nullptr;
}