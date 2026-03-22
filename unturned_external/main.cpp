#define NOMINMAX
#include "sdk/base.h"
#include "sdk/mono.h"
#include "features/no_recoil.h"
#include "features/no_spread.h"
#include "features/admin.h"
#include "features/players.h"
#include "features/overlay.h"
#include "features/esp.h"
#include "features/game_config.h"
#include <iostream>
#include <string>

using namespace std;

int main() {
    wstring game_name = L"Unturned.exe";
    DWORD pid = get_process_id(game_name);
    if (!pid) {
        wcout << L"game not found" << endl;
        Sleep(3000);
        return 1;
    }
    
    process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process_handle) {
        wcout << L"failed to open process" << endl;
        Sleep(3000);
        return 1;
    }
    
    MONO_BASEADDRESS = get_module_base(pid, L"mono-2.0-bdwgc.dll");
    uintptr_t unity_base = get_module_base(pid, L"UnityPlayer.dll");
    
    wcout << L"mono base: 0x" << hex << MONO_BASEADDRESS << endl;
    wcout << L"unity base: 0x" << hex << unity_base << endl;
    
    mono::init_functions();
    wcout << L"mono initialized" << endl;

    auto playerK = mono::find_class("Assembly-CSharp", "SDG.Unturned.Player");
    if (!playerK) {
        wcout << L"player class missing" << endl;
        
    }

    uintptr_t staticLocalAddr = playerK ? mono::get_static_field_addr(playerK, "_localPlayer") : 0;

    wcout << L"\ncheat initialized." << endl;
    wcout << L"running... press end to exit (home to toggle menu)\n" << endl;
    wcout << L"\nadmin spoof enabled\n" << endl;
    wcout << L"\nno recoil enabled\n" << endl;
    wcout << L"\nno spread enabled" << endl;

    features::g_esp_thread_running = true;
    std::thread esp_cache_thread(features::cache_updater_thread);
    esp_cache_thread.detach();

    OverlayHijack::Hijack overlay;
    if (!overlay.Start()) {
        wcout << L"failed to hijack Discord overlay. make sure discord is running with the NEW overlay enabled." << endl;
        Sleep(5000);
        CloseHandle(process_handle);
        return 1;
    }

    bool show_menu = true;
    bool enable_no_recoil = true;
    bool enable_no_spread = true;
    bool enable_esp = false;
    bool esp_local_player = false;
    bool esp_danger_radius = false;
    bool esp_players = true;
    bool esp_player_skeletons = true;
    bool esp_zombies = false;
    bool esp_zombie_skeletons = false;
    bool esp_vehicles = false;
    bool esp_barricades = false;
    bool esp_items = false;

    float esp_zombie_distance = 100.0f;
    float esp_player_distance = 600.0f;
    float esp_vehicle_distance = 500.0f;
    float esp_barricade_distance = 150.0f;
    float esp_item_distance = 100.0f;

    bool enable_admin = false;
    features::GameConfigSettings game_settings;

    while (true) {
        if (!overlay.Render())
            break;

        void* localPlayer = nullptr;
        if (staticLocalAddr) {
            localPlayer = read<void*>(staticLocalAddr);
        }

        if (GetAsyncKeyState(VK_HOME) & 1) {
            show_menu = !show_menu;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);

        ImGui::NewFrame();

        if (show_menu) {
            ImGui::Begin("Unturned menu", &show_menu, ImGuiWindowFlags_AlwaysAutoResize);
            
            if (ImGui::CollapsingHeader("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
            }

            if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox("Master Switch", &enable_esp)) {
                }
                if (enable_esp) {
                    ImGui::Indent();
                    ImGui::Checkbox("draw local player", &esp_local_player);
                    
                    ImGui::Text("Player ESP");
                    ImGui::Checkbox("draw danger radius", &esp_danger_radius);
                    ImGui::Checkbox("draw players", &esp_players);
                    if (esp_players) {
                        ImGui::Checkbox("player skeleton", &esp_player_skeletons);
                        ImGui::SliderFloat("player dist", &esp_player_distance, 10.0f, 2000.0f, "%.0fm");
                    }

                    ImGui::Checkbox("draw zombies", &esp_zombies);
                    if (esp_zombies) {
                        ImGui::Checkbox("draw zombie skeletons", &esp_zombie_skeletons);
                        ImGui::SliderFloat("Zombie Dist", &esp_zombie_distance, 10.0f, 750.0f, "%.0fm");
                    }
                    
                    ImGui::Checkbox("draw Vehicles", &esp_vehicles);
                    if (esp_vehicles) ImGui::SliderFloat("vehicle dist", &esp_vehicle_distance, 10.0f, 2500.0f, "%.0fm");

                    ImGui::Checkbox("draw barricades/structures (broken)", &esp_barricades);
                    if (esp_barricades) ImGui::SliderFloat("Barricade Dist", &esp_barricade_distance, 10.0f, 1000.0f, "%.0fm");

                    ImGui::Checkbox("draw items (loot, very bad performance)", &esp_items);
                    if (esp_items) ImGui::SliderFloat("item dist", &esp_item_distance, 10.0f, 400.0f, "%.0fm");

                    ImGui::Unindent();
                }
            }

            if (ImGui::CollapsingHeader("weapons", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("no recoil", &enable_no_recoil);
                ImGui::Checkbox("no spread", &enable_no_spread);
            }

            if (ImGui::CollapsingHeader("misc / world", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("admin spoof", &enable_admin);
                ImGui::Checkbox("force compass", &game_settings.compass);
                ImGui::Checkbox("force chart", &game_settings.chart);
                ImGui::Checkbox("force satellite", &game_settings.satellite);
                ImGui::SliderFloat("exit timer (s)", &game_settings.exit_timer, 0.0f, 10.0f, "%.1f");
                
                if (ImGui::Button("dump players to console")) {
                    features::dump_players_to_console();
                }
            }
            ImGui::End();
        }

        if (enable_esp) {
            features::render_esp(localPlayer, esp_local_player, esp_players, esp_zombies, esp_vehicles, esp_barricades, esp_items, 
                esp_player_skeletons, esp_zombie_skeletons, esp_player_distance, esp_zombie_distance, esp_vehicle_distance, esp_barricade_distance, esp_item_distance, esp_danger_radius);
        }

        overlay.EndRender();

        features::apply_admin(localPlayer, enable_admin);
        features::apply_game_config(game_settings);
        features::apply_no_recoil(localPlayer, enable_no_recoil);
        features::apply_no_spread(localPlayer, enable_no_spread);


        
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        
        Sleep(1);
    }

    features::g_esp_thread_running = false;
    overlay.Destroy();
    CloseHandle(process_handle);
    return 0;
}

