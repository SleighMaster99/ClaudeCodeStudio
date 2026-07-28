// ReleaseTool 내부 공유 선언 (main / build 간). 외부 노출 아님.
#pragma once
#include <windows.h>
#include <string>

// 워커 스레드 -> UI 스레드. WebView2 는 UI 스레드에서만 만질 수 있어서,
// 워커는 PostMessage 로 넘기고 WndProc 이 대신 회신한다.
#define WM_APP_BUILD_LINE (WM_APP + 1)   // lParam = new std::string* (UTF-8 한 줄) — 생성/배포 공용
#define WM_APP_BUILD_DONE (WM_APP + 2)   // wParam = 종료 코드, lParam = new std::string* (버전)
#define WM_APP_PUB_DONE   (WM_APP + 3)   // wParam = 종료 코드, lParam = new std::string* (버전)
#define WM_APP_PUB_STATUS (WM_APP + 4)   // lParam = new std::string* (배포 상태 JSON 조각)

// ---------- build.cpp ----------
std::wstring Widen(const std::string& s);
std::string  Narrow(const std::wstring& w);
std::wstring ExeDir();

// repo 루트 (ClaudeCodeStudio.sln 이 있는 폴더). 못 찾으면 빈 문자열.
std::wstring Repo_Root();
// installer\VERSION 의 마지막 발행 버전. 없으면 "0.0.0".
std::string  Repo_LatestVersion();
// 현재 git 브랜치. 못 읽으면 빈 문자열.
std::string  Repo_Branch();
bool         Repo_HasMsBuild();
bool         Repo_HasNsis();
bool         Repo_HasGh();

// Shipping\ClaudeCodeStudio-Setup-<ver>.exe 경로 / 존재 여부 / 크기(KB, 없으면 0).
std::string  Build_SetupPath(const std::string& version);
bool         Build_SetupExists(const std::string& version);
unsigned long long Build_SetupSizeKb(const std::string& version);

// 생성과 배포는 같은 로그 창을 쓰고 동시에 돌지 않는다 — 하나의 실행 플래그를 공유한다.
bool         Job_Running();

// Build.bat 을 워커 스레드에서 실행. 출력은 WM_APP_BUILD_LINE 으로 한 줄씩 올라온다.
void         Build_Start(HWND hwnd, const std::string& version, bool skipBuild);

// gh 로 태그 + Release 생성 + 설치파일 업로드. 완료 시 WM_APP_PUB_DONE.
void         Publish_Start(HWND hwnd, const std::string& version);
// 해당 버전이 이미 배포됐는지 원격 조회(네트워크) — 창을 막지 않게 워커에서 돌린다.
void         Publish_QueryStatus(HWND hwnd, const std::string& version);

// ---------- main.cpp ----------
void         Ui_Post(const std::string& json);
std::string  JsonEscape(const std::string& s);
std::string  JsonGetStr(const std::string& json, const std::string& key);
