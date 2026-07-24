// SyncClaudeCodeSetting 모듈 - ~/.claude 를 git 으로 동기화한다.
// Core 가 LoadLibrary + GetProcAddress 로 Module_Info/Init/Handle 을 바인딩해 구동한다.
//   요청: Module_Handle(reqJson)   예: {"module":"sync","cmd":"pull","arg":""}
//   응답: Module_Init 에서 받은 post 콜백으로 JSON 문자열 회신 (type: status/log/result)
// git 로직은 동기화 프로토타입에서 이식했고, WebView2/창 코드는 Core 가 전담한다.
// 미설정(git repo 아님) 감지 시 status.configured=false 로 알려 초기 설정(부트스트랩)을 유도한다.
#include <windows.h>

#include <string>
#include <vector>
#include <cstdio>
#include <filesystem>

#include "module_api.h"

namespace fs = std::filesystem;

static PostToUiFn   g_post = nullptr;
static std::wstring g_repoDir;   // 기본 %USERPROFILE%\.claude (테스트 시 CCSTUDIO_CLAUDE_DIR)

// ---------- string helpers ----------
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
static std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
static std::wstring EnvVar(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return L"";
    std::wstring v(n, L'\0');
    DWORD got = GetEnvironmentVariableW(name, v.data(), n);
    v.resize(got);
    return v;
}

// ---------- run a process, capture stdout+stderr (UTF-8) ----------
static DWORD RunCapture(const std::wstring& cmdline, const std::wstring& cwd, std::string& out) {
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
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code;
}
static DWORD Git(const std::wstring& args, std::string& out) {
    return RunCapture(L"git " + args, g_repoDir, out);
}

// ---------- JSON ----------
static std::string JsonEsc(const std::string& s) {
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
                else o += (char)c;
        }
    }
    return o;
}
static std::string JsonGet(const std::string& json, const std::string& key) {
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
            else { out += json[p]; ++p; }
        }
        return out;
    }
    size_t e = json.find_first_of(",}", p);
    return Trim(json.substr(p, (e == std::string::npos ? json.size() : e) - p));
}
static void PostToWeb(const std::string& json) {
    if (g_post) g_post(json.c_str());
}

// ---------- misc ----------
static std::string HostName() {
    wchar_t buf[256]; DWORD n = 256;
    if (GetComputerNameW(buf, &n)) return Narrow(std::wstring(buf, n));
    return "unknown";
}
static std::string NowStamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    char b[32];
    std::snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return b;
}
static std::string NowStampFile() {
    SYSTEMTIME st; GetLocalTime(&st);
    char b[24];
    std::snprintf(b, sizeof(b), "%04d%02d%02d-%02d%02d%02d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}
static void Result(bool ok, const std::string& msg, const std::string& detail) {
    std::string j = "{\"type\":\"result\",\"ok\":";
    j += ok ? "true" : "false";
    j += ",\"message\":\"" + JsonEsc(msg) + "\",\"detail\":\"" + JsonEsc(detail) + "\"}";
    PostToWeb(j);
}

// git 워킹트리(= 설정 완료) 여부.
static bool IsConfigured() {
    std::string out;
    DWORD code = Git(L"rev-parse --is-inside-work-tree", out);
    return code == 0 && Trim(out) == "true";
}

// ---------- commands ----------
static void CmdStatus() {
    if (!IsConfigured()) {
        PostToWeb("{\"type\":\"status\",\"configured\":false}");
        return;
    }
    std::string branch, dirty, remote, headHash, remoteHash, counts;
    Git(L"rev-parse --abbrev-ref HEAD", branch);      branch = Trim(branch);
    Git(L"status --porcelain", dirty);
    bool clean = Trim(dirty).empty();
    Git(L"remote get-url origin", remote);            remote = Trim(remote);
    Git(L"rev-parse --short HEAD", headHash);          headHash = Trim(headHash);
    Git(L"rev-parse --short origin/main", remoteHash);  remoteHash = Trim(remoteHash);

    int behind = 0, ahead = 0;
    if (Git(L"rev-list --left-right --count origin/main...HEAD", counts) == 0)
        std::sscanf(counts.c_str(), "%d %d", &behind, &ahead);

    std::string j = "{\"type\":\"status\",\"configured\":true,";
    j += "\"branch\":\""     + JsonEsc(branch)     + "\",";
    j += "\"clean\":"        + std::string(clean ? "true" : "false") + ",";
    j += "\"ahead\":"        + std::to_string(ahead)  + ",";
    j += "\"behind\":"       + std::to_string(behind) + ",";
    j += "\"remote\":\""     + JsonEsc(remote)     + "\",";
    j += "\"headHash\":\""   + JsonEsc(headHash)   + "\",";
    j += "\"remoteHash\":\"" + JsonEsc(remoteHash) + "\"}";
    PostToWeb(j);
}

static void CmdLog() {
    std::string headHash, remoteHash, raw;
    Git(L"rev-parse --short HEAD", headHash);           headHash = Trim(headHash);
    Git(L"rev-parse --short origin/main", remoteHash);   remoteHash = Trim(remoteHash);
    Git(L"log -8 --pretty=format:%h\x1f%cr\x1f%s\x1e", raw);

    std::string j = "{\"type\":\"log\",\"head\":\"" + JsonEsc(headHash) +
                    "\",\"remote\":\"" + JsonEsc(remoteHash) + "\",\"items\":[";
    bool first = true;
    size_t start = 0;
    while (start < raw.size()) {
        size_t end = raw.find('\x1e', start);
        std::string rec = raw.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = (end == std::string::npos) ? raw.size() : end + 1;
        rec = Trim(rec);
        if (rec.empty()) continue;

        std::string f[3];
        size_t p = 0;
        for (int k = 0; k < 3; ++k) {
            size_t q = rec.find('\x1f', p);
            f[k] = rec.substr(p, q == std::string::npos ? std::string::npos : q - p);
            if (q == std::string::npos) break;
            p = q + 1;
        }
        if (!first) j += ",";
        first = false;
        j += "{\"hash\":\""    + JsonEsc(f[0]) +
             "\",\"when\":\""  + JsonEsc(f[1]) +
             "\",\"subject\":\"" + JsonEsc(f[2]) + "\"}";
    }
    j += "]}";
    PostToWeb(j);
}

static void CmdFetch() {
    std::string out;
    DWORD code = Git(L"fetch origin", out);
    if (code != 0) Result(false, "원격에 접근할 수 없습니다 (오프라인/VPN 확인)", Trim(out));
    CmdStatus();
    CmdLog();
}

static void CmdPull() {
    std::string out;
    DWORD code = Git(L"pull --ff-only origin main", out);
    Result(code == 0, code == 0 ? "서버 변경을 가져왔습니다" : "가져오기 실패", Trim(out));
    CmdStatus();
    CmdLog();
}

static void CmdPush() {
    std::string dirty;
    Git(L"status --porcelain", dirty);
    if (!Trim(dirty).empty()) {
        std::string tmp;
        Git(L"add -A", tmp);
        std::string msg = "update from " + HostName() + " " + NowStamp();
        Git(L"commit -m \"" + Widen(msg) + L"\"", tmp);
    }
    std::string out;
    DWORD code = Git(L"push origin main", out);
    Result(code == 0, code == 0 ? "서버에 반영했습니다" : "반영 실패", Trim(out));
    CmdStatus();
    CmdLog();
}

static void CmdRevert(const std::string& hash) {
    Result(false, "되돌리기는 다음 버전에서 구현됩니다", "hash=" + hash);
}

// 서버 주소(origin URL) 변경. 이미 설정된 저장소의 remote 를 교체한다.
static void CmdSetRemote(const std::string& url) {
    std::string u = Trim(url);
    if (u.empty())      { Result(false, "서버 주소가 비어 있습니다", ""); return; }
    if (!IsConfigured()){ Result(false, "먼저 초기 설정이 필요합니다", ""); CmdStatus(); return; }
    std::string tmp;
    DWORD code = (Git(L"remote get-url origin", tmp) == 0)
        ? Git(L"remote set-url origin " + Widen(u), tmp)
        : Git(L"remote add origin "     + Widen(u), tmp);
    if (code != 0) { Result(false, "서버 주소 변경 실패", Trim(tmp)); CmdStatus(); return; }
    Result(true, "서버 주소를 변경했습니다", u);
    CmdStatus();
    CmdLog();
}

// 화이트리스트 항목이 있으면 .sync-backup-<시각>/ 으로 복사 (reset --hard 이전 안전망).
static void BackupExisting() {
    const wchar_t* items[] = { L"settings.json", L"CLAUDE.md", L"commands", L"hooks", L"output-styles" };
    std::error_code ec;
    fs::path backupDir = fs::path(g_repoDir) / (L".sync-backup-" + Widen(NowStampFile()));
    bool created = false;
    for (const wchar_t* it : items) {
        fs::path src = fs::path(g_repoDir) / it;
        if (!fs::exists(src, ec)) continue;
        if (!created) { fs::create_directories(backupDir, ec); created = true; }
        fs::copy(src, backupDir / it,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }
}

// 초기 설정: 미설정 ~/.claude 를 git 으로 부트스트랩해 origin/main 과 정렬.
static void CmdBootstrap(const std::string& url) {
    if (IsConfigured()) { Result(false, "이미 설정되어 있습니다", ""); CmdStatus(); CmdLog(); return; }

    std::string repoUrl = url.empty()
        ? "https://github.com/SleighMaster99/claude-code-settings.git" : url;
    std::string out, tmp;

    if (Git(L"init -b main", tmp) != 0) { Result(false, "저장소 초기화(git init) 실패", Trim(tmp)); return; }
    Git(L"remote remove origin", tmp);   // 잔여 origin 제거(있으면)
    if (Git(L"remote add origin " + Widen(repoUrl), tmp) != 0) { Result(false, "원격(origin) 추가 실패", Trim(tmp)); return; }
    if (Git(L"fetch origin", out) != 0) { Result(false, "원격에서 가져오기 실패 (저장소 URL/로그인 확인)", Trim(out)); return; }

    BackupExisting();

    if (Git(L"reset --hard origin/main", out) != 0) { Result(false, "서버 설정 정렬 실패", Trim(out)); return; }
    Git(L"branch --set-upstream-to=origin/main main", tmp);

    Result(true, "초기 설정 완료", "서버 설정을 가져왔습니다. 기존 설정은 .sync-backup 에 백업했습니다");
    CmdStatus();
    CmdLog();
}

// ---------- module exports ----------
MODULE_API const char* Module_Info() {
    return "{\"id\":\"sync\",\"tabTitle\":\"설정 동기화\",\"webEntry\":\"index.html\"}";
}
MODULE_API void Module_Init(PostToUiFn post) {
    g_post = post;
    std::wstring override = EnvVar(L"CCSTUDIO_CLAUDE_DIR");   // 테스트/검증용 repoDir 오버라이드
    g_repoDir = override.empty() ? (EnvVar(L"USERPROFILE") + L"\\.claude") : override;
}
MODULE_API void Module_Handle(const char* reqJson) {
    std::string req = reqJson ? reqJson : "";
    std::string cmd = JsonGet(req, "cmd");
    std::string arg = JsonGet(req, "arg");
    if      (cmd == "status")    CmdStatus();
    else if (cmd == "log")       CmdLog();
    else if (cmd == "refresh")   CmdFetch();
    else if (cmd == "pull")      CmdPull();
    else if (cmd == "push")      CmdPush();
    else if (cmd == "revert")    CmdRevert(arg);
    else if (cmd == "setRemote") CmdSetRemote(arg);
    else if (cmd == "bootstrap") CmdBootstrap(arg);
    else                         Result(false, "알 수 없는 명령", cmd);
}
