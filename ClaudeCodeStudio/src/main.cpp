// ClaudeCodeStudio - 최소 부트스트래퍼.
// 창 생성/WebView2 호스팅/탭 셸은 전부 Core.dll(Core_Run)이 전담한다.
// 이 exe 는 진입점에서 Core_Run 을 호출하는 것 외에 아무 일도 하지 않는다.
#include <windows.h>

#include "core_api.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    return Core_Run(hInst, nCmdShow);
}
