#pragma once
#include "../sdk/mono.h"
#include "../sdk/transform.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <string>

namespace features {

template <typename T>
class UnityArray
{
public:
	uintptr_t address;
	inline UnityArray(uintptr_t addr) : address(addr) {}

	inline uint32_t Count()
	{
		return read<uint32_t>(address + MONO_ARRAY_LENGTH_OFFSET);
	}

	inline T Get(uint32_t index)
	{
		return read<T>(read<uintptr_t>(address + MONO_LIST_ITEMS_OFFSET) + MONO_ARRAY_FIRST_ELEM_OFFSET + (index * sizeof(T)));
	}
};

void dump_players_to_console();

}

