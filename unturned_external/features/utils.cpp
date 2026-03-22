#include "utils.h"
#include <algorithm>

namespace features {

bool is_valid_ptr(uintptr_t addr) {
    return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
}

bool is_valid_transform(uintptr_t candidate) {
    if (!is_valid_ptr(candidate)) return false;
    uintptr_t hierarchy = read<uintptr_t>(candidate + 0x38);
    int index = read<int>(candidate + 0x40);
    if (!is_valid_ptr(hierarchy)) return false;
    if (index < 0 || index > 50000) return false;
    uintptr_t lt = read<uintptr_t>(hierarchy + 0x18);
    uintptr_t pi = read<uintptr_t>(hierarchy + 0x20);
    return is_valid_ptr(lt) && is_valid_ptr(pi);
}

uintptr_t find_transform_addr_from_player(uintptr_t playerAddr) {
    uintptr_t nativeComp = read<uintptr_t>(playerAddr + UNITY_OBJECT_TO_NATIVE);
    if (is_valid_ptr(nativeComp)) {
        uintptr_t gameObject = read<uintptr_t>(nativeComp + UNITY_NATIVE_TO_GAMEOBJECT);
        if (is_valid_ptr(gameObject)) {
            uintptr_t compArray = read<uintptr_t>(gameObject + UNITY_GAMEOBJECT_TO_COMPONENTS);
            if (is_valid_ptr(compArray)) {
                uintptr_t transformComp = read<uintptr_t>(compArray + UNITY_COMPONENTS_TO_TRANSFORM);
                if (is_valid_transform(transformComp)) {
                    return transformComp;
                }
            }
        }
    }
    for (int off = 0x0; off <= 0x100; off += 0x8) {
        uintptr_t candidate = read<uintptr_t>(playerAddr + off);
        if (is_valid_transform(candidate)) {
            return candidate;
        }
    }
    for (int off1 = 0x0; off1 <= 0x60; off1 += 0x8) {
        uintptr_t mid = read<uintptr_t>(playerAddr + off1);
        if (!is_valid_ptr(mid)) continue;
        for (int off2 = 0x0; off2 <= 0x80; off2 += 0x8) {
            uintptr_t candidate = read<uintptr_t>(mid + off2);
            if (is_valid_transform(candidate)) {
                return candidate;
            }
        }
    }
    return 0;
}

uintptr_t find_transform_addr(uintptr_t nativeComponentBase) {
    for (int off = 0x0; off <= 0x80; off += 0x8) {
        uintptr_t candidate = read<uintptr_t>(nativeComponentBase + off);
        if (is_valid_transform(candidate)) {
            return candidate;
        }
    }
    return 0;
}

bool is_event_or_delegate(const std::string& fname) {
    std::string lower = fname;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("event") != std::string::npos ||
           lower.find("delegate") != std::string::npos ||
           lower.find("changed") != std::string::npos ||
           lower.find("global") != std::string::npos ||
           lower.find("action") != std::string::npos ||
           lower.find("callback") != std::string::npos;
}

mono_class_field_t* find_field_by_substring(mono_class_t* klass, const std::vector<const char*>& candidates) {
    if (!klass) return nullptr;
    for (int i = 0; i < klass->num_fields(); ++i) {
        auto field = klass->get_field(i);
        if (!field) continue;
        std::string fname = field->name();
        if (is_event_or_delegate(fname)) continue;
        std::string fname_lower = fname;
        std::transform(fname_lower.begin(), fname_lower.end(), fname_lower.begin(), ::tolower);
        for (auto candidate : candidates) {
            std::string cand = candidate;
            std::transform(cand.begin(), cand.end(), cand.begin(), ::tolower);
            if (fname_lower == cand || fname_lower == "_" + cand) {
                return field;
            }
        }
    }
    for (int i = 0; i < klass->num_fields(); ++i) {
        auto field = klass->get_field(i);
        if (!field) continue;
        std::string fname = field->name();
        if (is_event_or_delegate(fname)) continue;
        std::string fname_lower = fname;
        std::transform(fname_lower.begin(), fname_lower.end(), fname_lower.begin(), ::tolower);
        for (auto candidate : candidates) {
            std::string cand = candidate;
            std::transform(cand.begin(), cand.end(), cand.begin(), ::tolower);
            if (fname_lower.find(cand) != std::string::npos) {
                return field;
            }
        }
    }
    return nullptr;
}

}

