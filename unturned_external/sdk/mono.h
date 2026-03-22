#pragma once

#include "base.h"
#include <memory>
#include <iostream>

constexpr auto GET_ROOT_DOMAIN_OFFSET = 0x751020; 

inline std::unordered_map<uintptr_t, uintptr_t> functions;

inline unsigned short utf8_to_utf16(const char* val)
{
    WCHAR wide[2];
    MultiByteToWideChar(CP_UTF8, 0, val, -1, wide, 2);
    return wide[0];
}

inline std::string read_widechar(const std::uintptr_t address, const std::size_t size)
{
    static std::unordered_map<std::uintptr_t, std::string> string_cache;
    auto it = string_cache.find(address);
    if (it != string_cache.end()) return it->second;

    std::unique_ptr<char[]> buffer(new char[size]);
    read((QWORD)address, buffer.get(), (DWORD)size);
    std::string res(buffer.get());
    string_cache[address] = res;
    return res;
}

struct glist_t
{
    OFFSET(data(), uintptr_t, 0x0)
    OFFSET(next(), uintptr_t, 0x8)
};

struct mono_root_domain_t
{
    OFFSET(domain_assemblies(), glist_t*, 0xA0)
    OFFSET(domain_id(), int, 0x90)
    OFFSET(jitted_function_table(), uintptr_t, 0x120)
};

struct mono_table_info_t
{
    int get_rows()
    {
        return read<int>(reinterpret_cast<uintptr_t>(this) + 0x8) & 0xFFFFFF;
    }
};

struct mono_method_t
{
    std::string name()
    {
        auto name = read_widechar(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x18), 128);
        if (static_cast<std::uint8_t>(name[0]) == 0xEE)
        {
            char name_buff[32];
            sprintf_s(name_buff, 32, XORS("\\u%04X"), utf8_to_utf16(const_cast<char*>(name.c_str())));
            name = name_buff;
        }
        return name;
    }
};

struct mono_class_field_t
{
    OFFSET(offset(), int, 0x18)
    std::string name()
    {
        auto name = read_widechar(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x8), 128);
        if (static_cast<std::uint8_t>(name[0]) == 0xEE)
        {
            char name_buff[32];
            sprintf_s(name_buff, 32, XORS("\\u%04X"), utf8_to_utf16(const_cast<char*>(name.c_str())));
            name = name_buff;
        }
        return name;
    }
};

struct mono_class_runtime_info_t
{
    OFFSET(max_domain(), int, 0x0)
};

struct mono_vtable_t
{
    mono_root_domain_t* domain()
    {
        return read<mono_root_domain_t*>(reinterpret_cast<uintptr_t>(this) + 0x10);
    }

    uintptr_t get_static_field_data()
    {
        auto flags = read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 48);
        if ((flags & 4) != 0)
        {
            auto base_addr = read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x0);
            auto offset_val = read<int>(read<uintptr_t>(base_addr) + 0x5c);
            return read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x48 + 8 * offset_val);
        }
        return 0;
    }
};

struct mono_class_t
{
    OFFSET(num_fields(), int, 0x100)
    OFFSET(runtime_info(), mono_class_runtime_info_t*, 0xD0)
    std::string name()
    {
        return read_widechar(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x48), 128);
    }

    std::string namespace_name()
    {
        return read_widechar(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x50), 128);
    }

    int get_num_methods()
    {
        const auto v2 = read<BYTE>(reinterpret_cast<uintptr_t>(this) + 0x1B) - 1;
        switch (v2)
        {
        case 0:
        case 1:
            return read<int>(reinterpret_cast<uintptr_t>(this) + 0xFC);
        case 3:
        case 5:
            return 0;
        case 4u:
            return read<int>(reinterpret_cast<uintptr_t>(this) + 0xF0);
        default: break;
        }
        return 0;
    }

    mono_method_t* get_method(const int i)
    {
        return reinterpret_cast<mono_method_t*>(read<uintptr_t>(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0xA0) + 0x8 * i));
    }

    mono_class_field_t* get_field(const int i)
    {
        return reinterpret_cast<mono_class_field_t*>(read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x98) + 0x20 * i);
    }

    mono_vtable_t* get_vtable(mono_root_domain_t* domain)
    {
        const auto runtime_info = this->runtime_info();
        if (!runtime_info) return nullptr;
        const auto domain_id = domain->domain_id();
        if (runtime_info->max_domain() < domain_id) return nullptr;
        return reinterpret_cast<mono_vtable_t*>(read<uintptr_t>(reinterpret_cast<uintptr_t>(runtime_info) + 8 * domain_id + 8));
    }

    mono_method_t* find_method(const char* method_name)
    {
        static std::unordered_map<uintptr_t, std::unordered_map<std::string, mono_method_t*>> method_cache;
        auto klass_ptr = reinterpret_cast<uintptr_t>(this);
        auto it_klass = method_cache.find(klass_ptr);
        if (it_klass != method_cache.end()) {
            auto it_meth = it_klass->second.find(method_name);
            if (it_meth != it_klass->second.end()) return it_meth->second;
        }

        auto mono_ptr = uintptr_t();
        for (auto i = 0; i < this->get_num_methods(); i++)
        {
            const auto method = this->get_method(i);
            if (!method) continue;
            if (!strcmp(method->name().c_str(), method_name))
                mono_ptr = reinterpret_cast<uintptr_t>(method);
        }
        auto res = reinterpret_cast<mono_method_t*>(functions[mono_ptr]);
        method_cache[klass_ptr][method_name] = res;
        return res;
    }

    mono_class_field_t* find_field(const char* field_name)
    {
        static std::unordered_map<uintptr_t, std::unordered_map<std::string, mono_class_field_t*>> field_cache;
        auto klass_ptr = reinterpret_cast<uintptr_t>(this);
        auto it_klass = field_cache.find(klass_ptr);
        if (it_klass != field_cache.end()) {
            auto it_f = it_klass->second.find(field_name);
            if (it_f != it_klass->second.end()) return it_f->second;
        }

        for (auto i = 0; i < this->num_fields(); i++)
        {
            const auto field = this->get_field(i);
            if (!field) continue;
            if (!strcmp(field->name().c_str(), field_name)) {
                field_cache[klass_ptr][field_name] = field;
                return field;
            }
        }
        return nullptr;
    }
};

struct mono_hash_table_t
{
    OFFSET(size(), uint32_t, 0x18)
    OFFSET(data(), uintptr_t, 0x20)
    OFFSET(next_value(), void*, 0x108)
    OFFSET(key_extract(), unsigned int, 0x58)
    template<typename T>
    T* lookup(void* key)
    {
        auto v4 = static_cast<mono_hash_table_t*>(read<void*>(data() + 0x8 * (reinterpret_cast<unsigned int>(key) % this->size())));
        if (!v4) return nullptr;
        while (reinterpret_cast<void*>(v4->key_extract()) != key)
        {
            v4 = static_cast<mono_hash_table_t*>(v4->next_value());
            if (!v4) return nullptr;
        }
        return reinterpret_cast<T*>(v4);
    }
};

struct mono_image_t
{
    OFFSET(flags(), int, 0x1C)
    mono_table_info_t* get_table_info(const int table_id)
    {
        if (table_id > 55) return nullptr;
        return reinterpret_cast<mono_table_info_t*>(reinterpret_cast<uintptr_t>(this) + 0x10 * (static_cast<int>(table_id) + 0xE));
    }

    mono_class_t* get(const int type_id)
    {
        if ((this->flags() & 0x20) != 0) return nullptr;
        if ((type_id & 0xFF000000) != 0x2000000) return nullptr;
        return reinterpret_cast<mono_hash_table_t*>(this + 0x4D0)->lookup<mono_class_t>(reinterpret_cast<void*>(type_id));
    }
};

struct mono_assembly_t
{
    OFFSET(mono_image(), mono_image_t*, 0x60)
};

namespace mono
{
    inline mono_root_domain_t* get_root_domain()
    {
        return reinterpret_cast<mono_root_domain_t*>(read<uintptr_t>(MONO_BASEADDRESS + GET_ROOT_DOMAIN_OFFSET));
    }

    inline void init_functions()
    {
        auto root = get_root_domain();
        std::wcout << L"root domain: 0x" << std::hex << (uintptr_t)root << std::endl;
        const auto jitted_table = root->jitted_function_table();
        std::wcout << L"jitted table: 0x" << std::hex << jitted_table << std::endl;
        auto count = read<int>(jitted_table + 0x8);
        std::wcout << L"jitted table count: " << std::dec << count << std::endl;
        for (auto i = 0; i < count; i++)
        {
            const auto entry = read<uintptr_t>(jitted_table + 0x10 + i * 0x8);
            if (!entry) continue;
            auto inner_count = read<int>(entry + 0x4);
            if (i < 3) std::wcout << L"  chunk[" << i << L"] entry=0x" << std::hex << entry << L" inner_count=" << std::dec << inner_count << std::endl;
            for (auto j = 0; j < inner_count; j++)
            {
                const auto function = read<uintptr_t>(entry + 0x18 + j * 0x8);
                if (!function) continue;
                const auto mono_ptr = read<uintptr_t>(function + 0x0);
                const auto jitted_ptr = read<uintptr_t>(function + 0x10);
                functions[mono_ptr] = jitted_ptr;
            }
        }
        std::wcout << L"init_functions done, loaded " << std::dec << functions.size() << L" functions" << std::endl;
    }

    inline mono_assembly_t* domain_assembly_open(mono_root_domain_t* domain, const char* name)
    {
        static std::unordered_map<std::string, mono_assembly_t*> assembly_cache;
        auto it = assembly_cache.find(name);
        if (it != assembly_cache.end()) return it->second;

        auto domain_assemblies = domain->domain_assemblies();
        if (!domain_assemblies) return nullptr;
        while (domain_assemblies)
        {
            auto data = domain_assemblies->data();
            if (data) {
                const auto data_name = read_widechar(read<uintptr_t>(data + 0x10), 128);
                if (!strcmp(data_name.c_str(), name)) {
                    auto res = reinterpret_cast<mono_assembly_t*>(data);
                    assembly_cache[name] = res;
                    return res;
                }
            }
            domain_assemblies = reinterpret_cast<glist_t*>(domain_assemblies->next());
        }
        return nullptr;
    }

    inline mono_class_t* find_class(const char* assembly_name, const char* class_name)
    {
        static std::unordered_map<std::string, mono_class_t*> class_cache;
        std::string cache_key = std::string(assembly_name) + ":" + class_name;
        auto it_cache = class_cache.find(cache_key);
        if (it_cache != class_cache.end()) return it_cache->second;

        const auto root_domain = get_root_domain();
        if (!root_domain) return nullptr;
        const auto domain_assembly = domain_assembly_open(root_domain, assembly_name);
        if (!domain_assembly) return nullptr;
        const auto mono_image = domain_assembly->mono_image();
        if (!mono_image) return nullptr;
        const auto table_info = mono_image->get_table_info(3);
        if (!table_info) return nullptr;
        auto tbl = static_cast<mono_hash_table_t*>(reinterpret_cast<void*>(mono_image + 0x4D0));
        for (int i = 0; i < table_info->get_rows(); i++)
        {
            const auto ptr = tbl->lookup<mono_class_t>(reinterpret_cast<void*>(0x02000000 | i + 1));
            if (!ptr) continue;
            auto name = ptr->name();
            if (!ptr->namespace_name().empty())
                name = ptr->namespace_name().append(".").append(ptr->name());
            if (!strcmp(name.c_str(), class_name)) {
                class_cache[cache_key] = ptr;
                return ptr;
            }
        }
        return nullptr;
    }

    inline uintptr_t get_instance_field_addr(void* obj, mono_class_t* klass, const char* field_name)
    {
        if (!obj || !klass) return 0;
        auto f = klass->find_field(field_name);
        if (!f) return 0;
        return reinterpret_cast<uintptr_t>(obj) + f->offset();
    }

    inline uintptr_t get_static_field_addr(mono_class_t* klass, const char* field_name)
    {
        static std::unordered_map<uintptr_t, std::unordered_map<std::string, uintptr_t>> sf_cache;
        auto klass_ptr = reinterpret_cast<uintptr_t>(klass);
        auto it_klass = sf_cache.find(klass_ptr);
        if (it_klass != sf_cache.end()) {
            auto it_f = it_klass->second.find(field_name);
            if (it_f != it_klass->second.end()) return it_f->second;
        }

        if (!klass) return 0;
        auto f = klass->find_field(field_name);
        if (!f) return 0;
        auto vt = klass->get_vtable(get_root_domain());
        if (!vt) return 0;
        auto base = vt->get_static_field_data();
        if (!base) return 0;
        
        uintptr_t addr = base + f->offset();
        sf_cache[klass_ptr][field_name] = addr;
        return addr;
    }

    template<typename T>
    inline bool write_instance(void* obj, mono_class_t* klass, const char* field_name, const T& v)
    {
        auto addr = get_instance_field_addr(obj, klass, field_name);
        if (!addr) return false;
        SIZE_T written = 0;
        return WriteProcessMemory(process_handle, (LPVOID)addr, &v, sizeof(T), &written) && written == sizeof(T);
    }

    template<typename T>
    inline bool write_static(mono_class_t* klass, const char* field_name, const T& v)
    {
        auto addr = get_static_field_addr(klass, field_name);
        if (!addr) return false;
        SIZE_T written = 0;
        return WriteProcessMemory(process_handle, (LPVOID)addr, &v, sizeof(T), &written) && written == sizeof(T);
    }
}

