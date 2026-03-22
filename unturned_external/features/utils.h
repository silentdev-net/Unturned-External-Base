#pragma once
#include "../sdk/mono.h"
#include "../sdk/transform.h"
#include <string>
#include <vector>

namespace features {

bool is_valid_ptr(uintptr_t addr);
bool is_valid_transform(uintptr_t candidate);
uintptr_t find_transform_addr_from_player(uintptr_t playerAddr);
uintptr_t find_transform_addr(uintptr_t nativeComponentBase);

bool is_event_or_delegate(const std::string& fname);
mono_class_field_t* find_field_by_substring(mono_class_t* klass, const std::vector<const char*>& candidates);

}

