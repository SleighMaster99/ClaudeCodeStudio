// 모듈 로더 + 레지스트리.
// D11: "암시적" 로딩 = 로드할 DLL 목록을 코드에 하드코딩하고 시작 시 자동 로드.
// (향후 진짜 플러그인이 필요하면 modules/ 폴더 스캔으로 전환 — 경계가 C 문자열이라 용이.)
#include "core_internal.h"

static std::vector<Module> g_modules;

// 로드 대상 모듈 DLL (exe 와 같은 폴더). 새 모듈 추가 시 여기에 등록.
static const wchar_t* kModuleDlls[] = {
    L"SyncClaudeCodeSetting.dll",
    L"ClaudeCodeStatusBar.dll",
};

// JSON 문자열에서 문자열/숫자 값 하나 추출 (단순 파서 — id/module/cmd 용).
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
            else { out += json[p]; ++p; }
        }
        return out;
    }
    size_t e = json.find_first_of(",}", p);
    return json.substr(p, (e == std::string::npos ? json.size() : e) - p);
}

void Modules_LoadAll(PostToUiFn post) {
    for (const wchar_t* name : kModuleDlls) {
        HMODULE h = LoadLibraryW(name);
        if (!h) continue;
        auto info   = (Module_Info_Fn)  GetProcAddress(h, "Module_Info");
        auto init   = (Module_Init_Fn)  GetProcAddress(h, "Module_Init");
        auto handle = (Module_Handle_Fn)GetProcAddress(h, "Module_Handle");
        if (!info || !init || !handle) { FreeLibrary(h); continue; }

        init(post);                 // 모듈에 UI post 콜백 전달
        Module m;
        m.dll    = h;
        m.handle = handle;
        m.info   = info();          // JSON, 즉시 복사 (모듈 소유 문자열)
        m.id     = JsonGetStr(m.info, "id");
        g_modules.push_back(m);
    }
}

Module* Modules_Find(const std::string& id) {
    for (auto& m : g_modules)
        if (m.id == id) return &m;
    return nullptr;
}
