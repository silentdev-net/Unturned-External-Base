#include "admin.h"

namespace features {

void apply_admin(void* localPlayer, bool enabled) {
    if (!g_cheatOffsets.initialized) return;
    if (!localPlayer) return;

    uintptr_t channelAddr = (uintptr_t)localPlayer + g_cheatOffsets.Player_channel;
    void* channel = channelAddr ? read<void*>(channelAddr) : nullptr;
    if (!channel) return;

    uintptr_t ownerAddr = (uintptr_t)channel + g_cheatOffsets.SteamChannel_owner;
    void* owner = ownerAddr ? read<void*>(ownerAddr) : nullptr;
    if (!owner) return;
    
    write<bool>((uintptr_t)owner + g_cheatOffsets.SteamPlayer_isAdmin, enabled);
}

}

