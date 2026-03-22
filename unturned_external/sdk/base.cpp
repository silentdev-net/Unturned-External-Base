#include "base.h"
#include <tlhelp32.h>
#include <cwchar>

HANDLE process_handle;
QWORD MONO_BASEADDRESS;

DWORD get_process_id(const std::wstring& process_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry)) {
        do {
            if (wcscmp(process_name.c_str(), entry.szExeFile) == 0) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

uintptr_t get_module_base(DWORD process_id, const std::wstring& module_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);
    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);
    if (Module32First(snapshot, &entry)) {
        do {
            if (wcscmp(module_name.c_str(), entry.szModule) == 0) {
                CloseHandle(snapshot);
                return (uintptr_t)entry.modBaseAddr;
            }
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}
