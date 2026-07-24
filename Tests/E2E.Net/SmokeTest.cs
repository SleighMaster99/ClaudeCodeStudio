using OpenQA.Selenium;

namespace ClaudeCodeStudio.E2E;

// PoC 게이트 — 격리 데스크톱에서 앱 실행 → iframe 키 입력 1회 + 클릭 1회 → 상태 검증.
// 목적은 구동 수단(격리 데스크톱 + CDP 합성 입력) 자체가 동작함을 보이는 것.
[TestClass]
[DoNotParallelize]
public class SmokeTest
{
    [TestMethod]
    public void Smoke_격리데스크톱_클릭_키입력_검증()
    {
        using var app = CcsApp.Launch(unconfigured: true);

        // top-frame: 좌측 탭 2개 렌더
        Assert.AreEqual(2, app.Driver.FindElements(By.CssSelector(".tab")).Count, "좌측 탭 2개");

        // sync 모듈 iframe 진입 (미설정 → '초기 설정' 화면)
        app.SwitchToModule("sync");
        var input = app.WaitForSelector("#repoUrl", 10000);

        // 키 입력 1회
        input.Clear();
        input.SendKeys("https://example.com/e2e-smoke.git");
        Assert.AreEqual("https://example.com/e2e-smoke.git", input.GetAttribute("value"), "키 입력 반영");

        // 클릭 1회 → 버튼이 '설정 중…' 으로 전환됨을 확인 (클릭이 처리됨)
        app.Driver.FindElement(By.Id("setupBtn")).Click();
        string text = CcsApp.WaitUntil(
            () => { try { return app.Driver.FindElement(By.Id("setupBtn")).Text; } catch { return ""; } },
            t => t.Contains("설정 중"), 5000);
        StringAssert.Contains(text, "설정 중", "클릭 처리(버튼 상태 전환)");
    }
}
