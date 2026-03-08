// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "profileapi.h"
#include <d3d11.h>
#include <tchar.h>

#include "GameOfLife.h"

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

__int64 Cycles()
{
	LARGE_INTEGER Cycles;
	QueryPerformanceCounter(&Cycles);
	return Cycles.QuadPart;
}

// Main code
int main(int, char**)
{
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	__int64 CyclesPerSecond = Frequency.QuadPart;
	double MillisecondsPerCycle = 1000.0 / (double)Frequency.QuadPart;

    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Game Of Life", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Game Of Life", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.0f, 0.f, 0.0f, 1.00f);

	ImVec2 WindowPos(10.f, 20.f);
    ImVec2 WindowSize(0.f, 0.f);

    enum EGoLStatus
    {
        Stopped,
		GridCreated,
        Paused,
        Playing
    };
    EGoLStatus GoLState = Stopped;
    int GridWidth = 200;
	int GridHeight = 100;
	constexpr int MaxGridWidth = 2000;
	constexpr int MaxGridHeight = 1000;
    const ImU32 Black = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
	const ImU32 White = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.9f));
    int IndexTime = 0;
    constexpr int TimeSize = 1000;
	double TimeInMilliseconds[TimeSize] { 0.};
    double AverageTimeInMilliseconds = 0.;
    __int64 StartStatsSeconds = 0;
	__int64 StartSpeedSeconds = 0;
    __int64 CurrentSpeed = CyclesPerSecond >> 1;
    GameOfLife TheSimulation;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
        {
            break;
        }

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
			WindowSize.x = g_ResizeWidth - WindowPos.x * 2.f;
			WindowSize.y = g_ResizeHeight - WindowPos.y * 2.f;
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
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
						StartStatsSeconds = Cycles();
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
					        DrawList->AddRectFilled(CellMin, CellMax, TheSimulation.GetCell(Row, Column) ? Black : White);
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

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

		__int64 CurrentTime = Cycles();
		if ((CurrentTime - StartSpeedSeconds) > CurrentSpeed && GoLState == EGoLStatus::Playing)
		{
            StartSpeedSeconds = CurrentTime;

            __int64 Start = Cycles();

			TheSimulation.NextStep();

            __int64 End = Cycles();
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

			constexpr int TimeSize = 100;
			double TimeInMilliseconds[TimeSize]{ 0. };
        }
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
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
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
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
