using OpenQA.Selenium;

namespace ClaudeCodeStudio.E2E;

// 실제 시나리오 — 탭 전환 + 모듈 iframe 렌더 검증(구동 수단이 실제 기능 검증에 쓰임을 보임).
[TestClass]
[DoNotParallelize]
public class Scenarios
{
    [TestMethod]
    public void 상태바탭_전환_후_카탈로그_팔레트_렌더()
    {
        using var app = CcsApp.Launch();

        // top-frame: '상태바 설정' 탭 클릭 → 해당 pane 활성화
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=statusbar]")).Click();
        app.WaitForSelector("#pane-statusbar.active", 5000);

        // statusbar 모듈 iframe: 카탈로그 팔레트 항목이 렌더됨 (ItemCatalog 승계, 70+ 항목)
        app.SwitchToModule("statusbar");
        int chips = CcsApp.WaitUntil(
            () => app.Driver.FindElements(By.CssSelector(".pal-chip")).Count,
            n => n > 50, 8000);
        Assert.IsTrue(chips > 50, $"팔레트 항목 렌더(실제 {chips}개)");
    }

    [TestMethod]
    public void 팔레트_카테고리_접기_펼치기()
    {
        using var app = CcsApp.Launch();
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=statusbar]")).Click();
        app.WaitForSelector("#pane-statusbar.active", 5000);
        app.SwitchToModule("statusbar");

        var group = app.WaitForSelector(".pal-group", 5000);
        var items = group.FindElement(By.CssSelector(".pal-items"));
        Assert.IsTrue(items.Displayed, "초기 펼침 상태");

        group.FindElement(By.CssSelector(".pal-cat")).Click();   // 접기
        bool collapsed = CcsApp.WaitUntil(() => !items.Displayed, v => v, 3000);
        Assert.IsTrue(collapsed, "헤더 클릭 후 접힘(항목 숨김)");

        group.FindElement(By.CssSelector(".pal-cat")).Click();   // 다시 펼치기
        bool expanded = CcsApp.WaitUntil(() => items.Displayed, v => v, 3000);
        Assert.IsTrue(expanded, "다시 클릭 후 펼침");
    }
}
