#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

class URenderData
{
public:
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static URenderData& Get();
    bool CreateD3DDevice(HWND hwnd);
    void Cleanup();
    void Initialize(HWND hwnd);
    bool IsMinimized();
    void Present();
    void RenderHud(class GameOfLife& TheSimulation);
    void Update(class GameOfLife& TheSimulation);

protected:

    // Data
    ID3D11Device* pd3dDevice = nullptr;
    ID3D11DeviceContext* pd3dDeviceContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    bool bSwapChainOccluded = false;
    UINT ResizeWidth = 0;
    UINT ResizeHeight = 0;
    ID3D11RenderTargetView* mainRenderTargetView = nullptr;

    // Forward declarations of helper functions
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();

    enum EGoLStatus
    {
        Stopped,
        GridCreated,
        Paused,
        Playing
    };
    EGoLStatus GoLState = Stopped;

    static constexpr int MaxGridWidth = 4000;
    static  constexpr int MaxGridHeight = 2000;
    int GridWidth = 2000;
    int GridHeight = 1000;
    const ImU32 Black = ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
    const ImU32 White = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, 0.9f));
    ImVec2 WindowPos = {10.f, 20.f};
    ImVec2 WindowSize = {0.f, 0.f};

    static constexpr int TimeSize = 1000;
    int IndexTime = 0;
    double TimeInMilliseconds[TimeSize]{ 0. };
    double AverageTimeInMilliseconds = 0.;
    __int64 StartStatsSeconds = 0;
    __int64 StartSpeedSeconds = 0;
    __int64 CurrentSpeed = 0;
    __int64 CyclesPerSecond = 0;
    double MillisecondsPerCycle =0.0;
};

