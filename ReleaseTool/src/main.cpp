// ReleaseTool - 릴리스 전용 독립 창 (버전 입력 -> 검증 -> Build.bat 실행 -> 로그).
//
// CCS 앱과 코드를 공유하지 않고 별도 exe 로 둔다. Core.dll 은 탭 셸 + 모듈 로더를
// 전제로 만들어져 있어서, 재사용하려면 Core 를 뜯어야 한다 — 잘 도는 앱을 건드리지
// 않으려고 창 + WebView2 만 여기서 새로 세운다.
//
// 배포 대상이 아니다: 인스톨러(installer\ClaudeCodeStudio.nsi)가 이 exe 와
// web\..\releasetool\ 을 언급하지 않으므로 설치본에는 들어가지 않는다.
//
//   JS -> C++ : postMessage(JSON) -> WebMessageReceived -> HandleWebMessage
//   C++ -> JS : Ui_Post(JSON) -> PostWebMessageAsString
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <string>
#include <memory>
#include <cstdio>

#include "releasetool.h"
#include "resource_ids.h"

using namespace Microsoft::WRL;

static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2>           g_webview;
static HWND         g_hwnd = nullptr;
static std::wstring g_webDir;

static const wchar_t* kVirtualHost = L"releasetool.local";
static const wchar_t* kStartUrl    = L"https://releasetool.local/index.html";
static const int      kWinW = 900, kWinH = 860;   // 배포 카드가 들어가 세로가 늘었다

// ---------- JSON ----------
std::string JsonEscape(const std::string& s) {
    std::string o; o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
                else          { o += (char)c; }
        }
    }
    return o;
}

// 값 하나만 뽑는 단순 파서 (cmd/version/skipBuild 용).
std::string JsonGetStr(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return "";
    p = json.find(':', p + pat.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    if (p < json.size() && json[p] == '"') {
        ++p;
        std::string out;
        while (p < json.size() && json[p] != '"') {
            if (json[p] == '\\' && p + 1 < json.size()) { out += json[p + 1]; p += 2; }
            else                                        { out += json[p]; ++p; }
        }
        return out;
    }
    size_t e = json.find_first_of(",}", p);
    return json.substr(p, (e == std::string::npos ? json.size() : e) - p);
}

void Ui_Post(const std::string& json) {
    if (!g_webview) return;
    g_webview->PostWebMessageAsString(Widen(json).c_str());
}

// ---------- 메시지 처리 ----------
// 마지막 발행 버전의 배포 준비 상태(설치파일 유무·크기·브랜치)를 담아 보낸다.
// 원격 조회(이미 배포됐는지)는 느려서 여기 넣지 않고, 별도 워커가 나중에 채운다.
static std::string PubFields(const std::string& ver) {
    return std::string(",\"setup\":")     + (Build_SetupExists(ver) ? "true" : "false")
         + ",\"setupPath\":\""            + JsonEscape(Build_SetupPath(ver)) + "\""
         + ",\"setupKb\":"                + std::to_string(Build_SetupSizeKb(ver))
         + ",\"branch\":\""               + JsonEscape(Repo_Branch()) + "\"";
}

static void SendInit() {
    std::string ver  = Repo_LatestVersion();
    std::wstring root = Repo_Root();
    std::string json =
        std::string("{\"type\":\"init\"")
        + ",\"latest\":\"" + JsonEscape(ver) + "\""
        + ",\"repo\":\""   + JsonEscape(Narrow(root)) + "\""
        + ",\"msbuild\":"  + (Repo_HasMsBuild() ? "true" : "false")
        + ",\"nsis\":"     + (Repo_HasNsis()    ? "true" : "false")
        + ",\"gh\":"       + (Repo_HasGh()      ? "true" : "false")
        + ",\"running\":"  + (Job_Running()     ? "true" : "false")
        + PubFields(ver)
        + "}";
    Ui_Post(json);

    // 이미 배포됐는지는 원격에 물어봐야 안다 — gh 가 있을 때만, 그리고 비동기로.
    if (Repo_HasGh() && Build_SetupExists(ver)) Publish_QueryStatus(g_hwnd, ver);
}

static void HandleWebMessage(const std::string& msg) {
    std::string cmd = JsonGetStr(msg, "cmd");
    if (cmd == "init") {
        SendInit();
    } else if (cmd == "build") {
        if (Job_Running()) return;
        Build_Start(g_hwnd,
                    JsonGetStr(msg, "version"),
                    JsonGetStr(msg, "skipBuild") == "true");
    } else if (cmd == "publish") {
        if (Job_Running()) return;
        std::string ver = JsonGetStr(msg, "version");
        // UI 가 잠가두지만, 브랜치 확인은 여기서도 한다 — 태그가 엉뚱한 커밋에 붙으면 되돌리기 어렵다.
        if (Repo_Branch() != "main") {
            Ui_Post("{\"type\":\"log\",\"line\":\"[ERROR] main 브랜치에서만 배포할 수 있습니다.\"}");
            Ui_Post("{\"type\":\"pubdone\",\"code\":1}");
            return;
        }
        Publish_Start(g_hwnd, ver);
    }
}

// ---------- window / webview ----------
static void ResizeToClient() {
    if (!g_controller) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    g_controller->put_Bounds(rc);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            ResizeToClient();
            return 0;

        // 워커 스레드가 올린 빌드 출력 한 줄.
        case WM_APP_BUILD_LINE: {
            std::unique_ptr<std::string> line(reinterpret_cast<std::string*>(lp));
            Ui_Post("{\"type\":\"log\",\"line\":\"" + JsonEscape(*line) + "\"}");
            return 0;
        }

        case WM_APP_BUILD_DONE: {
            std::unique_ptr<std::string> ver(reinterpret_cast<std::string*>(lp));
            int code = (int)wp;
            std::string json = "{\"type\":\"done\",\"code\":" + std::to_string(code);
            // 성공했으면 VERSION 이 갱신됐다 — 새 기준선과 배포 준비 상태를 다시 읽어 준다.
            if (code == 0) {
                std::string latest = Repo_LatestVersion();
                json += ",\"latest\":\"" + JsonEscape(latest) + "\"";
                json += PubFields(latest);
            }
            json += "}";
            Ui_Post(json);
            // 방금 만든 버전이 이미 배포돼 있는지 확인 (보통은 아니지만, 재생성한 경우가 있다).
            if (code == 0 && Repo_HasGh()) Publish_QueryStatus(g_hwnd, Repo_LatestVersion());
            return 0;
        }

        case WM_APP_PUB_DONE: {
            std::unique_ptr<std::string> ver(reinterpret_cast<std::string*>(lp));
            int code = (int)wp;
            Ui_Post("{\"type\":\"pubdone\",\"code\":" + std::to_string(code) + "}");
            // 성공했으면 릴리스 URL 을 받아와 카드에 링크를 건다.
            if (code == 0) Publish_QueryStatus(g_hwnd, *ver);
            return 0;
        }

        case WM_APP_PUB_STATUS: {
            std::unique_ptr<std::string> frag(reinterpret_cast<std::string*>(lp));
            Ui_Post("{\"type\":\"pubstatus\",\"data\":" + *frag + "}");
            return 0;
        }

        case WM_CLOSE:
            // 작업 중 창을 닫으면 자식 프로세스가 고아가 된다. 먼저 끝내게 한다.
            if (Job_Running()) {
                if (MessageBoxW(h, L"작업이 진행 중입니다. 그래도 닫을까요?",
                                L"ReleaseTool", MB_YESNO | MB_ICONWARNING) != IDYES)
                    return 0;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void InitWebView() {
    // 앱과 프로필을 섞지 않는다. CCS 상태 폴더 아래에 ReleaseTool 전용으로 둔다.
    wchar_t local[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    std::wstring userData = (n ? std::wstring(local) : ExeDir())
                          + L"\\ClaudeCodeStudio\\ReleaseTool\\WebView2";

    // 가상 호스트를 DNS 로 조회하려다 콜드 스타트가 늘어지는 것을 막는다 (Core 와 동일).
    auto envOptions = Make<CoreWebView2EnvironmentOptions>();
    envOptions->put_AdditionalBrowserArguments(
        L"--host-resolver-rules=\"MAP releasetool.local 127.0.0.1\"");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), envOptions.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(r) || !env) {
                    MessageBoxW(g_hwnd,
                        L"WebView2 런타임을 초기화하지 못했습니다.\n"
                        L"Microsoft Edge WebView2 Runtime 설치를 확인하세요.",
                        L"ReleaseTool", MB_ICONERROR);
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

                            EventRegistrationToken tok{};
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                            HandleWebMessage(Narrow(raw));
                                            CoTaskMemFree(raw);
                                        }
                                        return S_OK;
                                    }).Get(), &tok);

                            ResizeToClient();
                            g_webview->Navigate(kStartUrl);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr))
        MessageBoxW(g_hwnd, L"WebView2 환경 생성에 실패했습니다.", L"ReleaseTool", MB_ICONERROR);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 앱 셸(web\)과 섞이지 않게 자기 폴더를 따로 쓴다 — 인스톨러가 web\ 만 담기 때문에
    // 여기에 두면 설치본에 딸려 들어갈 일이 없다.
    g_webDir = ExeDir() + L"\\releasetool";

    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"ReleaseToolWnd";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // 앱 아이콘은 exe 리소스(ReleaseTool/app.rc)의 IDI_APP_ICON — 없으면 시스템 기본.
    wc.hIcon         = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm       = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"ReleaseTool",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, kWinW, kWinH,
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
