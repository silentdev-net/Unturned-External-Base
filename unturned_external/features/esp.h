#pragma once
#include <windows.h>
#include <iostream>
#include <cmath>
#include <cfloat>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <thread>
#include "../sdk/mono.h"
#include "../sdk/transform.h"

#ifndef MONO_STRING_LENGTH_OFFSET
#define MONO_STRING_LENGTH_OFFSET 0x10
#endif

#include "../imgui/imgui.h"

struct UnityMatrix {
    struct Vec4 { float x, y, z, w; } right, up, forward, translation;
};

struct Vector2 {
    float x, y;
};

struct SkeletonBones {
    vector3 Skeleton, Left_Hip, Left_Leg, Left_Foot, Right_Hip, Right_Leg, Right_Foot,
        Spine, Skull, Spot, Left_Shoulder, Left_Arm, Left_Hand, Left_Hook,
        Right_Shoulder, Right_Arm, Right_Hand, Right_Hook;
};


struct CachedPlayer {
    vector3 pos;
    std::string baseName;
    std::string equipmentName;
    int reputation;
    bool hasSkeleton;
    SkeletonBones bones;
    void* playerPtr; 
};

struct CachedZombie {
    vector3 pos;
    std::string typeName;
    bool hasSkeleton;
    SkeletonBones bones;
};

struct CachedVehicle {
    vector3 pos;
    std::string name;
};

struct CachedBarricade {
    vector3 pos;
    std::string name;
};

struct CachedItem {
    vector3 pos;
    std::string name;
    uint16_t id;
    uint8_t type;
    uint16_t rarity;
    uint8_t amount;
    uint8_t quality;
};

struct DangerEntity {
    std::string name;
    int reputation;
    float distance;
    bool is_zombie;
};

namespace features {
    struct CheatOffsets {
        bool initialized = false;
        int Player_skills = 0, Player_equipment = 0, Player_channel = 0;
        int PlayerSkills_reputation = 0, PlayerEquipment_asset = 0;
        int SteamPlayer_player = 0, SteamPlayer_playerID = 0, SteamPlayer_model = 0, SteamPlayer_isAdmin = 0;
        int SteamPlayerID_characterName = 0, SteamChannel_owner = 0;
        uintptr_t Provider_clients = 0, Provider_modeConfigData = 0;
        int ModeConfigData_Gameplay = 0, GameplayConfigData_Compass = 0, GameplayConfigData_Satellite = 0;
        int GameplayConfigData_Chart = 0, GameplayConfigData_TimerExit = 0;
        uintptr_t ZombieManager_regions = 0;
        int ZombieRegion_zombies = 0, Zombie_isDead = 0, Zombie_isVisible = 0, Zombie_speciality = 0;
        uintptr_t VehicleManager_vehicles = 0;
        int InteractableVehicle_asset = 0, VehicleAsset_vehicleName = 0;
        uintptr_t BarricadeManager_regions = 0;
        int BarricadeRegion_drops = 0, BarricadeDrop_model = 0, BarricadeDrop_asset = 0;
        uintptr_t ItemManager_regions = 0;
        int ItemRegion_drops = 0, ItemDrop_model = 0, ItemDrop_interactableItem = 0;
        int InteractableItem_item = 0, InteractableItem_asset = 0;
        int ItemAsset_itemName = 0, ItemAsset_id = 0, ItemAsset_type = 0, ItemAsset_rarity = 0;
        int ItemGunAsset_recoilMin_x = 0, ItemGunAsset_recoilMax_x = 0, ItemGunAsset_recoilMin_y = 0;
        int ItemGunAsset_recoilMax_y = 0, ItemGunAsset_baseSpreadAngleRadians = 0;
        int Item_amount = 0, Item_quality = 0;
    };

    extern CheatOffsets g_cheatOffsets;

    extern UnityMatrix g_viewMatrix;
    extern vector3 g_camPosition;
    
    
    extern std::mutex cache_mutex;
    extern std::vector<CachedPlayer> cached_players;
    extern std::vector<CachedZombie> cached_zombies;
    extern std::vector<CachedVehicle> cached_vehicles;
    extern std::vector<CachedBarricade> cached_barricades;
    extern std::vector<CachedItem> cached_items;
    extern std::vector<DangerEntity> close_entities;
    extern bool g_esp_thread_running;

    
    uintptr_t get_transform_from_component(uintptr_t component);
    bool world_to_screen(const vector3& worldPos, Vector2& screenPos, float screenW, float screenH);
    uintptr_t find_skeleton_recursive(uintptr_t transformAddr, int maxDepth = 3, int currentDepth = 0);
    bool get_skeleton(uintptr_t skeletonTransformAddr, SkeletonBones& bones);
    void draw_skeleton_lines(ImDrawList* draw_list, const SkeletonBones& bones, ImU32 color, float screenW, float screenH, float thickness = 1.0f);
    bool get_game_window_size(float& width, float& height);
    bool update_view_matrix(void* localPlayer);
    void draw_esp_box(ImDrawList* draw_list, const vector3& feetPos, float entityHeight, ImU32 boxColor, float screenW, float screenH);
    void draw_skeleton_box(ImDrawList* draw_list, const SkeletonBones& bones, ImU32 boxColor, float screenW, float screenH);
    std::string read_unity_string(uintptr_t strPtr);
    
    
    void init_esp_offsets();
    void cache_updater_thread(); 
    void render_esp(void* localPlayer, bool draw_local, bool draw_players, bool draw_zombies, bool draw_vehicles, bool draw_barricades, bool draw_items, bool draw_player_skeletons, bool draw_zombie_skeletons, float player_dist, float zombie_dist, float veh_dist, float bar_dist, float item_dist, bool draw_danger_radius);
}

