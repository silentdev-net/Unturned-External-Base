#pragma once
#include <windows.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"

#ifndef xorstr_
#define xorstr_(str) str
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace OverlayHijack
{
    extern ID3D11Device* g_pd3dDevice;
    extern ID3D11DeviceContext* g_pd3dDeviceContext;
    extern IDXGISwapChain* g_pSwapChain;
    extern bool g_SwapChainOccluded;
    extern UINT g_ResizeWidth, g_ResizeHeight;
    extern ID3D11RenderTargetView* g_mainRenderTargetView;
    extern WNDPROC g_OriginalWndProc;
    extern bool Vsync;
    extern HWND TargetOverlay;

    void CreateRenderTarget();
    void CleanupRenderTarget();

    class Hijack final
    {
    public:
        ImGuiIO& GetPublicIo();
        bool Start();
        bool Render();
        void EndRender();
        void Destroy();
        bool CreateDeviceD3D(HWND hWnd);
    };
}

