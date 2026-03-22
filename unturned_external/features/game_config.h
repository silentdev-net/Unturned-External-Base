#pragma once
#include "../sdk/mono.h"
#include "esp.h"
#include <iostream>

namespace features {

struct GameConfigSettings {
    bool compass = false;
    bool satellite = false;
    bool chart = false;
    float exit_timer = 10.0f; 
};

void apply_game_config(const GameConfigSettings& settings);
}

