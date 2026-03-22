#pragma once
#include "../sdk/mono.h"
#include "esp.h"



namespace features {

    inline void apply_no_spread(void* localPlayer, bool enabled) {
        if (!g_cheatOffsets.initialized) return;
        if (!enabled) return;

        if (!localPlayer) return;

        uintptr_t equipAddr = (uintptr_t)localPlayer + g_cheatOffsets.Player_equipment;
        void* equipment = equipAddr ? read<void*>(equipAddr) : nullptr;
        if (!equipment) return;

        uintptr_t assetAddr = (uintptr_t)equipment + g_cheatOffsets.PlayerEquipment_asset;
        void* asset = assetAddr ? read<void*>(assetAddr) : nullptr;
        if (!asset) return;

        write<float>((uintptr_t)asset + g_cheatOffsets.ItemGunAsset_baseSpreadAngleRadians, 0.0f);
    }

}

