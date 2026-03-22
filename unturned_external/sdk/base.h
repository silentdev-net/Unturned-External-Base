#pragma once

#include <string>
#include <windows.h>
#include <unordered_map>
#include <cstdio>
#include <cstdint>

typedef unsigned long long QWORD;

extern HANDLE process_handle;
extern QWORD MONO_BASEADDRESS;

template<typename T> T read(uintptr_t address) {
    T val;
    ReadProcessMemory(process_handle, (LPCVOID)address, &val, sizeof(T), nullptr);
    return val;
}

template<typename T> void write(uintptr_t address, T value) {
    WriteProcessMemory(process_handle, (LPVOID)address, &value, sizeof(T), nullptr);
}

inline void read(uintptr_t address, void* buffer, DWORD size) {
    ReadProcessMemory(process_handle, (LPCVOID)address, buffer, size, nullptr);
}

#define OFFSET(func, type, offset) type func { return read<type>( reinterpret_cast<uintptr_t>( this ) + offset ); }
#define XORS(str) str


constexpr int MONO_STRING_CHARS_OFFSET = 0x14;      
constexpr int MONO_LIST_ITEMS_OFFSET = 0x10;         
constexpr int MONO_LIST_SIZE_OFFSET = 0x18;          
constexpr int MONO_ARRAY_LENGTH_OFFSET = 0x18;       
constexpr int MONO_ARRAY_FIRST_ELEM_OFFSET = 0x20;   


constexpr int UNITY_OBJECT_TO_NATIVE = 0x10;          
constexpr int UNITY_NATIVE_TO_GAMEOBJECT = 0x30;      
constexpr int UNITY_GAMEOBJECT_TO_COMPONENTS = 0x30;   
constexpr int UNITY_COMPONENTS_TO_TRANSFORM = 0x08;    

DWORD get_process_id(const std::wstring& process_name);
uintptr_t get_module_base(DWORD process_id, const std::wstring& module_name);
