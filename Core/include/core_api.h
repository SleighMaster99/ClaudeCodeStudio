// Core.dll 공개 인터페이스.
// exe(부트스트래퍼)는 Core_Run() 하나만 호출한다. 기능 모듈(Sync/StatusBar)이
// export 하는 Module_* 계약은 P1~ 에서 별도 헤더(module_api.h)로 추가한다.
#pragma once
#include <windows.h>

#ifdef CORE_EXPORTS
#define CORE_API __declspec(dllexport)
#else
#define CORE_API __declspec(dllimport)
#endif

extern "C" {
    // 앱 실행 진입점: 창 + WebView2 + 좌측 탭 셸을 띄우고 메시지 루프를 돈다.
    // 반환값은 WM_QUIT 의 wParam (프로세스 종료 코드).
    CORE_API int Core_Run(HINSTANCE hInst, int nCmdShow);
}
