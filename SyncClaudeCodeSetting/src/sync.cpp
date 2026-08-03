// SyncClaudeCodeSetting 모듈 - ~/.claude 를 git 으로 동기화한다.
// Core 가 LoadLibrary + GetProcAddress 로 Module_Info/Init/Handle 을 바인딩해 구동한다.
//   요청: Module_Handle(reqJson)   예: {"module":"sync","cmd":"pull","arg":""}
//   응답: Module_Init 에서 받은 post 콜백으로 JSON 문자열 회신 (type: status/log/result)
// git 로직은 동기화 프로토타입에서 이식했고, WebView2/창 코드는 Core 가 전담한다.
// 미설정(git repo 아님) 감지 시 status.configured=false 로 알려 초기 설정(부트스트랩)을 유도한다.
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <wincrypt.h>

#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

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
// stdin 으로 데이터를 넘겨 실행한다 (git credential 등록용). 출력은 버린다.
static bool RunWithInput(const std::wstring& cmdline, const std::wstring& cwd, const std::string& input) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput   = rd;

    std::vector<wchar_t> cl(cmdline.begin(), cmdline.end());
    cl.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(rd);
    if (!ok) { CloseHandle(wr); return false; }

    DWORD written = 0;
    WriteFile(wr, input.data(), (DWORD)input.size(), &written, nullptr);
    CloseHandle(wr);
    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// ---------- GitHub (OAuth Device Flow + REST) ----------
// gh CLI 에 기대지 않는다 — 앱이 직접 브라우저 인증을 진행하고 REST 로 저장소를 다룬다.
// Device Flow 는 client_secret 이 필요 없어 client_id 를 앱에 넣어도 안전하다(공개 정보).
static const char* kOAuthClientId = "Ov23liJWwSRV0A7hyy0H";

// WinHTTP 로 JSON 요청 한 번. 반환값은 HTTP 상태코드(0 = 연결 자체 실패).
static DWORD HttpJson(const wchar_t* verb, const wchar_t* host, const std::wstring& path,
                      const std::string& body, const std::string& bearer, std::string& out) {
    out.clear();
    HINTERNET ses = WinHttpOpen(L"ClaudeCodeStudio", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return 0;

    DWORD status = 0;
    if (HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0)) {
        HINTERNET req = WinHttpOpenRequest(con, verb, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (req) {
            // Accept: application/json 이 없으면 OAuth 엔드포인트가 form-encoded 로 답한다.
            std::wstring headers = L"Accept: application/json\r\nContent-Type: application/json\r\n";
            if (!bearer.empty()) headers += L"Authorization: Bearer " + Widen(bearer) + L"\r\n";

            BOOL sent = WinHttpSendRequest(req, headers.c_str(), (DWORD)-1,
                                           body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                           (DWORD)body.size(), (DWORD)body.size(), 0);
            if (sent && WinHttpReceiveResponse(req, nullptr)) {
                DWORD sz = sizeof(status);
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    if (!WinHttpReadData(req, chunk.data(), avail, &read)) break;
                    out.append(chunk.data(), read);
                }
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(con);
    }
    WinHttpCloseHandle(ses);
    return status;
}

// 토큰 파일. 앱 상태 폴더를 따르므로 CCSTUDIO_STATE_DIR 을 쓰는 검증 실행은 자동으로 격리된다.
static fs::path TokenPath() {
    std::wstring dir = EnvVar(L"CCSTUDIO_STATE_DIR");
    if (dir.empty()) dir = EnvVar(L"LOCALAPPDATA") + L"\\ClaudeCodeStudio";
    return fs::path(dir) / L"github_token.bin";
}

// DPAPI 로 이 사용자 계정에서만 풀리게 암호화해 둔다 — 평문으로 두지 않는다.
static bool SaveToken(const std::string& token) {
    DATA_BLOB in{ (DWORD)token.size(), (BYTE*)token.data() }, enc{};
    if (!CryptProtectData(&in, L"ClaudeCodeStudio GitHub token", nullptr, nullptr, nullptr, 0, &enc))
        return false;
    std::error_code ec;
    fs::create_directories(TokenPath().parent_path(), ec);
    std::ofstream f(TokenPath(), std::ios::binary | std::ios::trunc);
    bool ok = false;
    if (f) { f.write((const char*)enc.pbData, enc.cbData); ok = f.good(); }
    LocalFree(enc.pbData);
    return ok;
}

static std::string LoadToken() {
    std::ifstream f(TokenPath(), std::ios::binary);
    if (!f) return "";
    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (raw.empty()) return "";
    DATA_BLOB in{ (DWORD)raw.size(), (BYTE*)raw.data() }, dec{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &dec)) return "";
    std::string token((const char*)dec.pbData, dec.cbData);
    LocalFree(dec.pbData);
    return token;
}

// 이번 실행 동안의 토큰. 파일 저장이 실패해도 세션은 이어갈 수 있게 메모리에도 들고 있는다.
static std::string g_token;
static std::string CurrentToken() {
    if (g_token.empty()) g_token = LoadToken();
    return g_token;
}

// 토큰을 git 자격증명 저장소에 넣는다. 이게 없으면 저장소는 만들어져도 첫 push 에서 막힌다.
static void StoreGitCredential(const std::string& user, const std::string& token) {
    std::string input = "protocol=https\nhost=github.com\nusername=" + user +
                        "\npassword=" + token + "\n\n";
    RunWithInput(L"git credential approve", g_repoDir, input);
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

// 저장된 토큰으로 로그인 계정을 확인한다. 유효하지 않으면 빈 문자열.
static std::string AccountFromToken(const std::string& token) {
    if (token.empty()) return "";
    std::string body;
    if (HttpJson(L"GET", L"api.github.com", L"/user", "", token, body) != 200) return "";
    return JsonGet(body, "login");
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
    // 저장소가 아니면 가져올 원격도 없다. 초기 설정 화면에서 시작 시 확인이 돌 때
    // 실패 토스트가 뜨지 않도록 조용히 상태만 되돌린다.
    if (!IsConfigured()) { CmdStatus(); return; }
    std::string out;
    DWORD code = Git(L"fetch origin", out);
    if (code != 0) Result(false, "원격에 접근할 수 없습니다 (오프라인/VPN 확인)", Trim(out));
    CmdStatus();
    CmdLog();
}

static void BackupExisting();   // 덮어쓰기 전 안전망 — 정의는 아래 초기 설정 절에 있다

// "서버 → 이 PC 적용" — 이 PC 를 서버 상태로 맞춘다.
// 버릴 것이 없으면 앞으로 감고(fast-forward), 있으면 백업한 뒤 서버 상태로 정렬한다.
// 두 갈래를 한 명령이 맡는 이유는 버튼 이름이 약속하는 것이 하나이기 때문이다 —
// 사용자는 "서버 설정을 이 PC 에 적용" 을 원하지 커밋이 뒤처졌는지를 묻지 않는다.
static void CmdPull() {
    std::string out;
    // 판단은 최신 서버 상태를 기준으로 한다. 캐시된 origin/main 으로 정렬하면
    // 화면에 보이지 않던 서버 변경을 지나친 채 로컬만 버리게 된다.
    if (Git(L"fetch origin", out) != 0) {
        Result(false, "서버에 접속하지 못했습니다 (오프라인/VPN 확인)", Trim(out));
        CmdStatus();
        CmdLog();
        return;
    }

    // 서버에 맞추면 사라지는 것 = 미커밋 수정 + 이 PC 에만 있는 커밋.
    std::string dirty, counts;
    Git(L"status --porcelain", dirty);
    int behind = 0, ahead = 0;
    if (Git(L"rev-list --left-right --count origin/main...HEAD", counts) == 0)
        std::sscanf(counts.c_str(), "%d %d", &behind, &ahead);
    bool discard = !Trim(dirty).empty() || ahead > 0;

    DWORD code;
    if (discard) {
        BackupExisting();
        code = Git(L"reset --hard origin/main", out);
    } else {
        code = Git(L"pull --ff-only origin main", out);
    }

    if (code != 0) {
        Result(false, "서버 설정 적용 실패", Trim(out));
    } else if (discard) {
        Result(true, "서버 설정으로 맞췄습니다", "기존 설정은 .sync-backup 에 백업했습니다");
    } else {
        Result(true, "서버 변경을 가져왔습니다", Trim(out));
    }
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

// 초기 설정 화면이 쓸 정보: 로그인 여부와 계정, 인스톨러가 남긴 저장소 URL.
static void CmdGhInfo() {
    std::string account = AccountFromToken(CurrentToken());

    // repoDir 을 오버라이드한 검증 실행에서는 이 PC 의 설치 흔적을 끌어오지 않는다.
    std::string suggested = EnvVar(L"CCSTUDIO_CLAUDE_DIR").empty() ? RegRead(L"RepoUrl") : "";

    std::string j = "{\"type\":\"gh\",\"loggedIn\":";
    j += account.empty() ? "false" : "true";
    j += ",\"account\":\"" + JsonEsc(account) + "\"";
    j += ",\"suggestedRepo\":\"" + JsonEsc(suggested) + "\"}";
    PostToWeb(j);
}

// 승인을 기다리는 Device Flow 상태. 폴링은 웹이 주기적으로 ghPoll 을 보내 진행한다.
static std::string g_deviceCode;
static int         g_pollIntervalSec = 5;   // GitHub 이 응답으로 알려 주는 최소 간격
static ULONGLONG   g_nextPollAt = 0;        // 이 시각 전에는 GitHub 에 묻지 않는다

// 다음 질의 시각을 현재로부터 간격만큼 미룬다.
static void DeferNextPoll() {
    g_nextPollAt = GetTickCount64() + (ULONGLONG)g_pollIntervalSec * 1000;
}

// 브라우저 인증을 시작한다. 일회용 코드를 화면에 띄우고 브라우저를 연 뒤 곧바로 돌아온다.
// 승인될 때까지 여기서 기다리면 창이 멈추므로, 확인은 ghPoll 이 나눠 맡는다.
static void CmdGhLogin() {
    std::string body = std::string("{\"client_id\":\"") + kOAuthClientId + "\",\"scope\":\"repo\"}";
    std::string resp;
    DWORD st = HttpJson(L"POST", L"github.com", L"/login/device/code", body, "", resp);
    if (st != 200) {
        Result(false, "GitHub 에 연결하지 못했습니다", "네트워크를 확인하세요 (HTTP " + std::to_string(st) + ")");
        return;
    }

    g_deviceCode = JsonGet(resp, "device_code");
    std::string userCode = JsonGet(resp, "user_code");
    std::string uri      = JsonGet(resp, "verification_uri");
    if (g_deviceCode.empty() || userCode.empty()) {
        Result(false, "로그인 코드를 받지 못했습니다", Trim(resp).substr(0, 200));
        return;
    }
    if (uri.empty()) uri = "https://github.com/login/device";

    // 응답이 알려 준 최소 간격에 1초를 얹는다. 왕복 지연 때문에 서버가 재는 간격이
    // 우리가 잰 것보다 짧아질 수 있고, 그러면 곧바로 slow_down 으로 되돌아온다.
    int iv = std::atoi(JsonGet(resp, "interval").c_str());
    g_pollIntervalSec = (iv > 0 ? iv : 5) + 1;
    DeferNextPoll();

    ShellExecuteW(nullptr, L"open", Widen(uri).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    PostToWeb("{\"type\":\"ghLogin\",\"code\":\"" + JsonEsc(userCode) + "\"}");
}

// 승인됐는지 한 번만 확인한다. 아직이면 조용히 넘어간다 — 웹이 다시 부른다.
static void CmdGhPoll() {
    if (g_deviceCode.empty()) { CmdGhInfo(); return; }

    // 웹은 타이머로도, 창이 돌아올 때도 부른다. 최소 간격은 여기서 지킨다 —
    // 어기면 GitHub 이 slow_down 으로 되받고, 그 요청은 승인 여부를 알려주지 않는다.
    if (GetTickCount64() < g_nextPollAt) return;
    DeferNextPoll();

    std::string body = std::string("{\"client_id\":\"") + kOAuthClientId +
                       "\",\"device_code\":\"" + g_deviceCode +
                       "\",\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\"}";
    std::string resp;
    DWORD st = HttpJson(L"POST", L"github.com", L"/login/oauth/access_token", body, "", resp);
    if (st != 200) {
        // 일시적 네트워크 문제일 수 있어 로그인을 접지는 않는다. 다만 조용히 넘어가면
        // 화면이 멈춘 것처럼 보이므로 무슨 일인지 남긴다.
        PostToWeb("{\"type\":\"ghPending\",\"note\":\"HTTP " + std::to_string(st) + "\"}");
        return;
    }

    std::string token = JsonGet(resp, "access_token");
    if (token.empty()) {
        std::string err = JsonGet(resp, "error");
        if (err == "expired_token" || err == "access_denied") {
            // 더 기다려도 소용없다. 화면이 '다시 시도' 를 열도록 종류를 알려 준다.
            g_deviceCode.clear();
            PostToWeb("{\"type\":\"ghExpired\",\"reason\":\"" + JsonEsc(err) + "\"}");
            return;
        }
        if (err == "slow_down") {
            // 규약대로 간격을 늘린다. 응답이 새 값을 주면 그것을, 아니면 5초를 더한다.
            int iv = std::atoi(JsonGet(resp, "interval").c_str());
            g_pollIntervalSec = (iv > 0 ? iv : g_pollIntervalSec) + 5;
            DeferNextPoll();
        }
        // authorization_pending / slow_down 은 정상 흐름 — 폴링이 살아 있음을 알리고 기다린다.
        PostToWeb("{\"type\":\"ghPending\",\"note\":\"" + JsonEsc(err) + "\"}");
        return;
    }

    // 승인됐다. 여기서 조용히 실패하면 화면이 '로그인 필요' 로 머물러 원인을 알 수 없으므로
    // 각 단계의 실패를 그대로 드러낸다.
    g_deviceCode.clear();
    g_token = token;
    if (!SaveToken(token))
        PostToWeb("{\"type\":\"ghPending\",\"note\":\"토큰을 저장하지 못했습니다 (이번 실행에만 유지)\"}");

    std::string account = AccountFromToken(token);
    if (account.empty()) {
        Result(false, "로그인은 됐지만 계정을 읽지 못했습니다", "네트워크를 확인하고 ⟳ 를 누르세요");
        return;
    }
    StoreGitCredential(account, token);   // push 가 바로 되게
    CmdGhInfo();
}

// 저장소를 준비하고(없으면 생성) 그 URL 로 초기 설정까지 이어서 수행한다.
// 이미 있는 저장소면 만들지 않고 그대로 연결한다 — 두 번째 PC 에서 같은 화면을 쓰는 경우.
static void CmdCreateRepo(const std::string& req) {
    std::string owner = Trim(JsonGet(req, "owner"));
    std::string name  = Trim(JsonGet(req, "repo"));
    bool isPrivate = JsonGet(req, "private") != "false";
    if (owner.empty() || name.empty()) { Result(false, "계정명과 저장소 이름이 필요합니다", ""); return; }

    std::string token = CurrentToken();
    if (token.empty()) { Result(false, "먼저 GitHub 로그인이 필요합니다", ""); return; }

    std::string resp;
    std::wstring path = L"/repos/" + Widen(owner) + L"/" + Widen(name);
    if (HttpJson(L"GET", L"api.github.com", path, "", token, resp) != 200) {
        std::string body = "{\"name\":\"" + JsonEsc(name) + "\",\"private\":" +
                           (isPrivate ? "true" : "false") + "}";
        DWORD st = HttpJson(L"POST", L"api.github.com", L"/user/repos", body, token, resp);
        if (st != 201) {
            Result(false, "저장소 생성 실패", "HTTP " + std::to_string(st) + " " + Trim(resp).substr(0, 200));
            return;
        }
    }
    CmdBootstrap("https://github.com/" + owner + "/" + name + ".git");
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
    else if (cmd == "ghLogin")   CmdGhLogin();
    else if (cmd == "ghPoll")    CmdGhPoll();
    else if (cmd == "ghCancel")  { g_deviceCode.clear(); CmdGhInfo(); }
    else if (cmd == "createRepo") CmdCreateRepo(req);
    else                         Result(false, "알 수 없는 명령", cmd);
}
