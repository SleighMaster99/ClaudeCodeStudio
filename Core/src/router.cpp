// 메시지 라우터.
// JS(부모 셸)가 보낸 JSON {"module":"sync","cmd":"...","arg":"..."} 에서 module 을
// 뽑아 해당 모듈의 Module_Handle 로 원본 JSON 을 그대로 넘긴다. cmd/arg 해석은 모듈 몫.
#include "core_internal.h"

void Router_Handle(const std::string& jsonMsg) {
    std::string module = JsonGetStr(jsonMsg, "module");
    Module* m = Modules_Find(module);
    if (m && m->handle)
        m->handle(jsonMsg.c_str());
}
