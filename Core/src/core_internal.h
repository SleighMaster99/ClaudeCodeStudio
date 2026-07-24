// Core 내부 공유 선언 (host / modules / router 간). 외부 노출 아님.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "module_api.h"

// 로드된 모듈 하나의 레지스트리 엔트리.
struct Module {
    std::string      id;              // Module_Info 의 "id"
    std::string      info;            // Module_Info() JSON 복사본
    HMODULE          dll    = nullptr;
    Module_Handle_Fn handle = nullptr;
};

// modules.cpp — 하드코딩 목록을 LoadLibrary + GetProcAddress 로 로드하고 Init.
void    Modules_LoadAll(PostToUiFn post);
Module* Modules_Find(const std::string& id);

// router.cpp — JS(부모 셸)에서 온 JSON 메시지를 대상 모듈로 분배.
void    Router_Handle(const std::string& jsonMsg);

// host.cpp — 모듈이 결과를 UI 로 돌려보낼 때 부르는 post 콜백.
void    Host_PostToUi(const char* json);

// 현재 Module_Handle 실행 중인 모듈 id. post 결과에 봉투 태그를 붙여
// 부모 셸이 올바른 모듈 iframe 으로 중계하게 한다. router.cpp 정의.
extern std::string g_currentModule;

// 공용 유틸 (modules.cpp 정의) — JSON 문자열에서 값 하나 추출.
std::string JsonGetStr(const std::string& json, const std::string& key);
