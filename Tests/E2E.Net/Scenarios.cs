using System.Text.RegularExpressions;
using OpenQA.Selenium;
using OpenQA.Selenium.Interactions;

namespace ClaudeCodeStudio.E2E;

// 실제 시나리오 — 탭 전환 + 모듈 iframe 렌더/조작 검증.
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

    [TestMethod]
    public void 팔레트_레이아웃_너비_조절()
    {
        using var app = CcsApp.Launch();
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=statusbar]")).Click();
        app.WaitForSelector("#pane-statusbar.active", 5000);
        app.SwitchToModule("statusbar");

        var palette = app.WaitForSelector(".palette", 5000);
        var splitter = app.Driver.FindElement(By.Id("splitter"));
        // 가로 조절 커서(ew-resize — 시스템 커서 테마 반영, app.css 주석 참조)가 적용됐는지 확인
        var cursor = (string)app.Js.ExecuteScript("return getComputedStyle(arguments[0]).cursor;", splitter);
        Assert.AreEqual("ew-resize", cursor, "splitter cursor=ew-resize");
        int before = palette.Size.Width;

        // splitter 를 오른쪽으로 드래그 → 팔레트가 넓어짐
        new Actions(app.Driver).ClickAndHold(splitter).MoveByOffset(120, 0).Release().Perform();

        int after = CcsApp.WaitUntil(() => palette.Size.Width, w => w > before + 40, 3000);
        Assert.IsTrue(after > before + 40, $"팔레트 너비 증가 ({before}→{after})");
    }

    [TestMethod]
    public void 설정탭_테마_다크_전환_셸과_모듈_적용()
    {
        using var app = CcsApp.Launch();
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=settings]")).Click();
        app.WaitForSelector("#pane-settings.active", 5000);

        // 드롭다운을 다크로 변경 (change 이벤트로 설정 저장 + 전 문서 주입)
        var theme = app.WaitForSelector("#theme", 5000);
        app.Js.ExecuteScript("arguments[0].value='dark'; arguments[0].dispatchEvent(new Event('change'));", theme);

        string shellTheme = CcsApp.WaitUntil(
            () => (string)app.Js.ExecuteScript("return document.documentElement.getAttribute('data-theme')||'';"),
            v => v == "dark", 3000);
        Assert.AreEqual("dark", shellTheme, "셸 문서에 data-theme=dark 적용");

        app.SwitchToModule("sync");
        string frameTheme = CcsApp.WaitUntil(
            () => (string)app.Js.ExecuteScript("return document.documentElement.getAttribute('data-theme')||'';"),
            v => v == "dark", 3000);
        Assert.AreEqual("dark", frameTheme, "sync 모듈 iframe 에 data-theme=dark 적용");
    }

    [TestMethod]
    public void 초기설정_저장소URL_빈칸으로_시작()
    {
        using var app = CcsApp.Launch(unconfigured: true);
        app.SwitchToModule("sync");

        var input = app.WaitForSelector("#repoUrl", 10000);
        CcsApp.WaitUntil(() => { try { return input.Displayed; } catch { return false; } }, v => v, 8000);

        Assert.AreEqual("", input.GetAttribute("value") ?? "", "저장소 URL 은 미리 채워지지 않는다");
        Assert.AreNotEqual("", input.GetAttribute("placeholder") ?? "", "입력 형식은 placeholder 로 안내한다");
    }

    // 토큰은 앱 상태 폴더에 있고 E2E 는 그 폴더를 매번 임시로 격리하므로, 테스트는 항상 미로그인으로 뜬다.
    // 버튼을 누르면 실제 GitHub device flow 가 시작되므로 코드가 화면에 뜨는 것까지 확인할 수 있다.
    [TestMethod]
    public void 초기설정_미로그인이면_로그인_버튼으로_인증을_시작한다()
    {
        using var app = CcsApp.Launch(unconfigured: true);
        app.SwitchToModule("sync");
        app.WaitForSelector("#repoUrl", 10000);

        bool shown = CcsApp.WaitUntil(
            () => { try { return app.Driver.FindElement(By.Id("ghLoginBtn")).Displayed; } catch { return false; } },
            v => v, 20000);
        Assert.IsTrue(shown, "미로그인이면 [GitHub 로그인] 버튼이 열린다");
        Assert.IsFalse(app.Driver.FindElement(By.Id("modeNew")).Enabled, "'새로 만들기' 갈래는 잠긴다");
        StringAssert.Contains(app.Driver.FindElement(By.Id("ghState")).Text, "로그인 필요",
            "상태 문구가 무엇을 해야 하는지 알려준다");

        app.Driver.FindElement(By.Id("ghLoginBtn")).Click();
        string code = CcsApp.WaitUntil(
            () => { try { return app.Driver.FindElement(By.Id("ghCode")).Text; } catch { return ""; } },
            s => s.Length > 0 && s != "—", 40000);
        StringAssert.Matches(code, new Regex("^[0-9A-Za-z]{4}-[0-9A-Za-z]{4}$"),
            $"브라우저에 넣을 일회용 코드가 화면에 뜬다 (실제: '{code}')");
        Assert.IsTrue(app.Driver.FindElement(By.Id("ghCodeBox")).Displayed, "코드 영역이 열린다");
    }

    [TestMethod]
    public void 설정탭_창크기_해상도_변경()
    {
        using var app = CcsApp.Launch();
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=settings]")).Click();
        app.WaitForSelector("#pane-settings.active", 5000);

        // 기본 1920x1080 에서 1280x720 선택 → C++ 가 SetWindowPos → 클라이언트 폭 감소
        long before = (long)app.Js.ExecuteScript("return window.innerWidth;");
        var sel = app.WaitForSelector("#winSize", 5000);
        app.Js.ExecuteScript("arguments[0].value='1280x720'; arguments[0].dispatchEvent(new Event('change'));", sel);

        long after = CcsApp.WaitUntil(
            () => (long)app.Js.ExecuteScript("return window.innerWidth;"),
            w => w < before - 100, 5000);
        Assert.IsTrue(after < before - 100, $"창 클라이언트 폭 감소 ({before}→{after})");
    }
}
