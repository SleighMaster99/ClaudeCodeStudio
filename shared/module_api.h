// 기능 모듈(Sync/StatusBar ...) 과 Core 가 공유하는 C 인터페이스 계약.
// 경계는 C 문자열(JSON) 뿐 — STL 을 DLL 경계로 넘기지 않아 ABI 안전.
#pragma once

// Core -> 모듈: UI(JS)로 결과 JSON 문자열을 돌려보내는 콜백.
typedef void (*PostToUiFn)(const char* json);

// 모듈이 export 하는 함수들의 시그니처. Core 는 GetProcAddress 로 바인딩한다.
extern "C" {
    typedef const char* (*Module_Info_Fn)();                     // 메타데이터 JSON
    typedef void        (*Module_Init_Fn)(PostToUiFn post);      // 초기화(post 콜백 전달)
    typedef void        (*Module_Handle_Fn)(const char* reqJson); // 요청 처리
}

// 모듈 구현 측 export 매크로. 모듈 프로젝트는 MODULE_EXPORTS 를 정의한다.
// (Core 는 정의하지 않으므로 이 헤더를 함수 포인터 타입 용도로만 쓴다.)
#ifdef MODULE_EXPORTS
#define MODULE_API extern "C" __declspec(dllexport)
#else
#define MODULE_API extern "C"
#endif
