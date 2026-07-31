// Core - 창 + WebView2 호스트 + 좌측 탭 셸(web/) 서빙 + 모듈 로딩/메시지 중계.
// exe(ClaudeCodeStudio)는 Core_Run() 하나만 호출한다.
//   JS(부모 셸) -> C++ : postMessage(JSON 문자열) -> WebMessageReceived -> Router_Handle
//   C++ -> JS         : Host_PostToUi(JSON) -> PostWebMessageAsString -> 부모 셸이 iframe 으로 중계
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <string>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <set>

#include "core_api.h"
#include "core_internal.h"
#include "resource_ids.h"

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2>           g_webview;
static HWND         g_hwnd = nullptr;
static std::wstring g_webDir;   // <exe dir>\web

static const wchar_t* kVirtualHost = L"claudecodestudio.local";
static const wchar_t* kStartUrl    = L"https://claudecodestudio.local/index.html";

// ---------- string / env helpers ----------
static std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
static std::string Narrow(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
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

// ---------- app state dir / window size ----------
// 앱 로컬 상태(창 크기, WebView2 프로필) 저장 위치.
// 기본 %LOCALAPPDATA%\ClaudeCodeStudio, 테스트 격리는 CCSTUDIO_STATE_DIR 오버라이드.
static std::wstring StateDir() {
    std::wstring o = EnvVar(L"CCSTUDIO_STATE_DIR");
    return o.empty() ? EnvVar(L"LOCALAPPDATA") + L"\\ClaudeCodeStudio" : o;
}
static std::wstring SizeFilePath() { return StateDir() + L"\\window_size.txt"; }

static const int kDefaultW = 1920, kDefaultH = 1080;

// "1600x900" 형식 파싱. 형식/범위 벗어나면 false.
static bool ParseSize(const std::string& s, int& w, int& h) {
    int pw = 0, ph = 0;
    if (std::sscanf(s.c_str(), "%dx%d", &pw, &ph) != 2) return false;
    if (pw < 800 || pw > 8192 || ph < 600 || ph > 8192) return false;
    w = pw; h = ph;
    return true;
}
// 저장된 창 크기(설정 탭 해상도 선택값). 없거나 손상이면 기본값.
static void LoadSavedSize(int& w, int& h) {
    w = kDefaultW; h = kDefaultH;
    std::ifstream f(SizeFilePath());
    if (!f) return;
    std::string line;
    std::getline(f, line);
    ParseSize(line, w, h);
}
static void SaveSize(int w, int h) {
    std::error_code ec;
    fs::create_directories(StateDir(), ec);
    std::ofstream f(SizeFilePath(), std::ios::trunc);
    if (f) f << w << "x" << h;
}

// 창 크기를 작업영역 안으로 줄인다 (저장값 자체는 바꾸지 않는다).
static void ClampToRect(const RECT& work, int& w, int& h) {
    int aw = work.right - work.left, ah = work.bottom - work.top;
    if (aw > 0 && w > aw) w = aw;
    if (ah > 0 && h > ah) h = ah;
}
// 창이 아직 없는 시작 시점 — 주 모니터 작업영역 기준으로 줄인다.
// (창 생성 위치가 CW_USEDEFAULT 라 주 모니터가 기준이 된다)
static void ClampToPrimaryWorkArea(int& w, int& h) {
    RECT work{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) ClampToRect(work, w, h);
}

// 설정 탭의 해상도 선택 → 창 크기 즉시 변경 + 저장(다음 실행 초기 크기).
// 저장은 선택값 그대로, 화면 적용만 현재 모니터 작업영역에 맞춰 줄인다.
static void ResizeWindowTo(int w, int h) {
    if (!g_hwnd) return;
    int cw = w, ch = h;
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        ClampToRect(mi.rcWork, cw, ch);
    if (IsZoomed(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
    SetWindowPos(g_hwnd, nullptr, 0, 0, cw, ch, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SaveSize(w, h);
}

// router 가 넘긴 "core" 모듈 명령 처리 (모듈 DLL 이 아닌 Core 자체 명령).
void Host_HandleCoreCmd(const std::string& jsonMsg) {
    std::string cmd = JsonGetStr(jsonMsg, "cmd");
    if (cmd == "resize") {
        int w = 0, h = 0;
        if (ParseSize(JsonGetStr(jsonMsg, "arg"), w, h)) ResizeWindowTo(w, h);
    }
}

// ---------- system fonts ----------
static std::wstring JsonEscapeW(const std::wstring& s) {
    std::wstring o; o.reserve(s.size() + 2);
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') { o += L'\\'; o += c; }
        else if (c >= 0x20) { o += c; }   // 제어문자는 버림
    }
    return o;
}
static int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    auto* names = reinterpret_cast<std::set<std::wstring>*>(lParam);
    if (lf && lf->lfFaceName[0] && lf->lfFaceName[0] != L'@')   // @ = 세로쓰기 폰트, 제외
        names->insert(lf->lfFaceName);
    return 1;
}
// 설치된 폰트 패밀리를 JSON 배열 문자열로 만든다 (정렬 + 중복 제거).
static std::wstring EnumSystemFontsJson() {
    std::set<std::wstring> names;
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        LOGFONTW lf{}; lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(hdc, &lf, EnumFontProc, reinterpret_cast<LPARAM>(&names), 0);
        ReleaseDC(nullptr, hdc);
    }
    std::wstring json = L"[";
    bool first = true;
    for (const auto& n : names) {
        if (!first) json += L",";
        first = false;
        json += L"\"" + JsonEscapeW(n) + L"\"";
    }
    json += L"]";
    return json;
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

// 모듈이 부르는 post 콜백 — 결과 JSON 을 부모 셸(JS)로 전달.
void Host_PostToUi(const char* json) {
    if (!g_webview || !json) return;
    // 현재 모듈 id 를 봉투로 씌운다 → 부모 셸이 env.module 로 iframe 을 골라 중계.
    std::string env = "{\"module\":\"" + g_currentModule + "\",\"payload\":" + json + "}";
    g_webview->PostWebMessageAsString(Widen(env).c_str());
}

static void InitWebView() {
    std::wstring userData = StateDir() + L"\\WebView2";

    // 가상 호스트 이름(claudecodestudio.local)을 로컬로 즉시 해석시킨다.
    // WebView2 가 이 이름을 DNS/mDNS 로 조회하려다 콜드 스타트가 지연되는 것을 예방(가상 호스트라 실제 통신은 없음).
    auto envOptions = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    envOptions->put_AdditionalBrowserArguments(
        L"--host-resolver-rules=\"MAP claudecodestudio.local 127.0.0.1\"");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), envOptions.Get(),
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

                            // JS(부모 셸) → C++ : JSON 문자열 수신 → 라우터로 분배
                            EventRegistrationToken tok{};
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                            Router_Handle(Narrow(raw));
                                            CoTaskMemFree(raw);
                                        }
                                        return S_OK;
                                    }).Get(), &tok);

                            // 모듈 로드 + 각 모듈에 UI post 콜백 전달
                            Modules_LoadAll(Host_PostToUi);

                            // 설치된 시스템 폰트 목록 + 현재 창 크기 설정값을 문서 생성 시 주입한다
                            // (설정 탭의 글꼴/창 크기 드롭다운이 읽는다). 등록 완료 후 내비게이트.
                            int sw = 0, sh = 0;
                            LoadSavedSize(sw, sh);
                            std::wstring fontsJs = L"window.__ccsFonts=" + EnumSystemFontsJson() + L";"
                                + L"window.__ccsWinSize=\"" + std::to_wstring(sw) + L"x" + std::to_wstring(sh) + L"\";";
                            ResizeToClient();
                            g_webview->AddScriptToExecuteOnDocumentCreated(fontsJs.c_str(),
                                Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
                                    [](HRESULT, LPCWSTR) -> HRESULT {
                                        if (g_webview) g_webview->Navigate(kStartUrl);
                                        return S_OK;
                                    }).Get());
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
    // 앱 아이콘은 exe 리소스(ClaudeCodeStudio/app.rc)에 있다 — hInst 가 exe 모듈 핸들.
    // 리소스가 없으면 null 로 남아 시스템 기본 아이콘이 쓰인다.
    wc.hIcon         = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm       = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);

    int winW = 0, winH = 0;
    LoadSavedSize(winW, winH);        // 설정 탭 해상도 선택값 (없으면 1920x1080)
    ClampToPrimaryWorkArea(winW, winH);   // 더 작은 화면으로 옮겨도 창이 화면 밖으로 나가지 않게
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"ClaudeCodeStudio",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
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
