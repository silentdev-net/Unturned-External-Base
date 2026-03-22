#include "game_config.h"

namespace features {

void apply_game_config(const GameConfigSettings& settings) {
    if (!g_cheatOffsets.initialized) return;
    uintptr_t configDataAddr = g_cheatOffsets.Provider_modeConfigData;
    void* configData = configDataAddr ? read<void*>(configDataAddr) : nullptr;
    if (!configData) return;

    uintptr_t gameplayFieldAddr = (uintptr_t)configData + g_cheatOffsets.ModeConfigData_Gameplay;
    void* gameplay = gameplayFieldAddr ? read<void*>(gameplayFieldAddr) : nullptr;
    if (!gameplay) return;

    write<bool>((uintptr_t)gameplay + g_cheatOffsets.GameplayConfigData_Compass, settings.compass);
    write<bool>((uintptr_t)gameplay + g_cheatOffsets.GameplayConfigData_Satellite, settings.satellite);
    write<bool>((uintptr_t)gameplay + g_cheatOffsets.GameplayConfigData_Chart, settings.chart);
    
    uint32_t exit_ticks = (uint32_t)(settings.exit_timer * 20.0f);
    write<uint32_t>((uintptr_t)gameplay + g_cheatOffsets.GameplayConfigData_TimerExit, exit_ticks);
}

}

