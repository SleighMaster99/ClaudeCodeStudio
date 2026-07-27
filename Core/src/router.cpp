// 메시지 라우터.
// JS(부모 셸)가 보낸 JSON {"module":"...","cmd":"...",...} 에서 module 을 뽑아
// 해당 모듈의 Module_Handle 로 원본 JSON 을 그대로 넘긴다. cmd 해석은 모듈 몫.
// Handle 실행 동안 g_currentModule 을 세팅해 두어, 모듈이 부르는 post 결과에
// 어느 모듈에서 나온 것인지 봉투 태그를 붙일 수 있게 한다(host.cpp).
#include "core_internal.h"

// Module_Handle 실행 중인 모듈 id (host.cpp 의 Host_PostToUi 가 참조).
std::string g_currentModule;

void Router_Handle(const std::string& jsonMsg) {
    std::string module = JsonGetStr(jsonMsg, "module");
    if (module == "core") {          // 모듈 DLL 이 아닌 Core 자체 명령 (셸 설정 탭)
        Host_HandleCoreCmd(jsonMsg);
        return;
    }
    Module* m = Modules_Find(module);
    if (m && m->handle) {
        g_currentModule = module;
        m->handle(jsonMsg.c_str());   // 이 안에서 동기적으로 post 가 호출된다
        g_currentModule.clear();
    }
}
