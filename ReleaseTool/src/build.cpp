// repo 위치 탐색 + 상태 조회 + Build.bat / gh 실행(출력 스트리밍).
// 실제 빌드·패키징은 Build.bat 이, 배포는 gh 가 한다 — 이 파일은 그것들을 띄우고 출력을 중계한다.
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cctype>

#include "releasetool.h"

namespace fs = std::filesystem;

// ---------- string / path helpers ----------
std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string Narrow(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
// 자식 출력의 인코딩은 도구마다 다르다 — MSBuild 는 UTF-8, ANSI 코드페이지로 쓰는 콘솔 도구도 있다.
// 유효한 UTF-8 이면 그대로 두고, 아니면 ANSI(한국어 Windows = 949)로 보고 옮긴다.
// (순수 ASCII 는 유효한 UTF-8 이라 그대로 통과한다 — Build.bat 자체 출력이 여기 해당)
static std::string ToUtf8(const std::string& raw) {
    if (raw.empty()) return "";
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            raw.data(), (int)raw.size(), nullptr, 0) > 0)
        return raw;
    int n = MultiByteToWideChar(CP_ACP, 0, raw.data(), (int)raw.size(), nullptr, 0);
    if (n <= 0) return raw;
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, raw.data(), (int)raw.size(), w.data(), n);
    return Narrow(w);
}
static std::wstring EnvVar(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return L"";
    std::wstring v(n, L'\0');
    DWORD got = GetEnvironmentVariableW(name, v.data(), n);
    v.resize(got);
    return v;
}
std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().wstring();
}
static std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// ---------- repo ----------
// exe 는 <repo>\bin\<Config>\ 에 있다. 두 단계 위가 repo 루트지만,
// 위치가 바뀌어도 버티게 .sln 이 보일 때까지 거슬러 올라간다.
std::wstring Repo_Root() {
    std::error_code ec;
    fs::path p = ExeDir();
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(p / L"ClaudeCodeStudio.sln", ec)) return p.wstring();
        if (!p.has_parent_path() || p == p.parent_path()) break;
        p = p.parent_path();
    }
    return L"";
}

std::string Repo_LatestVersion() {
    std::wstring root = Repo_Root();
    if (root.empty()) return "0.0.0";
    std::ifstream f(fs::path(root) / L"installer" / L"VERSION");
    if (!f) return "0.0.0";
    std::string line;
    std::getline(f, line);
    line = Trim(line);
    return line.empty() ? "0.0.0" : line;
}

std::string Build_SetupPath(const std::string& version) {
    std::wstring root = Repo_Root();
    if (root.empty()) return "";
    fs::path p = fs::path(root) / L"Shipping" / (L"ClaudeCodeStudio-Setup-" + Widen(version) + L".exe");
    return Narrow(p.wstring());
}
bool Build_SetupExists(const std::string& version) {
    std::string p = Build_SetupPath(version);
    if (p.empty()) return false;
    std::error_code ec;
    return fs::exists(Widen(p), ec);
}
unsigned long long Build_SetupSizeKb(const std::string& version) {
    std::string p = Build_SetupPath(version);
    if (p.empty()) return 0;
    std::error_code ec;
    auto n = fs::file_size(Widen(p), ec);
    if (ec) return 0;
    return (n + 1023) / 1024;
}

// ---------- 프로세스 실행 ----------
namespace {

// 출력을 모아서 돌려준다 (짧은 조회용 — git/gh status).
DWORD RunQuiet(const std::wstring& cmdline, const std::wstring& cwd, std::string& out) {
    out.clear();
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput  = wr;
    si.hStdError   = wr;

    std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
    cl.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return (DWORD)-1; }

    char buf[4096]; DWORD read = 0;
    while (ReadFile(rd, buf, sizeof(buf), &read, nullptr) && read > 0)
        out.append(buf, read);
    CloseHandle(rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

void PostLine(HWND hwnd, const std::string& utf8) {
    PostMessageW(hwnd, WM_APP_BUILD_LINE, 0,
                 reinterpret_cast<LPARAM>(new std::string(utf8)));
}

// 자식 stdout+stderr 를 파이프로 받아 줄이 완성될 때마다 UI 로 밀어 올린다.
// (전부 모았다가 한 번에 보내면 작업이 끝날 때까지 로그가 비어 보인다)
DWORD RunStreamed(HWND hwnd, const std::wstring& cmdline, const std::wstring& cwd) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput  = wr;
    si.hStdError   = wr;

    std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
    cl.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return (DWORD)-1; }

    std::string acc;
    char  buf[4096];
    DWORD read = 0;
    while (ReadFile(rd, buf, sizeof(buf), &read, nullptr) && read > 0) {
        acc.append(buf, read);
        size_t nl;
        while ((nl = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, nl);
            acc.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            PostLine(hwnd, ToUtf8(line));
        }
    }
    if (!acc.empty()) PostLine(hwnd, ToUtf8(acc));
    CloseHandle(rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

// UI 가 이미 검증하지만, 커맨드라인으로 흘러가는 값이라 여기서 한 번 더 막는다.
bool SafeVersion(const std::string& v) {
    if (v.empty() || v.size() > 32) return false;
    for (char c : v)
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.') return false;
    return true;
}

volatile LONG g_running = 0;

struct BuildJob { HWND hwnd; std::string version; bool skipBuild; };
struct PubJob   { HWND hwnd; std::string version; };

DWORD WINAPI BuildThread(LPVOID param) {
    std::unique_ptr<BuildJob> job(reinterpret_cast<BuildJob*>(param));
    DWORD code = (DWORD)-1;

    std::wstring root = Repo_Root();
    if (root.empty()) {
        PostLine(job->hwnd, "[ERROR] ClaudeCodeStudio.sln 을 찾지 못했습니다. "
                            "ReleaseTool.exe 가 repo 안에 있어야 합니다.");
    } else if (!SafeVersion(job->version)) {
        PostLine(job->hwnd, "[ERROR] 버전 형식이 올바르지 않습니다.");
    } else {
        std::wstring bat = (fs::path(root) / L"Build.bat").wstring();
        // cmd /s /c "<따옴표 포함 전체>" — 경로에 공백이 있어도 안전한 표준 형태.
        std::wstring cl = L"cmd.exe /s /c \"\"" + bat + L"\" " + Widen(job->version);
        if (job->skipBuild) cl += L" --skip-build";
        cl += L"\"";
        code = RunStreamed(job->hwnd, cl, root);
        if (code == (DWORD)-1)
            PostLine(job->hwnd, "[ERROR] Build.bat 을 실행하지 못했습니다.");
    }

    InterlockedExchange(&g_running, 0);
    PostMessageW(job->hwnd, WM_APP_BUILD_DONE, (WPARAM)code,
                 reinterpret_cast<LPARAM>(new std::string(job->version)));
    return 0;
}

// 배포는 끝났는데 Build.bat 이 올려 둔 installer\VERSION 이 아직 커밋되지 않았으면 알린다.
// 이 파일이 발행 기준선이라, 커밋을 빠뜨리면 다른 PC 의 clone 이 낡은 기준선을 들고
// 이미 배포한 버전을 다시 만들려 든다. 어느 브랜치에 올릴지는 사람이 정할 몫이라
// 커밋까지 대신하지는 않고, 할 일과 명령만 남긴다.
void WarnUncommittedVersion(HWND hwnd, const std::wstring& root) {
    std::string out;
    if (RunQuiet(L"git.exe status --porcelain -- installer/VERSION", root, out) != 0) return;
    if (Trim(out).empty()) return;
    PostLine(hwnd, "");
    PostLine(hwnd, "[TODO] installer\\VERSION 이 아직 커밋되지 않았습니다.");
    PostLine(hwnd, "       이 파일이 발행 기준선입니다. 커밋하지 않으면 다른 PC 에서");
    PostLine(hwnd, "       이미 배포한 버전을 다시 만들려는 시도가 통과합니다.");
    PostLine(hwnd, "       git add installer/VERSION");
    PostLine(hwnd, "       git commit -m \"chore: 발행 기준선 갱신\"");
}

DWORD WINAPI PublishThread(LPVOID param) {
    std::unique_ptr<PubJob> job(reinterpret_cast<PubJob*>(param));
    DWORD code = (DWORD)-1;

    std::wstring root = Repo_Root();
    std::string  setup = Build_SetupPath(job->version);

    if (root.empty()) {
        PostLine(job->hwnd, "[ERROR] repo 를 찾지 못했습니다.");
    } else if (!SafeVersion(job->version)) {
        PostLine(job->hwnd, "[ERROR] 버전 형식이 올바르지 않습니다.");
    } else if (!Build_SetupExists(job->version)) {
        PostLine(job->hwnd, "[ERROR] 설치파일이 없습니다: " + setup);
    } else {
        std::string tag = "v" + job->version;
        PostLine(job->hwnd, "> gh release create " + tag + " --generate-notes");
        PostLine(job->hwnd, "  " + setup);

        // gh 가 태그를 현재 커밋에 만들고 Release 를 생성한 뒤 자산을 올린다.
        // git tag 를 따로 치지 않는 이유: 로컬 태그만 만들면 원격에 없고, push 까지 하면
        // 실패 시 되돌릴 것이 둘로 늘어난다. gh 한 번이 원자적이다.
        std::wstring cl = L"gh.exe release create " + Widen(tag)
                        + L" \"" + Widen(setup) + L"\""
                        + L" --generate-notes"
                        + L" --title \"ClaudeCodeStudio " + Widen(job->version) + L"\"";
        code = RunStreamed(job->hwnd, cl, root);
        if (code == (DWORD)-1)
            PostLine(job->hwnd, "[ERROR] gh 를 실행하지 못했습니다. 설치와 PATH 를 확인하세요.");
        else if (code != 0)
            PostLine(job->hwnd, "[ERROR] 배포에 실패했습니다 (종료 코드 " + std::to_string(code) + ").");
        else
            WarnUncommittedVersion(job->hwnd, root);
    }

    InterlockedExchange(&g_running, 0);
    PostMessageW(job->hwnd, WM_APP_PUB_DONE, (WPARAM)code,
                 reinterpret_cast<LPARAM>(new std::string(job->version)));
    return 0;
}

// 원격 조회라 느릴 수 있어 창을 막지 않게 워커에서 돌린다. 실행 플래그는 잡지 않는다.
DWORD WINAPI StatusThread(LPVOID param) {
    std::unique_ptr<PubJob> job(reinterpret_cast<PubJob*>(param));
    std::string url;
    bool released = false;

    if (SafeVersion(job->version)) {
        std::wstring root = Repo_Root();
        std::string out;
        DWORD code = RunQuiet(L"gh.exe release view v" + Widen(job->version) + L" --json url -q .url",
                              root, out);
        out = Trim(out);
        // gh 는 릴리스가 없으면 0 이 아닌 코드로 끝난다.
        if (code == 0 && out.rfind("http", 0) == 0) { released = true; url = out; }
    }

    std::string json = std::string("{\"released\":") + (released ? "true" : "false")
                     + ",\"url\":\"" + JsonEscape(url) + "\""
                     + ",\"version\":\"" + JsonEscape(job->version) + "\"}";
    PostMessageW(job->hwnd, WM_APP_PUB_STATUS, 0,
                 reinterpret_cast<LPARAM>(new std::string(json)));
    return 0;
}

} // namespace

// ---------- 조회 ----------
std::string Repo_Branch() {
    std::wstring root = Repo_Root();
    if (root.empty()) return "";
    std::string out;
    if (RunQuiet(L"git.exe rev-parse --abbrev-ref HEAD", root, out) != 0) return "";
    return Trim(out);
}

// Build.bat 은 vswhere 로 MSBuild 를 찾는다. 여기서는 vswhere 존재만 확인한다
// (실제 MSBuild 탐색은 빌드 시점에 Build.bat 이 하고, 실패하면 로그로 알려준다).
bool Repo_HasMsBuild() {
    std::wstring pf = EnvVar(L"ProgramFiles(x86)");
    if (pf.empty()) return false;
    std::error_code ec;
    return fs::exists(pf + L"\\Microsoft Visual Studio\\Installer\\vswhere.exe", ec);
}

bool Repo_HasNsis() {
    std::error_code ec;
    wchar_t buf[MAX_PATH]{};
    DWORD cb = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\NSIS", nullptr,
                     RRF_RT_REG_SZ, nullptr, buf, &cb) == ERROR_SUCCESS) {
        if (fs::exists(std::wstring(buf) + L"\\makensis.exe", ec)) return true;
    }
    std::wstring pf = EnvVar(L"ProgramFiles(x86)");
    if (!pf.empty() && fs::exists(pf + L"\\NSIS\\makensis.exe", ec)) return true;
    return false;
}

// PATH 에서 gh.exe 를 찾는다 (프로세스를 띄우지 않아 시작이 느려지지 않는다).
bool Repo_HasGh() {
    wchar_t buf[MAX_PATH]{};
    return SearchPathW(nullptr, L"gh.exe", nullptr, MAX_PATH, buf, nullptr) > 0;
}

bool Job_Running() {
    return InterlockedCompareExchange(&g_running, 0, 0) != 0;
}

// ---------- 실행 ----------
void Build_Start(HWND hwnd, const std::string& version, bool skipBuild) {
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0) return;   // 이미 실행 중
    auto* job = new BuildJob{ hwnd, version, skipBuild };
    HANDLE h = CreateThread(nullptr, 0, BuildThread, job, 0, nullptr);
    if (h) CloseHandle(h);
    else { delete job; InterlockedExchange(&g_running, 0); }
}

void Publish_Start(HWND hwnd, const std::string& version) {
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0) return;
    auto* job = new PubJob{ hwnd, version };
    HANDLE h = CreateThread(nullptr, 0, PublishThread, job, 0, nullptr);
    if (h) CloseHandle(h);
    else { delete job; InterlockedExchange(&g_running, 0); }
}

void Publish_QueryStatus(HWND hwnd, const std::string& version) {
    auto* job = new PubJob{ hwnd, version };
    HANDLE h = CreateThread(nullptr, 0, StatusThread, job, 0, nullptr);
    if (h) CloseHandle(h);
    else delete job;
}
