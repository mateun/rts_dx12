
#include "appwindow.h"
#include "engine.h"
#include <windows.h>
#include <string>
#include <vector>
#include <directxtk/SimpleMath.h>


using namespace DirectX::SimpleMath;
Vector2 resizedDimension;
Vector2 tempDimension;

std::vector<Event*> frameEvents;

static bool inReSizing = false;

static LRESULT CALLBACK engineWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    switch (msg) 
    {
        case WM_SIZE:

            if (wParam == SIZE_MINIMIZED) break;

            tempDimension.x = LOWORD(lParam);
            tempDimension.y = HIWORD(lParam);

            
            
            break;
            
        case WM_ENTERSIZEMOVE:
            inReSizing = true;
            break;

        case WM_EXITSIZEMOVE:
            inReSizing = false;
            resizedDimension = tempDimension;   
            frameEvents.emplace_back(new Event{"resized", &resizedDimension});
            
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
    

}


Window createAppWindow(int width, int height, 
                    bool fullscreen)
{

    const std::wstring className = L"GameWindowClass";
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = engineWindowProc; 
    wc.hInstance = hInstance;
    wc.lpszClassName = className.c_str();
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    RECT clientRect = {0, 0, width, height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&clientRect, style, FALSE); // FALSE = no menu bar
    int adjustedWidth = clientRect.right - clientRect.left;
    int adjustedHeight = clientRect.bottom - clientRect.top;

    HWND hwnd = CreateWindowExW(
        0,
        className.c_str(),
        L"DX12App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, adjustedWidth, adjustedHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
	
	
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    
    return {width, height, hwnd};
}

std::vector<Event*> pollWindowMessages(Window window)
{
    
    frameEvents.clear();

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            frameEvents.emplace_back(new Event {"quit", nullptr});
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return frameEvents;
}