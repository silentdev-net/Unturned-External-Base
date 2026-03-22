#include "overlay.h"

namespace OverlayHijack
{
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    IDXGISwapChain* g_pSwapChain = nullptr;
    bool g_SwapChainOccluded = false;
    UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
    WNDPROC g_OriginalWndProc = nullptr;
    bool Vsync = false;
    HWND TargetOverlay = nullptr;

    void CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer;
        if (g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)) == S_OK)
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    }


    ImGuiIO& Hijack::GetPublicIo()
    {
        return ImGui::GetIO();
    }

    bool Hijack::Start()
    {
        OverlayHijack::TargetOverlay = FindWindowA(xorstr_("Chrome_WidgetWin_1"), xorstr_("Discord Overlay"));
        if (!OverlayHijack::TargetOverlay)
            return false;

        if (!CreateDeviceD3D(OverlayHijack::TargetOverlay))
        {
            return false;
        }

        g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(OverlayHijack::TargetOverlay, GWLP_WNDPROC, (LONG_PTR)WndProc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(OverlayHijack::TargetOverlay);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

        return true;
    }

    bool Hijack::Render()
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                return false;
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        return true;
    }

    void Hijack::EndRender()
    {
        ImGui::Render();

        float color[4] = { 0.f, 0.f, 0.f, 0.f };

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, color);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(OverlayHijack::Vsync ? 1 : 0, 0U);
    }

    void Hijack::Destroy()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_OriginalWndProc)
            SetWindowLongPtr(OverlayHijack::TargetOverlay, GWLP_WNDPROC, (LONG_PTR)g_OriginalWndProc);

        CleanupRenderTarget();
        if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    }

    bool Hijack::CreateDeviceD3D(HWND hWnd)
    {
        
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (res == DXGI_ERROR_UNSUPPORTED)
            res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

        if (res != S_OK)
            return false;

        CreateRenderTarget();
        return true;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        OverlayHijack::g_ResizeWidth = (UINT)LOWORD(lParam);
        OverlayHijack::g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    
    if (OverlayHijack::g_OriginalWndProc)
        return CallWindowProc(OverlayHijack::g_OriginalWndProc, hWnd, msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

