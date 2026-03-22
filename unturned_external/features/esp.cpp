#include "esp.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "players.h"
#include "utils.h"
#include "../imgui/imgui.h"

namespace features {

    std::mutex cache_mutex;
    std::vector<CachedPlayer> cached_players;
    std::vector<CachedZombie> cached_zombies;
    std::vector<CachedVehicle> cached_vehicles;
    std::vector<CachedBarricade> cached_barricades;
    std::vector<CachedItem> cached_items;
    std::vector<DangerEntity> close_entities;
    bool g_esp_thread_running = false;

    CheatOffsets g_cheatOffsets;

    UnityMatrix g_viewMatrix;
    vector3 g_camPosition;

    uintptr_t get_transform_from_component(uintptr_t component) {
        if (!component) return 0;
        uintptr_t native_component = read<uintptr_t>(component + UNITY_OBJECT_TO_NATIVE);
        if (!native_component) return 0;
        uintptr_t game_object = read<uintptr_t>(native_component + UNITY_NATIVE_TO_GAMEOBJECT);
        if (!game_object) return 0;
        uintptr_t component_list = read<uintptr_t>(game_object + UNITY_GAMEOBJECT_TO_COMPONENTS);
        if (!component_list) return 0;
        uintptr_t transform = read<uintptr_t>(component_list + UNITY_COMPONENTS_TO_TRANSFORM);
        return transform ? transform : 0;
    }

    bool world_to_screen(const vector3& worldPos, Vector2& screenPos, float screenW, float screenH) {
        float w = g_viewMatrix.right.w * worldPos.x + g_viewMatrix.up.w * worldPos.y + g_viewMatrix.forward.w * worldPos.z + g_viewMatrix.translation.w;
        if (w < 0.098f) return false;

        float x = g_viewMatrix.right.x * worldPos.x + g_viewMatrix.up.x * worldPos.y + g_viewMatrix.forward.x * worldPos.z + g_viewMatrix.translation.x;
        float y = g_viewMatrix.right.y * worldPos.x + g_viewMatrix.up.y * worldPos.y + g_viewMatrix.forward.y * worldPos.z + g_viewMatrix.translation.y;

        screenPos.x = (screenW / 2.0f) * (1.0f + x / w);
        screenPos.y = (screenH / 2.0f) * (1.0f - y / w);
        return true;
    }

    uintptr_t find_skeleton_recursive(uintptr_t transformAddr, int maxDepth, int currentDepth) {
        if (!transformAddr || currentDepth >= maxDepth) return 0;
        TransformInternal t(transformAddr);
        for (int i = 0; i < 20; i++) {
            uintptr_t childAddr = t.child(i);
            if (!childAddr) break;
            TransformInternal childT(childAddr);
            std::string childName = childT.name();
            if (childName.empty()) break;
            if (childName == "Skeleton") return childAddr;
            uintptr_t found = find_skeleton_recursive(childAddr, maxDepth, currentDepth + 1);
            if (found) return found;
        }
        return 0;
    }

    struct SkeletonBoneAddrs {
        uintptr_t Skeleton, Left_Hip, Left_Leg, Left_Foot, Right_Hip, Right_Leg, Right_Foot;
        uintptr_t Spine, Skull, Spot, Left_Shoulder, Left_Arm, Left_Hand, Right_Shoulder, Right_Arm, Right_Hand;
    };

    bool get_skeleton_addrs(uintptr_t skeletonTransformAddr, SkeletonBoneAddrs& addrs) {
        if (!skeletonTransformAddr) return false;
        TransformInternal Skeleton(skeletonTransformAddr);
        uintptr_t pLeft_Hip = Skeleton.child("Left_Hip"); if (!pLeft_Hip) return false;
        TransformInternal Left_Hip(pLeft_Hip);
        uintptr_t pLeft_Leg = Left_Hip.child(0); if (!pLeft_Leg) return false;
        TransformInternal Left_Leg(pLeft_Leg);
        uintptr_t pLeft_Foot = Left_Leg.child(0); if (!pLeft_Foot) return false;

        uintptr_t pRight_Hip = Skeleton.child("Right_Hip"); if (!pRight_Hip) return false;
        TransformInternal Right_Hip(pRight_Hip);
        uintptr_t pRight_Leg = Right_Hip.child(0); if (!pRight_Leg) return false;
        TransformInternal Right_Leg(pRight_Leg);
        uintptr_t pRight_Foot = Right_Leg.child(0); if (!pRight_Foot) return false;

        uintptr_t pSpine = Skeleton.child("Spine"); if (!pSpine) return false;
        TransformInternal Spine(pSpine);
        uintptr_t pSkull = Spine.child("Skull"); if (!pSkull) return false;
        TransformInternal Skull(pSkull);
        uintptr_t pSpot = Skull.child(0); if (!pSpot) return false;

        uintptr_t pLeft_Shoulder = Spine.child("Left_Shoulder"); if (!pLeft_Shoulder) return false;
        TransformInternal Left_Shoulder(pLeft_Shoulder);
        uintptr_t pLeft_Arm = Left_Shoulder.child(0); if (!pLeft_Arm) return false;
        TransformInternal Left_Arm(pLeft_Arm);
        uintptr_t pLeft_Hand = Left_Arm.child(0); if (!pLeft_Hand) return false;

        uintptr_t pRight_Shoulder = Spine.child("Right_Shoulder"); if (!pRight_Shoulder) return false;
        TransformInternal Right_Shoulder(pRight_Shoulder);
        uintptr_t pRight_Arm = Right_Shoulder.child(0); if (!pRight_Arm) return false;
        TransformInternal Right_Arm(pRight_Arm);
        uintptr_t pRight_Hand = Right_Arm.child(0); if (!pRight_Hand) return false;

        addrs.Skeleton = skeletonTransformAddr; addrs.Left_Hip = pLeft_Hip;
        addrs.Left_Leg = pLeft_Leg; addrs.Left_Foot = pLeft_Foot;
        addrs.Right_Hip = pRight_Hip; addrs.Right_Leg = pRight_Leg;
        addrs.Right_Foot = pRight_Foot; addrs.Spine = pSpine;
        addrs.Skull = pSkull; addrs.Spot = pSpot;
        addrs.Left_Shoulder = pLeft_Shoulder; addrs.Left_Arm = pLeft_Arm;
        addrs.Left_Hand = pLeft_Hand; addrs.Right_Shoulder = pRight_Shoulder;
        addrs.Right_Arm = pRight_Arm; addrs.Right_Hand = pRight_Hand;
        return true;
    }

    bool get_skeleton(const SkeletonBoneAddrs& addrs, SkeletonBones& bones) {
        if (!addrs.Skeleton) return false;
        bones.Skeleton = TransformInternal(addrs.Skeleton).position();
        bones.Left_Hip = TransformInternal(addrs.Left_Hip).position();
        bones.Left_Leg = TransformInternal(addrs.Left_Leg).position();
        bones.Left_Foot = TransformInternal(addrs.Left_Foot).position();
        bones.Right_Hip = TransformInternal(addrs.Right_Hip).position();
        bones.Right_Leg = TransformInternal(addrs.Right_Leg).position();
        bones.Right_Foot = TransformInternal(addrs.Right_Foot).position();
        bones.Spine = TransformInternal(addrs.Spine).position();
        bones.Skull = TransformInternal(addrs.Skull).position();
        bones.Spot = TransformInternal(addrs.Spot).position();
        bones.Left_Shoulder = TransformInternal(addrs.Left_Shoulder).position();
        bones.Left_Arm = TransformInternal(addrs.Left_Arm).position();
        bones.Left_Hand = TransformInternal(addrs.Left_Hand).position();
        bones.Right_Shoulder = TransformInternal(addrs.Right_Shoulder).position();
        bones.Right_Arm = TransformInternal(addrs.Right_Arm).position();
        bones.Right_Hand = TransformInternal(addrs.Right_Hand).position();
        return true;
    }

    void draw_skeleton_lines(ImDrawList* draw_list, const SkeletonBones& bones, ImU32 color, float screenW, float screenH, float thickness) {
        Vector2 sSpot, sSkull, sRShoulder, sLShoulder, sRArm, sLArm, sRHand, sLHand, sSpine, sRHip, sLHip, sRLeg, sLLeg, sRFoot, sLFoot;
        if (!world_to_screen(bones.Spot, sSpot, screenW, screenH)) return;
        if (!world_to_screen(bones.Skull, sSkull, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Shoulder, sRShoulder, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Shoulder, sLShoulder, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Arm, sRArm, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Arm, sLArm, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Hand, sRHand, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Hand, sLHand, screenW, screenH)) return;
        if (!world_to_screen(bones.Spine, sSpine, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Hip, sRHip, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Hip, sLHip, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Leg, sRLeg, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Leg, sLLeg, screenW, screenH)) return;
        if (!world_to_screen(bones.Right_Foot, sRFoot, screenW, screenH)) return;
        if (!world_to_screen(bones.Left_Foot, sLFoot, screenW, screenH)) return;

        draw_list->AddLine(ImVec2(sSpot.x, sSpot.y), ImVec2(sSkull.x, sSkull.y), color, thickness);
        draw_list->AddLine(ImVec2(sRShoulder.x, sRShoulder.y), ImVec2(sSkull.x, sSkull.y), color, thickness);
        draw_list->AddLine(ImVec2(sLShoulder.x, sLShoulder.y), ImVec2(sSkull.x, sSkull.y), color, thickness);
        draw_list->AddLine(ImVec2(sRArm.x, sRArm.y), ImVec2(sRShoulder.x, sRShoulder.y), color, thickness);
        draw_list->AddLine(ImVec2(sLArm.x, sLArm.y), ImVec2(sLShoulder.x, sLShoulder.y), color, thickness);
        draw_list->AddLine(ImVec2(sRHand.x, sRHand.y), ImVec2(sRArm.x, sRArm.y), color, thickness);
        draw_list->AddLine(ImVec2(sLHand.x, sLHand.y), ImVec2(sLArm.x, sLArm.y), color, thickness);
        draw_list->AddLine(ImVec2(sSpine.x, sSpine.y), ImVec2(sSkull.x, sSkull.y), color, thickness);
        draw_list->AddLine(ImVec2(sSpine.x, sSpine.y), ImVec2(sRHip.x, sRHip.y), color, thickness);
        draw_list->AddLine(ImVec2(sSpine.x, sSpine.y), ImVec2(sLHip.x, sLHip.y), color, thickness);
        draw_list->AddLine(ImVec2(sRHip.x, sRHip.y), ImVec2(sRLeg.x, sRLeg.y), color, thickness);
        draw_list->AddLine(ImVec2(sLHip.x, sLHip.y), ImVec2(sLLeg.x, sLLeg.y), color, thickness);
        draw_list->AddLine(ImVec2(sRLeg.x, sRLeg.y), ImVec2(sRFoot.x, sRFoot.y), color, thickness);
        draw_list->AddLine(ImVec2(sLLeg.x, sLLeg.y), ImVec2(sLFoot.x, sLFoot.y), color, thickness);
    }

    bool get_game_window_size(float& width, float& height) {
        HWND gameWnd = FindWindowA(nullptr, "Unturned");
        if (!gameWnd) return false;
        RECT rect;
        GetClientRect(gameWnd, &rect);
        width = (float)(rect.right - rect.left);
        height = (float)(rect.bottom - rect.top);
        return (width > 0 && height > 0);
    }

    bool update_view_matrix(void* localPlayer) {
        auto mainCameraK = mono::find_class("Assembly-CSharp", "SDG.Unturned.MainCamera");
        if (!mainCameraK) return false;

        uintptr_t cameraFieldAddr = mono::get_static_field_addr(mainCameraK, "camera");
        if (!cameraFieldAddr) cameraFieldAddr = mono::get_static_field_addr(mainCameraK, "_camera");
        if (!cameraFieldAddr) cameraFieldAddr = mono::get_static_field_addr(mainCameraK, "instance");
        if (!cameraFieldAddr) cameraFieldAddr = mono::get_static_field_addr(mainCameraK, "_instance");
        if (!cameraFieldAddr) return false;

        uintptr_t cameraPtr = read<uintptr_t>(cameraFieldAddr);
        if (!cameraPtr) return false;

        uintptr_t nativeCamera = read<uintptr_t>(cameraPtr + UNITY_OBJECT_TO_NATIVE);
        if (!nativeCamera) return false;

        UnityMatrix matrix;
        read(nativeCamera + 0x100, &matrix, sizeof(UnityMatrix));
        g_viewMatrix = matrix;

        vector3 camPos;
        read(nativeCamera + 0x454, &camPos, sizeof(vector3));
        g_camPosition = camPos;
        return true;
    }

    void draw_esp_box(ImDrawList* draw_list, const vector3& feetPos, float entityHeight, ImU32 boxColor, float screenW, float screenH) {
        vector3 headPos = feetPos;
        headPos.y += entityHeight;
        Vector2 screenFeet, screenHead;
        if (world_to_screen(feetPos, screenFeet, screenW, screenH) && world_to_screen(headPos, screenHead, screenW, screenH)) {
            float boxHeight = screenFeet.y - screenHead.y;
            if (boxHeight < 2.0f) return;
            float boxWidth = boxHeight / 2.0f;
            float centerX = (screenHead.x + screenFeet.x) / 2.0f;
            ImVec2 topLeft(centerX - boxWidth / 2.0f, screenHead.y);
            ImVec2 bottomRight(centerX + boxWidth / 2.0f, screenFeet.y);

            draw_list->AddRect(ImVec2(topLeft.x - 1, topLeft.y - 1), ImVec2(bottomRight.x + 1, bottomRight.y + 1), ImColor(0, 0, 0, 255), 0.0f, 0, 1.0f);
            draw_list->AddRect(ImVec2(topLeft.x + 1, topLeft.y + 1), ImVec2(bottomRight.x - 1, bottomRight.y - 1), ImColor(0, 0, 0, 255), 0.0f, 0, 1.0f);
            draw_list->AddRect(topLeft, bottomRight, boxColor, 0.0f, 0, 1.0f);
        }
    }

    void draw_skeleton_box(ImDrawList* draw_list, const SkeletonBones& bones, ImU32 boxColor, float screenW, float screenH) {
        Vector2 screenPoints[16];
        vector3 bonePositions[16] = {
            bones.Skeleton, bones.Left_Hip, bones.Left_Leg, bones.Left_Foot, bones.Right_Hip, bones.Right_Leg, bones.Right_Foot,
            bones.Spine, bones.Skull, bones.Spot, bones.Left_Shoulder, bones.Left_Arm, bones.Left_Hand,
            bones.Right_Shoulder, bones.Right_Arm, bones.Right_Hand
        };
        float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
        int validCount = 0;
        for (int i = 0; i < 16; i++) {
            if (world_to_screen(bonePositions[i], screenPoints[i], screenW, screenH)) {
                if (screenPoints[i].x < minX) minX = screenPoints[i].x;
                if (screenPoints[i].y < minY) minY = screenPoints[i].y;
                if (screenPoints[i].x > maxX) maxX = screenPoints[i].x;
                if (screenPoints[i].y > maxY) maxY = screenPoints[i].y;
                validCount++;
            }
        }
        if (validCount < 4) return;
        float boxH = maxY - minY;
        if (boxH < 2.0f) return;
        float padX = boxH * 0.1f, padY = boxH * 0.05f;
        minX -= padX; maxX += padX; minY -= padY; maxY += padY;

        draw_list->AddRect(ImVec2(minX - 1, minY - 1), ImVec2(maxX + 1, maxY + 1), ImColor(0, 0, 0, 255), 0.0f, 0, 1.0f);
        draw_list->AddRect(ImVec2(minX + 1, minY + 1), ImVec2(maxX - 1, maxY - 1), ImColor(0, 0, 0, 255), 0.0f, 0, 1.0f);
        draw_list->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), boxColor, 0.0f, 0, 1.0f);
    }

    std::string read_unity_string(uintptr_t strPtr) {
        if (!strPtr) return "";
        uint32_t len = read<uint32_t>(strPtr + MONO_STRING_LENGTH_OFFSET);
        if (len == 0 || len > 256) return "";
        std::string res = "";
        char buffer[512] = {0};
        read(strPtr + MONO_STRING_CHARS_OFFSET, buffer, len * 2);
        for (uint32_t i = 0; i < len * 2; i += 2) {
            if (buffer[i] != '\0') res += buffer[i];
        }
        return res;
    }

    int get_field_offset(mono_class_t* klass, const std::vector<std::string>& names) {
        if (!klass) return 0;
        for (const auto& name : names) {
            auto f = klass->find_field(name.c_str());
            if (f) return f->offset();
        }
        return 0;
    }

    void init_esp_offsets() {
        if (g_cheatOffsets.initialized) return;

        auto itemAssetK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ItemAsset");
        auto itemGunAssetK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ItemGunAsset");
        auto playerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.Player");
        if (playerK) {
            g_cheatOffsets.Player_skills = get_field_offset(playerK, {"_skills", "skills"});
            g_cheatOffsets.Player_equipment = get_field_offset(playerK, {"_equipment", "equipment"});
            g_cheatOffsets.Player_channel = get_field_offset(playerK, {"_channel", "channel"});
        }
        auto steamPlayerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.SteamPlayer");
        if (steamPlayerK) {
            g_cheatOffsets.SteamPlayer_player = get_field_offset(steamPlayerK, {"_player", "player"});
            g_cheatOffsets.SteamPlayer_playerID = get_field_offset(steamPlayerK, {"playerID", "_playerID"});
            g_cheatOffsets.SteamPlayer_model = get_field_offset(steamPlayerK, {"_model", "model"});
            g_cheatOffsets.SteamPlayer_isAdmin = get_field_offset(steamPlayerK, {"_isAdmin", "isAdmin"});
        }
        auto steamChannelK = mono::find_class("Assembly-CSharp", "SDG.Unturned.SteamChannel");
        if (steamChannelK) g_cheatOffsets.SteamChannel_owner = get_field_offset(steamChannelK, {"owner", "_owner"});

        auto steamPlayerIDK = mono::find_class("Assembly-CSharp", "SDG.Unturned.SteamPlayerID");
        if (steamPlayerIDK) g_cheatOffsets.SteamPlayerID_characterName = get_field_offset(steamPlayerIDK, {"characterName", "_characterName"});

        auto skillsK = mono::find_class("Assembly-CSharp", "SDG.Unturned.PlayerSkills");
        if (skillsK) g_cheatOffsets.PlayerSkills_reputation = get_field_offset(skillsK, {"_reputation", "reputation"});

        auto equipK = mono::find_class("Assembly-CSharp", "SDG.Unturned.PlayerEquipment");
        if (equipK) g_cheatOffsets.PlayerEquipment_asset = get_field_offset(equipK, {"_asset", "asset"});

        auto providerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.Provider");
        if (providerK) {
            g_cheatOffsets.Provider_clients = mono::get_static_field_addr(providerK, "_clients");
            g_cheatOffsets.Provider_modeConfigData = mono::get_static_field_addr(providerK, "_modeConfigData");
        }
        auto modeConfigK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ModeConfigData");
        if (modeConfigK) g_cheatOffsets.ModeConfigData_Gameplay = get_field_offset(modeConfigK, {"Gameplay", "_Gameplay"});

        auto gameplayConfigK = mono::find_class("Assembly-CSharp", "SDG.Unturned.GameplayConfigData");
        if (gameplayConfigK) {
            g_cheatOffsets.GameplayConfigData_Compass = get_field_offset(gameplayConfigK, {"Compass", "_Compass"});
            g_cheatOffsets.GameplayConfigData_Satellite = get_field_offset(gameplayConfigK, {"Satellite", "_Satellite"});
            g_cheatOffsets.GameplayConfigData_Chart = get_field_offset(gameplayConfigK, {"Chart", "_Chart"});
            g_cheatOffsets.GameplayConfigData_TimerExit = get_field_offset(gameplayConfigK, {"Timer_Exit", "_Timer_Exit"});
        }
        auto zombieManagerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ZombieManager");
        if (zombieManagerK) g_cheatOffsets.ZombieManager_regions = mono::get_static_field_addr(zombieManagerK, "_regions");

        auto zombieRegionK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ZombieRegion");
        if (zombieRegionK) g_cheatOffsets.ZombieRegion_zombies = get_field_offset(zombieRegionK, {"_zombies", "zombies"});

        auto zombieK = mono::find_class("Assembly-CSharp", "SDG.Unturned.Zombie");
        if (zombieK) {
            g_cheatOffsets.Zombie_isDead = get_field_offset(zombieK, {"_isDead", "isDead"});
            g_cheatOffsets.Zombie_isVisible = get_field_offset(zombieK, {"_isVisible", "isVisible"});
            g_cheatOffsets.Zombie_speciality = get_field_offset(zombieK, {"_speciality", "speciality", "Speciality", "<speciality>k__BackingField", "type", "_type"});
        }
        auto vehicleManagerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.VehicleManager");
        if (vehicleManagerK) {
            g_cheatOffsets.VehicleManager_vehicles = mono::get_static_field_addr(vehicleManagerK, "_vehicles");
            if (!g_cheatOffsets.VehicleManager_vehicles) {
                uintptr_t instanceAddr = mono::get_static_field_addr(vehicleManagerK, "instance");
                if (instanceAddr) {
                    uintptr_t instancePtr = read<uintptr_t>(instanceAddr);
                    if (instancePtr) g_cheatOffsets.VehicleManager_vehicles = mono::get_instance_field_addr((void*)instancePtr, vehicleManagerK, "vehicles");
                }
            }
        }
        auto interactableVehicleK = mono::find_class("Assembly-CSharp", "SDG.Unturned.InteractableVehicle");
        if (interactableVehicleK) g_cheatOffsets.InteractableVehicle_asset = get_field_offset(interactableVehicleK, {"asset", "_asset"});

        auto vehicleAssetK = mono::find_class("Assembly-CSharp", "SDG.Unturned.VehicleAsset");
        if (vehicleAssetK) g_cheatOffsets.VehicleAsset_vehicleName = get_field_offset(vehicleAssetK, {"_vehicleName", "vehicleName"});

        auto barricadeManagerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.BarricadeManager");
        if (barricadeManagerK) g_cheatOffsets.BarricadeManager_regions = mono::get_static_field_addr(barricadeManagerK, "<regions>k__BackingField");

        auto barricadeRegionK = mono::find_class("Assembly-CSharp", "SDG.Unturned.BarricadeRegion");
        if (barricadeRegionK) g_cheatOffsets.BarricadeRegion_drops = get_field_offset(barricadeRegionK, {"_drops", "drops"});

        auto barricadeDropK = mono::find_class("Assembly-CSharp", "SDG.Unturned.BarricadeDrop");
        if (barricadeDropK) {
            g_cheatOffsets.BarricadeDrop_model = get_field_offset(barricadeDropK, {"_model", "model"});
            g_cheatOffsets.BarricadeDrop_asset = get_field_offset(barricadeDropK, {"<asset>k__BackingField", "_asset", "asset"});
        }
        auto itemManagerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ItemManager");
        if (itemManagerK) g_cheatOffsets.ItemManager_regions = mono::get_static_field_addr(itemManagerK, "<regions>k__BackingField");

        auto itemRegionK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ItemRegion");
        if (itemRegionK) g_cheatOffsets.ItemRegion_drops = get_field_offset(itemRegionK, {"_drops", "drops"});

        auto itemDropK = mono::find_class("Assembly-CSharp", "SDG.Unturned.ItemDrop");
        if (itemDropK) {
            g_cheatOffsets.ItemDrop_model = get_field_offset(itemDropK, {"_model", "model", "modelTransform"});
            g_cheatOffsets.ItemDrop_interactableItem = get_field_offset(itemDropK, {"_interactableItem", "interactableItem"});
        }
        auto interactableItemK = mono::find_class("Assembly-CSharp", "SDG.Unturned.InteractableItem");
        if (interactableItemK) {
            g_cheatOffsets.InteractableItem_asset = get_field_offset(interactableItemK, {"asset", "_asset"});
            g_cheatOffsets.InteractableItem_item = get_field_offset(interactableItemK, {"item", "_item"});
        }
        if (itemAssetK) {
            g_cheatOffsets.ItemAsset_itemName = get_field_offset(itemAssetK, {"<itemName>k__BackingField", "_itemName", "itemName", "ItemName"});
            g_cheatOffsets.ItemAsset_id = get_field_offset(itemAssetK, {"id", "_id"});
            g_cheatOffsets.ItemAsset_type = get_field_offset(itemAssetK, {"_type", "type"});
            g_cheatOffsets.ItemAsset_rarity = get_field_offset(itemAssetK, {"rarity", "_rarity"});
        }
        if (itemGunAssetK) {
            g_cheatOffsets.ItemGunAsset_recoilMin_x = get_field_offset(itemGunAssetK, {"recoilMin_x", "recoilMinX"});
            g_cheatOffsets.ItemGunAsset_recoilMax_x = get_field_offset(itemGunAssetK, {"recoilMax_x", "recoilMaxX"});
            g_cheatOffsets.ItemGunAsset_recoilMin_y = get_field_offset(itemGunAssetK, {"recoilMin_y", "recoilMinY"});
            g_cheatOffsets.ItemGunAsset_recoilMax_y = get_field_offset(itemGunAssetK, {"recoilMax_y", "recoilMaxY"});
            g_cheatOffsets.ItemGunAsset_baseSpreadAngleRadians = get_field_offset(itemGunAssetK, {"<baseSpreadAngleRadians>k__BackingField", "baseSpreadAngleRadians"});
        }
        auto itemK = mono::find_class("Assembly-CSharp", "SDG.Unturned.Item");
        if (itemK) {
            g_cheatOffsets.Item_amount = get_field_offset(itemK, {"amount", "_amount"});
            g_cheatOffsets.Item_quality = get_field_offset(itemK, {"quality", "_quality"});
        }
        g_cheatOffsets.initialized = true;
    }

    struct PtrCache {
        struct Player {
            uintptr_t transformAddr;
            void* playerPtr;
            std::string baseName;
            std::string equipmentName;
            int reputation;
            bool hasSkeleton;
            SkeletonBoneAddrs boneAddrs;
        };
        struct Zombie {
            uintptr_t transformAddr;
            std::string typeName;
            bool hasSkeleton;
            SkeletonBoneAddrs boneAddrs;
        };
        struct Vehicle {
            uintptr_t transformAddr;
            std::string name;
        };
        struct Barricade {
            uintptr_t transformAddr;
            std::string name;
        };
        struct Item {
            uintptr_t transformAddr;
            std::string name;
            uint16_t id;
            uint8_t type;
            uint16_t rarity;
            uint8_t amount;
            uint8_t quality;
        };
        std::vector<Player> players;
        std::vector<Zombie> zombies;
        std::vector<Vehicle> vehicles;
        std::vector<Barricade> barricades;
        std::vector<Item> items;
    } g_ptrCache;

    void SlowUpdatePlayers() {
        g_ptrCache.players.clear();
        if (!g_cheatOffsets.Provider_clients) return;
        uintptr_t clientsArrPtr = read<uintptr_t>(g_cheatOffsets.Provider_clients);
        if (!clientsArrPtr) return;
        UnityArray<void*> clients(clientsArrPtr);
        uint32_t count = clients.Count();
        if (count > 100) count = 100;
        for (uint32_t i = 0; i < count; i++) {
            void* steamPlayer = clients.Get(i);
            if (!steamPlayer) continue;
            void* player = g_cheatOffsets.SteamPlayer_player ? read<void*>((uintptr_t)steamPlayer + g_cheatOffsets.SteamPlayer_player) : nullptr;
            if (!player) continue;
            uintptr_t transformAddr = find_transform_addr_from_player((uintptr_t)player);
            if (!transformAddr) continue;
            TransformInternal transform(transformAddr);
            vector3 pos = transform.position();
            float dist = g_camPosition.distance(pos);
            std::string baseName = "Player";
            int reputation = 0;
            void* playerID = g_cheatOffsets.SteamPlayer_playerID ? read<void*>((uintptr_t)steamPlayer + g_cheatOffsets.SteamPlayer_playerID) : nullptr;
            if (playerID && g_cheatOffsets.SteamPlayerID_characterName) {
                uintptr_t strPtr = read<uintptr_t>((uintptr_t)playerID + g_cheatOffsets.SteamPlayerID_characterName);
                if (strPtr) baseName = read_unity_string(strPtr);
            }
            void* skills = g_cheatOffsets.Player_skills ? read<void*>((uintptr_t)player + g_cheatOffsets.Player_skills) : nullptr;
            if (skills && g_cheatOffsets.PlayerSkills_reputation) {
                reputation = read<int>((uintptr_t)skills + g_cheatOffsets.PlayerSkills_reputation);
            }
            std::string equipmentName = "";
            void* equipment = g_cheatOffsets.Player_equipment ? read<void*>((uintptr_t)player + g_cheatOffsets.Player_equipment) : nullptr;
            if (equipment && g_cheatOffsets.PlayerEquipment_asset) {
                void* asset = read<void*>((uintptr_t)equipment + g_cheatOffsets.PlayerEquipment_asset);
                if (asset && g_cheatOffsets.ItemAsset_itemName) {
                    uintptr_t assetStrPtr = read<uintptr_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_itemName);
                    if (assetStrPtr) equipmentName = read_unity_string(assetStrPtr);
                }
            }
            if (dist < 500.0f) {  }

            PtrCache::Player cp; 
            cp.transformAddr = transformAddr;
            cp.baseName = baseName; 
            cp.equipmentName = equipmentName; 
            cp.reputation = reputation; 
            cp.playerPtr = player;
            cp.hasSkeleton = false;

            uintptr_t modelPtr = g_cheatOffsets.SteamPlayer_model ? read<uintptr_t>((uintptr_t)steamPlayer + g_cheatOffsets.SteamPlayer_model) : 0;
            if (modelPtr) {
                uintptr_t modelNativeTransform = get_transform_from_component(modelPtr);
                if (!modelNativeTransform) modelNativeTransform = read<uintptr_t>(modelPtr + UNITY_OBJECT_TO_NATIVE);
                uintptr_t skeletonTransform = find_skeleton_recursive(modelNativeTransform);
                if (skeletonTransform) {
                    cp.hasSkeleton = get_skeleton_addrs(skeletonTransform, cp.boneAddrs);
                }
            }
            g_ptrCache.players.push_back(cp);
        }
    }

    void SlowUpdateZombies() {
        g_ptrCache.zombies.clear();
        if (!g_cheatOffsets.ZombieManager_regions) return;
        uintptr_t regionsArrPtr = read<uintptr_t>(g_cheatOffsets.ZombieManager_regions);
        if (!regionsArrPtr) return;
        uint32_t regionCount = read<uint32_t>(regionsArrPtr + MONO_ARRAY_LENGTH_OFFSET);
        if (regionCount > 200) regionCount = 200;
        for (uint32_t r = 0; r < regionCount; r++) {
            uintptr_t region = read<uintptr_t>(regionsArrPtr + MONO_ARRAY_FIRST_ELEM_OFFSET + (r * 0x8));
            if (!region) continue;
            uintptr_t zombiesList = g_cheatOffsets.ZombieRegion_zombies ? read<uintptr_t>(region + g_cheatOffsets.ZombieRegion_zombies) : 0;
            if (!zombiesList) continue;
            uintptr_t itemsArr = read<uintptr_t>(zombiesList + MONO_LIST_ITEMS_OFFSET);
            uint32_t zombieCount = read<uint32_t>(zombiesList + MONO_LIST_SIZE_OFFSET);
            if (!itemsArr || zombieCount == 0 || zombieCount > 500) continue;
            for (uint32_t z = 0; z < zombieCount; z++) {
                uintptr_t zombie = read<uintptr_t>(itemsArr + MONO_ARRAY_FIRST_ELEM_OFFSET + (z * 0x8));
                if (!zombie) continue;
                bool isDead = g_cheatOffsets.Zombie_isDead ? read<bool>(zombie + g_cheatOffsets.Zombie_isDead) : false;
                bool isVisible = g_cheatOffsets.Zombie_isVisible ? read<bool>(zombie + g_cheatOffsets.Zombie_isVisible) : true;
                if (isDead || !isVisible) continue;
                uintptr_t transformAddr = find_transform_addr_from_player(zombie);
                if (!transformAddr) continue;
                TransformInternal transform(transformAddr);
                vector3 pos = transform.position();
                float dist = g_camPosition.distance(pos);
                int speciality = g_cheatOffsets.Zombie_speciality ? read<int>(zombie + g_cheatOffsets.Zombie_speciality) : 0;
                std::string zombieType = "Normal";
                switch (speciality) {
                    case 0: zombieType = "Normal"; break; case 1: zombieType = "Mega"; break; case 2: zombieType = "Crawler"; break;
                    case 3: zombieType = "Sprinter"; break; case 4: case 5: zombieType = "Flanker"; break; case 6: zombieType = "Burner"; break;
                    case 7: zombieType = "Acid"; break; case 8: zombieType = "Boss Electric"; break; case 9: zombieType = "Boss Wind"; break;
                    case 10: zombieType = "Boss Magma"; break; case 11: zombieType = "Spirit"; break; case 12: zombieType = "Boss Spirit"; break;
                    case 13: zombieType = "Boss Nuclear"; break; case 14: zombieType = "Red Volatile"; break; case 15: zombieType = "Blue Volatile"; break;
                    default: zombieType = "Zombie"; break;
                }
                PtrCache::Zombie cz; 
                cz.transformAddr = transformAddr;
                cz.typeName = zombieType; 
                cz.hasSkeleton = false;
                
                uintptr_t skeletonTransform = find_skeleton_recursive(transformAddr);
                if (skeletonTransform) {
                    cz.hasSkeleton = get_skeleton_addrs(skeletonTransform, cz.boneAddrs);
                }
                g_ptrCache.zombies.push_back(cz);
            }
        }
    }

    void SlowUpdateVehicles() {
        g_ptrCache.vehicles.clear();
        if (!g_cheatOffsets.VehicleManager_vehicles) return;
        uintptr_t vehiclesListPtr = read<uintptr_t>(g_cheatOffsets.VehicleManager_vehicles);
        if (!vehiclesListPtr) return;
        uintptr_t itemsArr = read<uintptr_t>(vehiclesListPtr + MONO_LIST_ITEMS_OFFSET);
        uint32_t count = read<uint32_t>(vehiclesListPtr + MONO_LIST_SIZE_OFFSET);
        if (!itemsArr || count == 0 || count > 500) return;
        for (uint32_t i = 0; i < count; i++) {
            uintptr_t vehicle = read<uintptr_t>(itemsArr + MONO_ARRAY_FIRST_ELEM_OFFSET + (i * 0x8));
            if (!vehicle) continue;
            uintptr_t transformAddr = get_transform_from_component(vehicle);
            if (!transformAddr) continue;
            TransformInternal t(transformAddr);
            vector3 pos = t.position();
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;
            std::string name = "Vehicle";
            void* asset = g_cheatOffsets.InteractableVehicle_asset ? read<void*>(vehicle + g_cheatOffsets.InteractableVehicle_asset) : nullptr;
            if (asset && g_cheatOffsets.VehicleAsset_vehicleName) {
                uintptr_t strPtr = read<uintptr_t>((uintptr_t)asset + g_cheatOffsets.VehicleAsset_vehicleName);
                if (strPtr) name = read_unity_string(strPtr);
            }
            g_ptrCache.vehicles.push_back({transformAddr, name});
        }
    }

    void SlowUpdateBarricades() {
        g_ptrCache.barricades.clear();
        if (!g_cheatOffsets.BarricadeManager_regions) return;
        uintptr_t regionsArrPtr = read<uintptr_t>(g_cheatOffsets.BarricadeManager_regions);
        if (!regionsArrPtr) return;
        UnityArray<uintptr_t> regions(regionsArrPtr);
        uint32_t regionCount = regions.Count();
        if (regionCount > 10000) regionCount = 10000;
        for (uint32_t r = 0; r < regionCount; r++) {
            uintptr_t region = regions.Get(r);
            if (!region) continue;
            uintptr_t dropsList = g_cheatOffsets.BarricadeRegion_drops ? read<uintptr_t>(region + g_cheatOffsets.BarricadeRegion_drops) : 0;
            if (!dropsList) continue;
            uintptr_t itemsArr = read<uintptr_t>(dropsList + MONO_LIST_ITEMS_OFFSET);
            uint32_t dropCount = read<uint32_t>(dropsList + MONO_LIST_SIZE_OFFSET);
            if (!itemsArr || dropCount == 0 || dropCount > 1000) continue;
            UnityArray<uintptr_t> drops(itemsArr);
            for (uint32_t d = 0; d < dropCount; d++) {
                uintptr_t drop = drops.Get(d);
                if (!drop) continue;
                uintptr_t model = g_cheatOffsets.BarricadeDrop_model ? read<uintptr_t>(drop + g_cheatOffsets.BarricadeDrop_model) : 0;
                if (!model) continue;
                uintptr_t transformAddr = find_transform_addr_from_player(model);
                if (!transformAddr) continue;
                TransformInternal t(transformAddr);
                vector3 pos = t.position();
                std::string name = "Barricade";
                void* asset = g_cheatOffsets.BarricadeDrop_asset ? read<void*>(drop + g_cheatOffsets.BarricadeDrop_asset) : nullptr;
                if (asset && g_cheatOffsets.ItemAsset_itemName) {
                    uintptr_t strPtr = read<uintptr_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_itemName);
                    if (strPtr) name = read_unity_string(strPtr);
                }
                if (name.find("Generator") != std::string::npos || name.find("Sentry") != std::string::npos || name.find("Bedroll") != std::string::npos ||
                    name.find("Claim") != std::string::npos || name.find("Wardrobe") != std::string::npos || name.find("Crate") != std::string::npos ||
                    name == "Cot" || name == "Locker") {
                    g_ptrCache.barricades.push_back({transformAddr, name});
                }
            }
        }
    }

    void SlowUpdateItems() {
        g_ptrCache.items.clear();
        if (!g_cheatOffsets.ItemManager_regions) return;
        uintptr_t regionsArrPtr = read<uintptr_t>(g_cheatOffsets.ItemManager_regions);
        if (!regionsArrPtr) return;
        uint32_t regionCount = read<uint32_t>(regionsArrPtr + MONO_ARRAY_LENGTH_OFFSET);
        if (regionCount > 10000) regionCount = 10000;
        for (uint32_t r = 0; r < regionCount; r++) {
            uintptr_t region = read<uintptr_t>(regionsArrPtr + MONO_ARRAY_FIRST_ELEM_OFFSET + 0x8 + (r * sizeof(uintptr_t)));
            if (!region) continue;
            uintptr_t dropsList = g_cheatOffsets.ItemRegion_drops ? read<uintptr_t>(region + g_cheatOffsets.ItemRegion_drops) : 0;
            if (!dropsList) continue;
            uintptr_t itemsArr = read<uintptr_t>(dropsList + MONO_LIST_ITEMS_OFFSET);
            uint32_t dropCount = read<uint32_t>(dropsList + MONO_LIST_SIZE_OFFSET);
            if (!itemsArr || dropCount == 0 || dropCount > 1000) continue;
            for (uint32_t d = 0; d < dropCount; d++) {
                uintptr_t drop = read<uintptr_t>(itemsArr + MONO_ARRAY_FIRST_ELEM_OFFSET + (d * sizeof(uintptr_t)));
                if (!drop) continue;
                uintptr_t model = g_cheatOffsets.ItemDrop_model ? read<uintptr_t>(drop + g_cheatOffsets.ItemDrop_model) : 0;
                if (!model) continue;
                uintptr_t nativeTransform = read<uintptr_t>(model + UNITY_OBJECT_TO_NATIVE);
                if (!nativeTransform) continue;
                TransformInternal t(nativeTransform);
                vector3 pos = t.position();
                if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) continue;
                PtrCache::Item ci; 
                ci.transformAddr = nativeTransform; 
                ci.name = "Item"; ci.id = 0; ci.type = 0; ci.rarity = 0; ci.amount = 0; ci.quality = 0;
                void* interactableItem = g_cheatOffsets.ItemDrop_interactableItem ? read<void*>(drop + g_cheatOffsets.ItemDrop_interactableItem) : nullptr;
                if (interactableItem) {
                    void* asset = g_cheatOffsets.InteractableItem_asset ? read<void*>((uintptr_t)interactableItem + g_cheatOffsets.InteractableItem_asset) : nullptr;
                    if (asset) {
                        if (g_cheatOffsets.ItemAsset_id) ci.id = read<uint16_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_id);
                        if (g_cheatOffsets.ItemAsset_type) ci.type = read<uint8_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_type);
                        if (g_cheatOffsets.ItemAsset_rarity) ci.rarity = read<uint16_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_rarity);
                        if (g_cheatOffsets.ItemAsset_itemName) {
                            uintptr_t strPtr = read<uintptr_t>((uintptr_t)asset + g_cheatOffsets.ItemAsset_itemName);
                            if (strPtr) ci.name = read_unity_string(strPtr);
                        }
                    }
                    void* itemData = g_cheatOffsets.InteractableItem_item ? read<void*>((uintptr_t)interactableItem + g_cheatOffsets.InteractableItem_item) : nullptr;
                    if (itemData) {
                        if (g_cheatOffsets.Item_amount) ci.amount = read<uint8_t>((uintptr_t)itemData + g_cheatOffsets.Item_amount);
                        if (g_cheatOffsets.Item_quality) ci.quality = read<uint8_t>((uintptr_t)itemData + g_cheatOffsets.Item_quality);
                    }
                }
                g_ptrCache.items.push_back(ci);
            }
        }
    }

    void FastUpdate(std::vector<CachedPlayer>& tp, std::vector<CachedZombie>& tz, std::vector<CachedVehicle>& tv, std::vector<CachedBarricade>& tb, std::vector<CachedItem>& ti, std::vector<DangerEntity>& td) {
        for (auto& p : g_ptrCache.players) {
            TransformInternal t(p.transformAddr);
            vector3 pos = t.position();
            float dist = g_camPosition.distance(pos);
            if (dist < 500.0f) td.push_back({ p.baseName, p.reputation, dist, false });
            
            CachedPlayer cp; cp.pos = pos; cp.baseName = p.baseName; cp.equipmentName = p.equipmentName; 
            cp.reputation = p.reputation; cp.playerPtr = p.playerPtr; cp.hasSkeleton = p.hasSkeleton;
            if (p.hasSkeleton) {
                cp.hasSkeleton = get_skeleton(p.boneAddrs, cp.bones);
            }
            tp.push_back(cp);
        }
        for (auto& z : g_ptrCache.zombies) {
            TransformInternal t(z.transformAddr);
            vector3 pos = t.position();
            float dist = g_camPosition.distance(pos);
            if (dist < 100.0f) td.push_back({ z.typeName, 0, dist, true });
            
            CachedZombie cz; cz.pos = pos; cz.typeName = z.typeName; cz.hasSkeleton = z.hasSkeleton;
            if (z.hasSkeleton) {
                cz.hasSkeleton = get_skeleton(z.boneAddrs, cz.bones);
            }
            tz.push_back(cz);
        }
        for (auto& v : g_ptrCache.vehicles) {
            TransformInternal t(v.transformAddr);
            tv.push_back({t.position(), v.name});
        }
        for (auto& b : g_ptrCache.barricades) {
            TransformInternal t(b.transformAddr);
            tb.push_back({t.position(), b.name});
        }
        for (auto& i : g_ptrCache.items) {
            TransformInternal t(i.transformAddr);
            ti.push_back({t.position(), i.name, i.id, i.type, i.rarity, i.amount, i.quality});
        }
    }

    void cache_updater_thread() {
        auto last_full_update = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        while (g_esp_thread_running) {
            std::vector<CachedPlayer> tp;
            std::vector<CachedZombie> tz;
            std::vector<CachedVehicle> tv;
            std::vector<CachedBarricade> tb;
            std::vector<CachedItem> ti;
            std::vector<DangerEntity> td;

            init_esp_offsets();
            
            auto now = std::chrono::steady_clock::now();
            if (now - last_full_update > std::chrono::milliseconds(1000)) {
                SlowUpdatePlayers();
                SlowUpdateZombies();
                SlowUpdateVehicles();
                SlowUpdateBarricades();
                SlowUpdateItems();
                last_full_update = now;
            }

            FastUpdate(tp, tz, tv, tb, ti, td);

            std::sort(td.begin(), td.end(), [](const DangerEntity& a, const DangerEntity& b) {
                return a.distance < b.distance;
            });

            
            {
                std::lock_guard<std::mutex> lock(cache_mutex);
                cached_players = std::move(tp);
                cached_zombies = std::move(tz);
                cached_vehicles = std::move(tv);
                cached_barricades = std::move(tb);
                cached_items = std::move(ti);
                close_entities = std::move(td);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); 
        }
    }

    void draw_text_centered(ImDrawList* draw_list, Vector2 screenPos, const char* text, ImColor color) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        draw_list->AddText(ImVec2(screenPos.x - textSize.x / 2.0f, screenPos.y), color, text);
    }

    void render_esp(void* localPlayer, bool draw_local, bool draw_players, bool draw_zombies, bool draw_vehicles, bool draw_barricades, bool draw_items, bool draw_player_skeletons, bool draw_zombie_skeletons, float player_dist, float zombie_dist, float veh_dist, float bar_dist, float item_dist, bool draw_danger_radius) {
        if (!update_view_matrix(localPlayer)) return;

        float screenW = 1920.0f, screenH = 1080.0f;
        get_game_window_size(screenW, screenH);
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        std::lock_guard<std::mutex> lock(cache_mutex);
        
        if (draw_players) {
            for (const auto& p : cached_players) {
                if (p.playerPtr == localPlayer && !draw_local) continue;
                float dist = g_camPosition.distance(p.pos);
                if (dist > player_dist) continue;
                
                bool drewBox = false;
                if (p.hasSkeleton) {
                    draw_skeleton_box(draw_list, p.bones, ImColor(255, 255, 255, 255), screenW, screenH);
                    drewBox = true;
                    if (draw_player_skeletons) draw_skeleton_lines(draw_list, p.bones, ImColor(255, 255, 255, 255), screenW, screenH);
                }
                if (!drewBox) draw_esp_box(draw_list, p.pos, 1.8f, ImColor(255, 255, 255, 255), screenW, screenH);

                Vector2 screenPos;
                if (world_to_screen(p.pos, screenPos, screenW, screenH)) {
                    char text[256];
                    sprintf_s(text, "%s [%d] %.0fm", p.baseName.c_str(), p.reputation, dist);
                    ImVec2 textSize = ImGui::CalcTextSize(text);
                    draw_list->AddText(ImVec2(screenPos.x - textSize.x / 2.0f, screenPos.y), ImColor(255, 255, 255, 255), text);
                    if (!p.equipmentName.empty()) {
                        char equipText[128];
                        sprintf_s(equipText, "%s", p.equipmentName.c_str());
                        ImVec2 equipSize = ImGui::CalcTextSize(equipText);
                        draw_list->AddText(ImVec2(screenPos.x - equipSize.x / 2.0f, screenPos.y + textSize.y), ImColor(200, 200, 200, 255), equipText);
                    }
                }
            }
        }

        if (draw_zombies) {
            for (const auto& z : cached_zombies) {
                float dist = g_camPosition.distance(z.pos);
                if (dist > zombie_dist) continue;
                bool drewBox = false;
                if (z.hasSkeleton) {
                    draw_skeleton_box(draw_list, z.bones, ImColor(255, 0, 0, 255), screenW, screenH);
                    drewBox = true;
                    if (draw_zombie_skeletons) draw_skeleton_lines(draw_list, z.bones, ImColor(255, 0, 0, 255), screenW, screenH);
                }
                if (!drewBox) draw_esp_box(draw_list, z.pos, 1.7f, ImColor(255, 0, 0, 255), screenW, screenH);

                Vector2 screenPos;
                if (world_to_screen(z.pos, screenPos, screenW, screenH)) {
                    char text[64];
                    sprintf_s(text, "[%s] %.0fm", z.typeName.c_str(), dist);
                    draw_text_centered(draw_list, screenPos, text, ImColor(255, 0, 0, 255));
                }
            }
        }

        if (draw_vehicles) {
            for (const auto& v : cached_vehicles) {
                float dist = g_camPosition.distance(v.pos);
                if (dist > veh_dist) continue;
                Vector2 screenPos;
                if (world_to_screen(v.pos, screenPos, screenW, screenH)) {
                    char text[128];
                    sprintf_s(text, "%s [%.0fm]", v.name.c_str(), dist);
                    draw_text_centered(draw_list, screenPos, text, ImColor(200, 190, 230, 255));
                }
            }
        }

        if (draw_barricades) {
            for (const auto& b : cached_barricades) {
                float dist = g_camPosition.distance(b.pos);
                if (dist > bar_dist) continue;
                Vector2 screenPos;
                if (world_to_screen(b.pos, screenPos, screenW, screenH)) {
                    char text[128];
                    sprintf_s(text, "%s [%.0fm]", b.name.c_str(), dist);
                    draw_text_centered(draw_list, screenPos, text, ImColor(178, 178, 178, 255));
                }
            }
        }

        if (draw_items) {
            for (const auto& item : cached_items) {
                float dist = g_camPosition.distance(item.pos);
                if (dist > item_dist) continue;
                Vector2 screenPos;
                if (world_to_screen(item.pos, screenPos, screenW, screenH)) {
                    char text[128];
                    if (item.amount > 1) sprintf_s(text, "%s x%d [%.0fm]", item.name.c_str(), item.amount, dist);
                    else sprintf_s(text, "%s [%.0fm]", item.name.c_str(), dist);
                    
                    ImColor col = ImColor(255, 255, 255, 255);
                    bool isExplosive = (item.id == 254 || item.id == 1100 || item.id == 512 || item.type == 13 || item.type == 17 || item.type == 16);
                    bool isSpecialMelee = (item.id == 226 || item.id == 116);
                    bool isBuildingMaterial = (item.id == 68);

                    if ((item.type == 4 || item.type == 3) && item.amount) col = ImColor(107, 122, 79, 255);
                    else if (item.type == 1) col = ImColor(107, 255, 79, 255);
                    else if (item.type == 2 && isSpecialMelee) col = ImColor(79, 79, 79, 255);
                    else if (isExplosive) col = ImColor(255, 0, 0, 255);
                    else if (item.rarity >= 3 || item.type == 5 || item.type == 6) col = ImColor(165, 120, 219, 255);
                    else if (isBuildingMaterial) col = ImColor(165, 120, 219, 255);
                    else col = ImColor(200, 200, 200, 255);

                    ImVec2 textSize = ImGui::CalcTextSize(text);
                    draw_list->AddRectFilled(ImVec2(screenPos.x - textSize.x / 2.0f - 2, screenPos.y - 2), ImVec2(screenPos.x + textSize.x / 2.0f + 2, screenPos.y + textSize.y + 2), ImColor(0, 0, 0, 150));
                    draw_list->AddText(ImVec2(screenPos.x - textSize.x / 2.0f, screenPos.y), col, text);
                }
            }
        }

        if (draw_danger_radius && !close_entities.empty()) {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(200, -1), ImVec2(200, -1));
            ImGui::Begin("Danger Radius", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[Danger Radius] %zu Targets", close_entities.size());
            ImGui::Separator();
            for (const auto& dp : close_entities) {
                ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (dp.is_zombie) {
                    color = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
                    ImGui::TextColored(color, "[Z] %s", dp.name.c_str());
                } else {
                    if (dp.reputation < 0) color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    else if (dp.reputation > 0) color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                    ImGui::TextColored(color, "%s [%d]", dp.name.c_str(), dp.reputation);
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%.0fm", dp.distance);
            }
            ImGui::End();
        }
    }
}

