// ReleaseTool 내부 공유 선언 (main / build 간). 외부 노출 아님.
#pragma once
#include <windows.h>
#include <string>

// 빌드 워커 스레드 -> UI 스레드. WebView2 는 UI 스레드에서만 만질 수 있어서,
// 워커는 PostMessage 로 넘기고 WndProc 이 대신 회신한다.
#define WM_APP_BUILD_LINE (WM_APP + 1)   // lParam = new std::string* (UTF-8 한 줄)
#define WM_APP_BUILD_DONE (WM_APP + 2)   // wParam = 종료 코드, lParam = new std::string* (버전)

// ---------- build.cpp ----------
std::wstring Widen(const std::string& s);
std::string  Narrow(const std::wstring& w);
std::wstring ExeDir();

// repo 루트 (ClaudeCodeStudio.sln 이 있는 폴더). 못 찾으면 빈 문자열.
std::wstring Repo_Root();
// installer\VERSION 의 마지막 발행 버전. 없으면 "0.0.0".
std::string  Repo_LatestVersion();
bool         Repo_HasMsBuild();
bool         Repo_HasNsis();

// Build.bat 을 워커 스레드에서 실행. 출력은 WM_APP_BUILD_LINE 으로 한 줄씩 올라온다.
void         Build_Start(HWND hwnd, const std::string& version, bool skipBuild);
bool         Build_Running();
std::string  Build_SetupPath(const std::string& version);

// ---------- main.cpp ----------
void         Ui_Post(const std::string& json);
std::string  JsonEscape(const std::string& s);
std::string  JsonGetStr(const std::string& json, const std::string& key);
