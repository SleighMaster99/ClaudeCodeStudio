// Core - 창 + WebView2 호스트 + 좌측 탭 셸(web/) 서빙.
//
// exe(ClaudeCodeStudio)는 Core_Run() 하나만 호출한다. WebView2 컨트롤을 Win32
// 창에 얹고, web/ 폴더를 가상 호스트로 매핑해 index.html(탭 셸)을 띄운다.
// 기능 모듈(Sync/StatusBar) 로딩과 JS<->C++ 메시지 라우터는 P1~ 에서 이 위에 얹는다.
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>

#include <string>
#include <filesystem>

#include "core_api.h"

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2>           g_webview;
static HWND         g_hwnd   = nullptr;
static std::wstring g_webDir;   // <exe dir>\web

static const wchar_t* kVirtualHost = L"claudecodestudio.local";
static const wchar_t* kStartUrl    = L"https://claudecodestudio.local/index.html";

// ---------- environment helpers ----------
static std::wstring EnvVar(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return L"";
    std::wstring v(n, L'\0');
    DWORD got = GetEnvironmentVariableW(name, v.data(), n);
    v.resize(got);
    return v;
}
static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().wstring();
}

// ---------- window / webview ----------
static void ResizeToClient() {
    if (!g_controller) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    g_controller->put_Bounds(rc);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:    ResizeToClient(); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void InitWebView() {
    std::wstring userData = EnvVar(L"LOCALAPPDATA") + L"\\ClaudeCodeStudio\\WebView2";
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(r) || !env) {
                    MessageBoxW(g_hwnd,
                        L"WebView2 런타임을 초기화하지 못했습니다.\n"
                        L"Microsoft Edge WebView2 Runtime 설치를 확인하세요.",
                        L"오류", MB_ICONERROR);
                    return r;
                }
                env->CreateCoreWebView2Controller(g_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT r2, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(r2) || !ctrl) return r2;
                            g_controller = ctrl;
                            g_controller->get_CoreWebView2(&g_webview);

                            ComPtr<ICoreWebView2_3> wv3;
                            if (SUCCEEDED(g_webview.As(&wv3))) {
                                wv3->SetVirtualHostNameToFolderMapping(
                                    kVirtualHost, g_webDir.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                            }
                            ResizeToClient();
                            g_webview->Navigate(kStartUrl);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        MessageBoxW(g_hwnd, L"WebView2 환경 생성에 실패했습니다.", L"오류", MB_ICONERROR);
    }
}

extern "C" CORE_API int Core_Run(HINSTANCE hInst, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    g_webDir = ExeDir() + L"\\web";

    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"ClaudeCodeStudioWnd";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"ClaudeCodeStudio",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 980, 760,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    InitWebView();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_webview.Reset();
    g_controller.Reset();
    if (SUCCEEDED(co)) CoUninitialize();
    return (int)msg.wParam;
}
