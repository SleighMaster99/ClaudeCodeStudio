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
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
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

// settings.json 의 statusLine 값(객체)만 새 값으로 교체/삽입, 나머지 필드 보존.
static void ApplyStatusLine() {
    std::string rawCmd = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + Narrow(g_scriptPath) + "\"";
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
    if (!WriteFileUtf8(g_configPath, cfg)) { Result(false, "config.json 저장 실패", Narrow(g_configPath)); return; }
    ApplyStatusLine();
    Result(true, "저장 & 적용 완료", "다음 Claude Code 호출부터 반영됩니다");
}

// ---------- module exports ----------
MODULE_API const char* Module_Info() {
    return "{\"id\":\"statusbar\",\"tabTitle\":\"상태바 설정\",\"webEntry\":\"index.html\"}";
}
MODULE_API void Module_Init(PostToUiFn post) {
    g_post = post;
    g_root = FindRoot();
    g_configPath   = g_root + L"\\config.json";
    g_scriptPath   = g_root + L"\\StatusLine.ps1";
    g_settingsPath = EnvVar(L"USERPROFILE") + L"\\.claude\\settings.json";
}
MODULE_API void Module_Handle(const char* reqJson) {
    std::string req = reqJson ? reqJson : "";
    std::string cmd = JsonGetStr(req, "cmd");
    if      (cmd == "load")  CmdLoad();
    else if (cmd == "save")  CmdSave(req);
    else if (cmd == "apply") CmdApply(req);
    else                     Result(false, "알 수 없는 명령", cmd);
}
