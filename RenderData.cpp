
#include "RenderData.h"
#include "GameOfLife.h"


__int64 CPUCycles()
{
    LARGE_INTEGER Cycles;
    QueryPerformanceCounter(&Cycles);
    return Cycles.QuadPart;
}

URenderData RenderData;

URenderData& URenderData::Get()
{
    return RenderData;
}

bool URenderData::CreateD3DDevice(HWND hwnd)
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    CyclesPerSecond = Frequency.QuadPart;
    MillisecondsPerCycle = 1000.0 / (double)Frequency.QuadPart;
    CurrentSpeed = CyclesPerSecond >> 1;

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        return false;
    }

    return true;
}

void URenderData::Initialize(HWND hwnd)
{
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);
}

bool URenderData::IsMinimized()
{
    return bSwapChainOccluded && pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
}

bool URenderData::CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
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
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, &featureLevel, &pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, &featureLevel, &pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void URenderData::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (pSwapChain) { pSwapChain->Release(); pSwapChain = nullptr; }
    if (pd3dDeviceContext) { pd3dDeviceContext->Release(); pd3dDeviceContext = nullptr; }
    if (pd3dDevice) { pd3dDevice->Release(); pd3dDevice = nullptr; }
}

void URenderData::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
    pBackBuffer->Release();
}

void URenderData::CleanupRenderTarget()
{
    if (mainRenderTargetView)
    {
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr; 
    }
}

void URenderData::Cleanup()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI URenderData::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        Get().ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        Get().ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void URenderData::Present()
{
    const ImVec4 clear_color = ImVec4(0.0f, 0.f, 0.0f, 1.00f);

    const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
    pd3dDeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
    pd3dDeviceContext->ClearRenderTargetView(mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    HRESULT hr = pSwapChain->Present(1, 0);   // Present with vsync
    //HRESULT hr = pSwapChain->Present(0, 0); // Present without vsync
    bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

void URenderData::RenderHud(GameOfLife& TheSimulation)
{
    // Handle window resize (we don't resize directly in the WM_SIZE handler)
    if (ResizeWidth != 0 && ResizeHeight != 0)
    {
        CleanupRenderTarget();
        WindowSize.x = ResizeWidth - RenderData.WindowPos.x * 2.f;
        WindowSize.y = ResizeHeight - RenderData.WindowPos.y * 2.f;
        pSwapChain->ResizeBuffers(0, ResizeWidth, ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        ResizeWidth = ResizeHeight = 0;
        CreateRenderTarget();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        static float f = 0.0f;
        static int counter = 0;

        if (GoLState != EGoLStatus::Stopped)
        {
            bool bRenderGrid = GoLState != EGoLStatus::Stopped;

            ImGui::Begin("Grid", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);
            ImGui::SetWindowPos(WindowPos);
            ImGui::SetWindowSize(WindowSize);

            ImGui::BeginMainMenuBar();
            switch (GoLState)
            {
            case Stopped:
                break;
            case GridCreated:
                if (ImGui::Button("Fill Randomly"))
                {
                    TheSimulation.FillRandomly();
                }
                if (ImGui::Button("Start"))
                {
                    StartStatsSeconds = CPUCycles();
                    IndexTime = 0;
                    AverageTimeInMilliseconds = 0;
                    GoLState = EGoLStatus::Playing;
                }
                break;
            case Paused:
                if (ImGui::Button("Continue"))
                {
                    GoLState = EGoLStatus::Playing;
                }
                if (ImGui::Button("Stop"))
                {
                    GoLState = EGoLStatus::Stopped;
                }
                break;
            case Playing:
                ImGui::Text("%.4lf ms (%.0lf FPS)  Speed: ", AverageTimeInMilliseconds, 1000.0 / AverageTimeInMilliseconds);
                if (ImGui::Button("+"))
                {
                    CurrentSpeed -= (CurrentSpeed / 10);
                }
                if (ImGui::Button("-"))
                {
                    CurrentSpeed += (CurrentSpeed / 10);
                }
                if (ImGui::Button("Pause"))
                {
                    GoLState = EGoLStatus::Paused;
                }
                if (ImGui::Button("Stop"))
                {
                    GoLState = EGoLStatus::Stopped;
                }
                break;
            default:
                break;
            }
            ImGui::EndMainMenuBar();

            if (bRenderGrid)
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();

                ImVec2 ClipMin = DrawList->GetClipRectMin();
                ClipMin.x += 10.f;
                ClipMin.y += 10.f;
                ImVec2 ClipMax = DrawList->GetClipRectMax();
                ClipMax.x -= 10.f;
                ClipMax.y -= 10.f;

                ImVec2 WindowPos = ClipMin;
                const int NumColumns = TheSimulation.GetNumColumns();
                const int NumRows = TheSimulation.GetNumRows();
                const float CellWidth = (ClipMax.x - ClipMin.x) / NumColumns;
                const float CellHeight = (ClipMax.y - ClipMin.y) / NumRows;
                const float BorderSize = 0.f;
                for (int Column = 0; Column < NumColumns; ++Column)
                {
                    for (int Row = 0; Row < NumRows; ++Row)
                    {
                        const ImVec2 CellMin = ImVec2(WindowPos.x + Column * CellWidth, WindowPos.y + Row * CellHeight);
                        const ImVec2 CellMax = ImVec2(WindowPos.x + (Column + 1) * CellWidth - BorderSize, WindowPos.y + (Row + 1) * CellHeight - BorderSize);
                        DrawList->AddRectFilled(CellMin, CellMax, TheSimulation.GetCell(Row, Column) ? White : Black);
                    }
                }
            }

            ImGui::End();
        }
        else if (GoLState == EGoLStatus::Stopped)
        {
            ImGui::Begin("Grid Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::InputInt("Width", &GridWidth);
            GridWidth = GridWidth < 100 ? 100 : (GridWidth > MaxGridWidth) ? MaxGridWidth : GridWidth;
            ImGui::InputInt("Height", &GridHeight);
            GridHeight = GridHeight < 100 ? 100 : (GridHeight > MaxGridHeight) ? MaxGridHeight : GridHeight;
            if (ImGui::Button("Create Grid"))
            {
                TheSimulation.Initialize(GridHeight, GridWidth);
                GoLState = EGoLStatus::GridCreated;
            }

            ImGui::End();
        }
    }
}

void URenderData::Update(GameOfLife& TheSimulation)
{
    __int64 CurrentTime = CPUCycles();
    if ((CurrentTime - StartSpeedSeconds) > CurrentSpeed && GoLState == EGoLStatus::Playing)
    {
        StartSpeedSeconds = CurrentTime;

        __int64 Start = CPUCycles();

        TheSimulation.NextStep();

        __int64 End = CPUCycles();
        TimeInMilliseconds[IndexTime] = (End - Start) * MillisecondsPerCycle;
        IndexTime++;
        if (IndexTime >= TimeSize || (End - StartStatsSeconds) > CyclesPerSecond)
        {
            AverageTimeInMilliseconds = 0.;
            for (int I = 0; I < IndexTime; ++I)
            {
                AverageTimeInMilliseconds += TimeInMilliseconds[I];
            }
            AverageTimeInMilliseconds /= IndexTime;
            StartStatsSeconds = End;
            IndexTime = 0;
        }
    }
}
