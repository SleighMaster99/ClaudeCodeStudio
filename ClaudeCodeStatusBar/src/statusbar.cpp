// ClaudeCodeStatusBar 모듈 - statusLine 레이아웃(config.json) 편집 + settings.json 적용.
// Core 가 LoadLibrary + GetProcAddress 로 구동. 카탈로그(항목 팔레트)는 web 이 소유하고,
// 이 백엔드는 config.json I/O 와 settings.json 의 statusLine 적용만 담당한다.
//   load  -> config.json 을 그대로 UI 로 전달
//   save  -> UI 의 config(rows) 를 config.json 에 기록
//   apply -> save + settings.json 의 statusLine 을 StatusLine.ps1 경로로 갱신(다른 필드 보존)
#include <windows.h>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "module_api.h"

namespace fs = std::filesystem;

static PostToUiFn   g_post = nullptr;
static std::wstring g_root;          // StatusLine.ps1 이 있는 폴더
static std::wstring g_configPath;    // g_root\config.json
static std::wstring g_scriptPath;    // g_root\StatusLine.ps1
static std::wstring g_settingsPath;  // ~/.claude/settings.json

// ---------- helpers ----------
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
// JSON 에서 문자열 값 하나 추출 (cmd 용).
static std::string JsonGetStr(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return "";
    p = json.find(':', p + pat.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) ++p;
    if (p < json.size() && json[p] == '"') {
        ++p; std::string out;
        while (p < json.size() && json[p] != '"') {
            if (json[p] == '\\' && p + 1 < json.size()) { out += json[p + 1]; p += 2; }
            else { out += json[p]; ++p; }
        }
        return out;
    }
    size_t e = json.find_first_of(",}", p);
    return json.substr(p, (e == std::string::npos ? json.size() : e) - p);
}
// JSON 에서 객체 값({...}) 하나를 통째로 추출 (config 용, 괄호/문자열 매칭).
static std::string JsonGetObject(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t p = json.find(pat);
    if (p == std::string::npos) return "";
    p = json.find(':', p + pat.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) ++p;
    if (p >= json.size() || json[p] != '{') return "";
    int depth = 0; bool inStr = false;
    for (size_t i = p; i < json.size(); ++i) {
        char c = json[i];
        if (inStr) { if (c == '\\') ++i; else if (c == '"') inStr = false; }
        else if (c == '"') inStr = true;
        else if (c == '{') ++depth;
        else if (c == '}') { --depth; if (depth == 0) return json.substr(p, i - p + 1); }
    }
    return "";
}

static std::string ReadFileUtf8(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss; ss << f.rdbuf();
    std::string s = ss.str();
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s.erase(0, 3);   // UTF-8 BOM 제거
    return s;
}
static bool WriteFileUtf8(const std::wstring& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(content.data(), (std::streamsize)content.size());
    return (bool)f;
}

static void PostToWeb(const std::string& json) { if (g_post) g_post(json.c_str()); }
static void Result(bool ok, const std::string& msg, const std::string& detail) {
    std::string j = "{\"type\":\"result\",\"ok\":";
    j += ok ? "true" : "false";
    j += ",\"message\":\"" + JsonEsc(msg) + "\",\"detail\":\"" + JsonEsc(detail) + "\"}";
    PostToWeb(j);
}

static std::wstring FindRoot() {
    // 이 dll(모듈) 자기 위치를 기준으로 상위에서 StatusLine.ps1 을 찾는다.
    // exe(호스트) 기준이 아니라 dll 기준이라, GUI 앱이든 단위 테스트든 동일하게 동작한다.
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&FindRoot), &self);
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(self, buf, MAX_PATH);
    fs::path dir = fs::path(buf).parent_path();
    for (int i = 0; i < 8; ++i) {
        std::error_code ec;
        if (fs::exists(dir / L"StatusLine.ps1", ec)) return dir.wstring();
        if (!dir.has_parent_path()) break;
        dir = dir.parent_path();
    }
    std::error_code ec;
    return fs::current_path(ec).wstring();
}

// ---------- commands ----------
static void CmdLoad() {
    std::string cfg = ReadFileUtf8(g_configPath);
    if (cfg.empty()) cfg = "{\"rows\":[]}";
    PostToWeb("{\"type\":\"config\",\"config\":" + cfg + "}");
}

static void CmdSave(const std::string& req) {
    std::string cfg = JsonGetObject(req, "config");
    if (cfg.empty()) { Result(false, "저장할 레이아웃이 없습니다", ""); return; }
    if (!WriteFileUtf8(g_configPath, cfg)) { Result(false, "config.json 저장 실패", Narrow(g_configPath)); return; }
    Result(true, "레이아웃을 저장했습니다", "");
}

// pwsh(PowerShell 7)가 PATH 에 있는지 확인 — 없는데 pwsh 로 적용하면 statusLine 이 조용히 죽는다.
static bool PwshAvailable() {
    wchar_t buf[MAX_PATH];
    return SearchPathW(nullptr, L"pwsh.exe", nullptr, MAX_PATH, buf, nullptr) > 0;
}

// StatusLine.ps1 경로를 statusLine command 인자로 변환한다.
// g_scriptPath 가 %USERPROFILE% 하위(= 설치본)면 ~ 기반 forward-slash 경로(PC 공통,
// Git Bash 가 ~ 확장 — D10 실측 확인)를 따옴표 없이, 아니면 절대경로를 따옴표로 반환.
static std::string ScriptArg() {
    std::wstring home = EnvVar(L"USERPROFILE");
    std::wstring p = g_scriptPath;
    if (!home.empty() && p.size() > home.size() &&
        _wcsnicmp(p.c_str(), home.c_str(), (int)home.size()) == 0) {
        std::wstring rel = L"~" + p.substr(home.size());   // ~\AppData\...\StatusLine.ps1
        for (auto& c : rel) if (c == L'\\') c = L'/';       // ~/AppData/.../StatusLine.ps1
        return Narrow(rel);                                 // 따옴표 없이(bash ~ 확장)
    }
    return "\"" + Narrow(p) + "\"";                         // 절대경로는 따옴표
}

// settings.json 의 statusLine 값(객체)만 새 값으로 교체/삽입, 나머지 필드 보존.
// shell: "pwsh" 면 PowerShell 7, 그 외는 Windows PowerShell 5 (config options.shell).
static void ApplyStatusLine(const std::string& shell) {
    std::string exe = (shell == "pwsh") ? "pwsh" : "powershell";
    std::string rawCmd = exe + " -NoProfile -ExecutionPolicy Bypass -File " + ScriptArg();
    std::string obj = "{ \"type\": \"command\", \"command\": \"" + JsonEsc(rawCmd) + "\" }";

    std::string s = ReadFileUtf8(g_settingsPath);
    if (s.empty()) { WriteFileUtf8(g_settingsPath, "{\n  \"statusLine\": " + obj + "\n}\n"); return; }

    size_t key = s.find("\"statusLine\"");
    if (key != std::string::npos) {
        size_t colon = s.find(':', key);
        if (colon != std::string::npos) {
            size_t vs = colon + 1;
            while (vs < s.size() && (s[vs] == ' ' || s[vs] == '\t' || s[vs] == '\n' || s[vs] == '\r')) ++vs;
            if (vs < s.size() && s[vs] == '{') {
                int depth = 0; bool inStr = false; size_t ve = s.size();
                for (size_t i = vs; i < s.size(); ++i) {
                    char c = s[i];
                    if (inStr) { if (c == '\\') ++i; else if (c == '"') inStr = false; }
                    else if (c == '"') inStr = true;
                    else if (c == '{') ++depth;
                    else if (c == '}') { --depth; if (depth == 0) { ve = i + 1; break; } }
                }
                s = s.substr(0, vs) + obj + s.substr(ve);
                WriteFileUtf8(g_settingsPath, s);
                return;
            }
        }
    }
    // statusLine 없음 → 최상위 여는 중괄호 뒤에 삽입
    size_t brace = s.find('{');
    if (brace == std::string::npos) { WriteFileUtf8(g_settingsPath, "{\n  \"statusLine\": " + obj + "\n}\n"); return; }
    s = s.substr(0, brace + 1) + "\n  \"statusLine\": " + obj + "," + s.substr(brace + 1);
    WriteFileUtf8(g_settingsPath, s);
}

static void CmdApply(const std::string& req) {
    std::string cfg = JsonGetObject(req, "config");
    if (cfg.empty()) { Result(false, "적용할 레이아웃이 없습니다", ""); return; }
    std::string shell = JsonGetStr(JsonGetObject(cfg, "options"), "shell");
    if (shell == "pwsh" && !PwshAvailable()) {
        Result(false, "pwsh(PowerShell 7)를 찾을 수 없습니다",
               "PowerShell 7 설치 후 다시 적용하거나 실행 셸을 powershell 로 바꾸세요");
        return;
    }
    if (!WriteFileUtf8(g_configPath, cfg)) { Result(false, "config.json 저장 실패", Narrow(g_configPath)); return; }
    ApplyStatusLine(shell);
    Result(true, "저장 & 적용 완료", "다음 Claude Code 호출부터 반영됩니다");
}

// ---------- module exports ----------
MODULE_API const char* Module_Info() {
    return "{\"id\":\"statusbar\",\"tabTitle\":\"상태바 설정\",\"webEntry\":\"index.html\"}";
}
MODULE_API void Module_Init(PostToUiFn post) {
    g_post = post;
    std::wstring ro = EnvVar(L"CCSTUDIO_STATUSBAR_ROOT");   // 검증용 루트(config.json/StatusLine.ps1 위치) 오버라이드
    g_root = ro.empty() ? FindRoot() : ro;
    g_configPath   = g_root + L"\\config.json";
    g_scriptPath   = g_root + L"\\StatusLine.ps1";
    std::wstring so = EnvVar(L"CCSTUDIO_SETTINGS_PATH");   // 검증용 settings 경로 오버라이드
    g_settingsPath = so.empty() ? (EnvVar(L"USERPROFILE") + L"\\.claude\\settings.json") : so;
}
MODULE_API void Module_Handle(const char* reqJson) {
    std::string req = reqJson ? reqJson : "";
    std::string cmd = JsonGetStr(req, "cmd");
    if      (cmd == "load")  CmdLoad();
    else if (cmd == "save")  CmdSave(req);
    else if (cmd == "apply") CmdApply(req);
    else                     Result(false, "알 수 없는 명령", cmd);
}
