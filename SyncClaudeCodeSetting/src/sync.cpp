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
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "module_api.h"

namespace fs = std::filesystem;

static PostToUiFn   g_post = nullptr;
static std::wstring g_repoDir;   // 기본 %USERPROFILE%\.claude (테스트 시 CCSTUDIO_CLAUDE_DIR)

// 셸 설정 탭 값 (configure 명령으로 갱신, 저장소는 웹 localStorage — 여기는 프로세스 생존 동안만).
// 저장소 URL 에는 기본값을 두지 않는다 — 빈 값이면 초기 설정을 거부한다.
// 특정 저장소를 미리 채워두면 다른 사용자가 무심코 남의 저장소로 부트스트랩할 수 있다.
static const char*  kDefaultCommitTpl = "update from {host} {time}";
static int          g_logCount    = 8;   // 이력 표시 개수 (1~50)
static std::string  g_commitTpl;         // 커밋 메시지 형식 (빈 값 = 기본)
static std::string  g_defaultRepo;       // 초기 설정 기본 저장소 URL (빈 값 = 없음)

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
// GitHub CLI. 미설치면 CreateProcess 자체가 실패해 (DWORD)-1 이 돌아온다 — 로그인 실패(exit 1)와 구분된다.
static DWORD Gh(const std::wstring& args, std::string& out) {
    return RunCapture(L"gh " + args, g_repoDir, out);
}

// 인스톨러가 남긴 값 읽기 (HKCU\Software\ClaudeCodeStudio). 없으면 빈 문자열.
static std::string RegRead(const wchar_t* name) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ClaudeCodeStudio", 0, KEY_READ, &k) != ERROR_SUCCESS)
        return "";
    wchar_t buf[1024];
    DWORD cb = sizeof(buf), type = 0;
    LSTATUS st = RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &cb);
    RegCloseKey(k);
    if (st != ERROR_SUCCESS || type != REG_SZ) return "";
    std::wstring w(buf, cb / sizeof(wchar_t));
    while (!w.empty() && w.back() == L'\0') w.pop_back();
    return Trim(Narrow(w));
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
static std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}
// 명령줄 인자 하나를 Windows 규칙(CommandLineToArgvW)대로 인용한다.
// 따옴표는 \" 로, 따옴표 직전의 백슬래시 연속은 두 배로 늘린다 — 이렇게 해야
// 값이 \ 로 끝나거나 " 를 포함해도 인자 경계가 깨지지 않는다.
static std::string QuoteArg(const std::string& s) {
    std::string out = "\"";
    size_t slashes = 0;
    for (char c : s) {
        if (c == '\\') { ++slashes; out += c; continue; }
        if (c == '"') { out.append(slashes + 1, '\\'); out += '"'; }
        else out += c;
        slashes = 0;
    }
    out.append(slashes, '\\');   // 닫는 따옴표 앞의 백슬래시도 두 배로
    out += '"';
    return out;
}
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
    std::string dirtyTrimmed = Trim(dirty);
    bool clean = dirtyTrimmed.empty();
    // 미커밋 변경 파일 수 (porcelain 한 줄 = 한 항목) — UI 가 이력 맨 위에 별도 행으로 표시한다.
    int dirtyCount = 0;
    if (!clean) {
        dirtyCount = 1;
        for (char c : dirtyTrimmed) if (c == '\n') ++dirtyCount;
    }
    Git(L"remote get-url origin", remote);            remote = Trim(remote);
    Git(L"rev-parse --short HEAD", headHash);          headHash = Trim(headHash);
    Git(L"rev-parse --short origin/main", remoteHash);  remoteHash = Trim(remoteHash);

    int behind = 0, ahead = 0;
    if (Git(L"rev-list --left-right --count origin/main...HEAD", counts) == 0)
        std::sscanf(counts.c_str(), "%d %d", &behind, &ahead);

    std::string j = "{\"type\":\"status\",\"configured\":true,";
    j += "\"branch\":\""     + JsonEsc(branch)     + "\",";
    j += "\"clean\":"        + std::string(clean ? "true" : "false") + ",";
    j += "\"dirtyCount\":"   + std::to_string(dirtyCount) + ",";
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
    Git(L"log -" + std::to_wstring(g_logCount) + L" --pretty=format:%h\x1f%cr\x1f%s\x1e", raw);

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
        // 커밋 메시지 형식({host}/{time} 치환). 인용은 QuoteArg 가 담당하므로
        // 값에 따옴표·백슬래시(예: 경로)가 들어가도 그대로 전달된다.
        std::string tpl = g_commitTpl.empty() ? kDefaultCommitTpl : g_commitTpl;
        std::string msg = ReplaceAll(ReplaceAll(tpl, "{host}", HostName()), "{time}", NowStamp());
        if (Trim(msg).empty()) msg = ReplaceAll(ReplaceAll(std::string(kDefaultCommitTpl), "{host}", HostName()), "{time}", NowStamp());
        Git(L"commit -m " + Widen(QuoteArg(msg)), tmp);
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
        ? Git(L"remote set-url origin " + Widen(QuoteArg(u)), tmp)
        : Git(L"remote add origin "     + Widen(QuoteArg(u)), tmp);
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

// 초기 설정이 중간에 실패했을 때, 이 실행에서 만든 .git 을 지워 원래 상태로 되돌린다.
// 롤백하지 않으면 ~/.claude 가 저장소로 남아 다음 실행에서 초기 설정 화면이 뜨지 않는다.
static void RollbackInit() {
    std::error_code ec;
    fs::remove_all(fs::path(g_repoDir) / L".git", ec);
}

// 화이트리스트 .gitignore — 없을 때만 만든다.
// 전부 무시한 뒤 PC 간에 옮겨 다녀야 할 설정만 다시 허용한다.
// 자격증명·세션 데이터·캐시가 서버로 올라가지 않게 하는 안전장치다.
static void WriteDefaultGitignore() {
    fs::path p = fs::path(g_repoDir) / L".gitignore";
    std::error_code ec;
    if (fs::exists(p, ec)) return;
    std::ofstream f(p, std::ios::trunc | std::ios::binary);
    if (!f) return;
    f << "# ~/.claude sync - whitelist model.\n"
         "# Ignore everything at the repo root, then re-include only the global config\n"
         "# meant to travel between machines. Anything not listed here - credentials,\n"
         "# session data, caches, machine-local settings - stays on this machine only.\n"
         "/*\n"
         "\n"
         "!/.gitignore\n"
         "!/CLAUDE.md\n"
         "!/settings.json\n"
         "!/commands/\n"
         "!/hooks/\n"
         "!/output-styles/\n"
         "\n"
         "# Backups and compiled caches are never tracked, even inside the folders above.\n"
         "*.bak\n"
         "*.orig\n"
         "__pycache__/\n"
         "*.pyc\n";
}

// 초기 설정: 미설정 ~/.claude 를 서버 저장소와 연결한다.
// 서버에 main 이 있으면 그것으로 정렬하고, 빈 저장소면 이 PC 설정을 첫 커밋으로 올린다.
static void CmdBootstrap(const std::string& url) {
    if (IsConfigured()) { Result(false, "이미 설정되어 있습니다", ""); CmdStatus(); CmdLog(); return; }

    std::string repoUrl = Trim(url.empty() ? g_defaultRepo : url);
    if (repoUrl.empty()) { Result(false, "저장소 URL 을 입력하세요", ""); return; }

    std::error_code ec;
    bool hadGit = fs::exists(fs::path(g_repoDir) / L".git", ec);   // 우리가 만든 .git 만 롤백 대상
    std::string out, tmp;

    if (Git(L"init -b main", tmp) != 0) { Result(false, "저장소 초기화(git init) 실패", Trim(tmp)); return; }
    Git(L"remote remove origin", tmp);   // 잔여 origin 제거(있으면)
    if (Git(L"remote add origin " + Widen(QuoteArg(repoUrl)), tmp) != 0) {
        if (!hadGit) RollbackInit();
        Result(false, "원격(origin) 추가 실패", Trim(tmp));
        return;
    }
    if (Git(L"fetch origin", out) != 0) {
        if (!hadGit) RollbackInit();
        Result(false, "원격에서 가져오기 실패 (저장소 URL/로그인 확인)", Trim(out));
        return;
    }

    // 서버에 main 이 없다 = 방금 만든 빈 저장소. 이 PC 설정을 첫 커밋으로 올린다.
    if (Git(L"rev-parse --verify origin/main", tmp) != 0) {
        WriteDefaultGitignore();
        Git(L"add -A", tmp);
        Git(L"commit -m " + Widen(QuoteArg("initial sync from " + HostName())), tmp);
        if (Git(L"push -u origin main", out) != 0) {
            Result(false, "첫 반영 실패 (권한 확인 후 '이 PC → 서버 반영' 으로 재시도)", Trim(out));
            CmdStatus();
            CmdLog();
            return;
        }
        Result(true, "초기 설정 완료", "이 PC 의 설정을 서버에 올렸습니다");
        CmdStatus();
        CmdLog();
        return;
    }

    BackupExisting();

    if (Git(L"reset --hard origin/main", out) != 0) { Result(false, "서버 설정 정렬 실패", Trim(out)); return; }
    Git(L"branch --set-upstream-to=origin/main main", tmp);

    Result(true, "초기 설정 완료", "서버 설정을 가져왔습니다. 기존 설정은 .sync-backup 에 백업했습니다");
    CmdStatus();
    CmdLog();
}

// 초기 설정 화면이 쓸 환경 정보: gh 설치/로그인 여부, 로그인 계정, 인스톨러가 남긴 저장소 URL.
static void CmdGhInfo() {
    std::string out;
    DWORD code = Gh(L"api user --jq .login", out);
    bool installed = (code != (DWORD)-1);
    std::string account = Trim(out);
    bool loggedIn = installed && code == 0 && !account.empty();

    // repoDir 을 오버라이드한 검증 실행에서는 이 PC 의 설치 흔적을 끌어오지 않는다.
    std::string suggested = EnvVar(L"CCSTUDIO_CLAUDE_DIR").empty() ? RegRead(L"RepoUrl") : "";

    std::string j = "{\"type\":\"gh\",\"installed\":";
    j += installed ? "true" : "false";
    j += ",\"loggedIn\":";
    j += loggedIn ? "true" : "false";
    j += ",\"account\":\"" + JsonEsc(loggedIn ? account : "") + "\"";
    j += ",\"suggestedRepo\":\"" + JsonEsc(suggested) + "\"}";
    PostToWeb(j);
}

// GitHub 저장소를 준비하고(없으면 생성) 그 URL 로 초기 설정까지 이어서 수행한다.
// 이미 있는 저장소면 만들지 않고 그대로 연결한다 — 두 번째 PC 에서 같은 화면을 쓰는 경우.
static void CmdCreateRepo(const std::string& req) {
    std::string owner = Trim(JsonGet(req, "owner"));
    std::string name  = Trim(JsonGet(req, "repo"));
    bool isPrivate = JsonGet(req, "private") != "false";
    if (owner.empty() || name.empty()) { Result(false, "계정명과 저장소 이름이 필요합니다", ""); return; }

    std::string slug = owner + "/" + name, out;
    if (Gh(L"repo view " + Widen(QuoteArg(slug)), out) != 0) {
        std::wstring vis = isPrivate ? L" --private" : L" --public";
        if (Gh(L"repo create " + Widen(QuoteArg(slug)) + vis, out) != 0) {
            Result(false, "저장소 생성 실패 (gh 로그인/권한 확인)", Trim(out));
            return;
        }
    }
    CmdBootstrap("https://github.com/" + slug + ".git");
}

// 셸 설정 탭 값 수신. 응답 없음 — 이후 명령부터 반영된다.
static void CmdConfigure(const std::string& req) {
    std::string n = JsonGet(req, "logCount");
    int v = std::atoi(n.c_str());
    g_logCount = (v >= 1 && v <= 50) ? v : 8;
    g_commitTpl = JsonGet(req, "commitMsg");
    g_defaultRepo = Trim(JsonGet(req, "defaultRepo"));
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
    else if (cmd == "configure") CmdConfigure(req);
    else if (cmd == "log")       CmdLog();
    else if (cmd == "refresh")   CmdFetch();
    else if (cmd == "pull")      CmdPull();
    else if (cmd == "push")      CmdPush();
    else if (cmd == "revert")    CmdRevert(arg);
    else if (cmd == "setRemote") CmdSetRemote(arg);
    else if (cmd == "bootstrap") CmdBootstrap(arg);
    else if (cmd == "ghInfo")    CmdGhInfo();
    else if (cmd == "createRepo") CmdCreateRepo(req);
    else                         Result(false, "알 수 없는 명령", cmd);
}
