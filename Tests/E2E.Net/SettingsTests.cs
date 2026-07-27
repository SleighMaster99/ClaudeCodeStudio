using System.Diagnostics;
using OpenQA.Selenium;

namespace ClaudeCodeStudio.E2E;

// 설정 기능 E2E — 이번에 추가된 설정이 실제 동작(파일 기록/재시작 복원/모듈 반영)까지 이어지는지 검증.
//   재시작 복원   : stateDir 재사용으로 앱을 두 번 띄워 테마/창 크기/시작 탭 유지 확인
//   상태바 옵션   : CCSTUDIO_STATUSBAR_ROOT/CCSTUDIO_SETTINGS_PATH 임시 경로로 config.json/settings.json 기록 확인
//   동기화 설정   : 임시 git 저장소(+로컬 bare 원격) 픽스처로 이력 개수·커밋 메시지 형식·push 반영 확인
[TestClass]
[DoNotParallelize]
public class SettingsTests
{
    private static string NewTmpDir(string tag)
    {
        string p = Path.Combine(Path.GetTempPath(), $"ccs-e2e-{tag}-{Guid.NewGuid():N}");
        Directory.CreateDirectory(p);
        return p;
    }

    private static void SetSelect(CcsApp app, string css, string value)
    {
        var el = app.WaitForSelector(css, 5000);
        app.Js.ExecuteScript(
            "arguments[0].value=arguments[1]; arguments[0].dispatchEvent(new Event('change'));", el, value);
    }

    private static void SetInput(CcsApp app, string css, string value)
    {
        var el = app.WaitForSelector(css, 5000);
        app.Js.ExecuteScript(
            "arguments[0].value=arguments[1];" +
            "arguments[0].dispatchEvent(new Event('input'));" +
            "arguments[0].dispatchEvent(new Event('change'));", el, value);
    }

    private static void OpenSettingsTab(CcsApp app)
    {
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector("[data-tab=settings]")).Click();
        app.WaitForSelector("#pane-settings.active", 5000);
    }

    /// <summary>모듈 탭을 화면에 활성화한 뒤 iframe 으로 진입 — iframe 내부 클릭은 pane 이 보여야 가능.</summary>
    private static void OpenModuleTab(CcsApp app, string id)
    {
        app.Driver.SwitchTo().DefaultContent();
        app.Driver.FindElement(By.CssSelector($"[data-tab={id}]")).Click();
        app.WaitForSelector($"#pane-{id}.active", 5000);
        app.SwitchToModule(id);
    }

    private static string ShellTheme(CcsApp app) =>
        (string)app.Js.ExecuteScript("return document.documentElement.getAttribute('data-theme')||'';");

    [TestMethod]
    public void 재시작_후_테마_창크기_시작탭_복원()
    {
        string state = NewTmpDir("state");
        try
        {
            long widthAfterResize;
            using (var app = CcsApp.Launch(stateDir: state))
            {
                OpenSettingsTab(app);
                long before = (long)app.Js.ExecuteScript("return window.innerWidth;");
                SetSelect(app, "#theme", "dark");
                SetSelect(app, "#winSize", "1280x720");
                SetSelect(app, "#startTab", "statusbar");
                widthAfterResize = CcsApp.WaitUntil(
                    () => (long)app.Js.ExecuteScript("return window.innerWidth;"),
                    w => w < before - 100, 5000);
                Assert.IsTrue(widthAfterResize < before - 100, $"1280 선택 후 폭 감소 ({before}→{widthAfterResize})");
                Thread.Sleep(500);   // localStorage 디스크 커밋 여유
            }

            using (var app = CcsApp.Launch(stateDir: state))
            {
                string theme = CcsApp.WaitUntil(() => ShellTheme(app), t => t == "dark", 5000);
                Assert.AreEqual("dark", theme, "재시작 후 다크 테마 복원");

                app.WaitForSelector("#pane-statusbar.active", 10000);   // 시작 탭=상태바 복원

                long w = (long)app.Js.ExecuteScript("return window.innerWidth;");
                Assert.IsTrue(Math.Abs(w - widthAfterResize) <= 80, $"창 크기 복원 ({widthAfterResize}→{w})");

                OpenSettingsTab(app);
                Assert.AreEqual("1280x720", app.WaitForSelector("#winSize", 5000).GetAttribute("value"), "창 크기 드롭다운 복원");

                // 마지막 사용 탭 모드: sync 탭 사용 기록 후 재시작 → sync 로 시작
                SetSelect(app, "#startTab", "last");
                app.Driver.FindElement(By.CssSelector("[data-tab=sync]")).Click();
                app.WaitForSelector("#pane-sync.active", 5000);
                Thread.Sleep(500);
            }

            using (var app = CcsApp.Launch(stateDir: state))
            {
                app.WaitForSelector("#pane-sync.active", 10000);   // 마지막 사용 탭(sync) 복원
            }
        }
        finally { try { Directory.Delete(state, true); } catch { } }
    }

    [TestMethod]
    public void 상태바_옵션_저장_적용_파일기록()
    {
        string root = NewTmpDir("sbroot");
        string setDir = NewTmpDir("sbset");
        string settingsPath = Path.Combine(setDir, "settings.json");
        File.WriteAllText(settingsPath, "{\n  \"model\": \"opus\"\n}\n");
        try
        {
            using var app = CcsApp.Launch(statusbarRoot: root, settingsPath: settingsPath);
            app.Driver.SwitchTo().DefaultContent();
            app.Driver.FindElement(By.CssSelector("[data-tab=statusbar]")).Click();
            app.WaitForSelector("#pane-statusbar.active", 5000);
            app.SwitchToModule("statusbar");
            app.WaitForSelector("#optBarW", 8000);

            // 옵션 변경 → 저장. 토스트는 C++ 가 파일 쓰기를 마친 뒤 표시되므로,
            // 토스트 확인 후 1회 읽기 — 쓰는 중 폴링으로 인한 파일 공유 레이스를 없앤다.
            SetInput(app, "#optBarW", "16");
            SetInput(app, "#optWarn", "30");
            SetInput(app, "#optCrit", "70");
            SetSelect(app, "#optIcon", "nerd");
            app.Driver.FindElement(By.Id("saveBtn")).Click();
            string saveToast = CcsApp.WaitUntil(
                () => { try { return app.Driver.FindElement(By.Id("toast")).Text; } catch { return ""; } },
                t => t.Contains("저장했습니다"), 8000);
            Assert.IsTrue(saveToast.Contains("저장했습니다"), $"저장 결과 토스트 (실제: '{saveToast}')");

            string cfg = File.ReadAllText(Path.Combine(root, "config.json"));
            StringAssert.Contains(cfg, "\"bar_width\":16", "막대 폭 기록");
            StringAssert.Contains(cfg, "\"warn_pct\":30", "주의 임계값 기록");
            StringAssert.Contains(cfg, "\"crit_pct\":70", "위험 임계값 기록");
            StringAssert.Contains(cfg, "\"icon_set\":\"nerd\"", "아이콘 세트 기록");

            // 실행 셸 pwsh 선택 → 저장&적용 → settings.json statusLine 이 pwsh 로, 기존 필드 보존
            SetSelect(app, "#optShell", "pwsh");
            app.Driver.FindElement(By.Id("applyBtn")).Click();
            string applyToast = CcsApp.WaitUntil(
                () => { try { return app.Driver.FindElement(By.Id("toast")).Text; } catch { return ""; } },
                t => t.Contains("적용 완료"), 8000);
            Assert.IsTrue(applyToast.Contains("적용 완료"), $"적용 결과 토스트 (실제: '{applyToast}')");

            string s = File.ReadAllText(settingsPath);
            StringAssert.Contains(s, "statusLine", "settings.json 에 statusLine 기록");
            StringAssert.Contains(s, "pwsh -NoProfile", "실행 셸 pwsh 반영");
            StringAssert.Contains(s, "\"model\"", "settings.json 기존 필드 보존");
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
            try { Directory.Delete(setDir, true); } catch { }
        }
    }

    // ---------- 동기화 설정 (git 픽스처) ----------
    private static string Git(string args, string cwd)
    {
        var psi = new ProcessStartInfo("git", args)
        {
            WorkingDirectory = cwd,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        using var p = Process.Start(psi)!;
        string outp = p.StandardOutput.ReadToEnd() + p.StandardError.ReadToEnd();
        p.WaitForExit(30000);
        return outp;
    }

    /// <summary>임시 작업 저장소(main, N 커밋) + 로컬 bare 원격(origin) 픽스처.</summary>
    private static (string work, string baseDir) MakeSyncFixture(int commits)
    {
        string baseDir = NewTmpDir("git");
        string bare = Path.Combine(baseDir, "remote.git");
        string work = Path.Combine(baseDir, "work");
        Git($"init --bare -b main \"{bare}\"", baseDir);
        Git($"init -b main \"{work}\"", baseDir);
        Git("config user.name ccs-e2e", work);
        Git("config user.email ccs-e2e@example.com", work);
        for (int i = 1; i <= commits; i++)
        {
            File.WriteAllText(Path.Combine(work, "settings.json"), $"{{\"rev\":{i}}}");
            Git("add -A", work);
            Git($"commit -m c{i}", work);
        }
        Git($"remote add origin \"{bare}\"", work);
        Git("push -u origin main", work);
        return (work, baseDir);
    }

    [TestMethod]
    public void 동기화_이력개수_커밋형식_반영()
    {
        var (work, baseDir) = MakeSyncFixture(15);
        try
        {
            using var app = CcsApp.Launch(claudeDir: work);

            // 이력 표시 개수: 3 → 목록 3건, 12 → 12건 (설정 변경이 sync 모듈 git log -N 까지 반영)
            OpenSettingsTab(app);
            SetInput(app, "#syncHist", "3");
            OpenModuleTab(app, "sync");
            int n3 = CcsApp.WaitUntil(
                () => app.Driver.FindElements(By.CssSelector(".hist-row")).Count, n => n == 3, 8000);
            Assert.AreEqual(3, n3, "이력 3건 표시");

            OpenSettingsTab(app);
            SetInput(app, "#syncHist", "12");
            OpenModuleTab(app, "sync");
            int n12 = CcsApp.WaitUntil(
                () => app.Driver.FindElements(By.CssSelector(".hist-row")).Count, n => n == 12, 8000);
            Assert.AreEqual(12, n12, "이력 12건 표시");

            // 커밋 메시지 형식: {host}/{time} 치환된 제목으로 push 되는지 bare 원격에서 확인
            OpenSettingsTab(app);
            SetInput(app, "#syncCommitMsg", "sync: {host} / {time}");
            File.WriteAllText(Path.Combine(work, "CLAUDE.md"), "e2e change");
            OpenModuleTab(app, "sync");   // iframe 내부 클릭은 pane 활성화 필요
            app.Driver.FindElement(By.Id("refreshBtn")).Click();   // 상태 갱신 → push 활성화
            bool pushReady = CcsApp.WaitUntil(
                () => { try { return app.Driver.FindElement(By.Id("pushBtn")).Enabled; } catch { return false; } },
                v => v, 8000);
            Assert.IsTrue(pushReady, "로컬 변경 감지 후 push 버튼 활성화");
            app.Driver.FindElement(By.Id("pushBtn")).Click();

            string bare = Path.Combine(baseDir, "remote.git");
            string subject = CcsApp.WaitUntil(
                () => Git("log -1 --pretty=%s main", bare).Trim(), s => s.StartsWith("sync: "), 10000);
            string expectedPrefix = $"sync: {Environment.MachineName} / ";
            Assert.IsTrue(subject.StartsWith(expectedPrefix, StringComparison.OrdinalIgnoreCase),
                $"커밋 메시지 형식 치환 (실제: '{subject}')");
        }
        finally { try { Directory.Delete(baseDir, true); } catch { } }
    }

    [TestMethod]
    public void 동기화_기본저장소URL_초기설정_프리필()
    {
        using var app = CcsApp.Launch(unconfigured: true);
        OpenSettingsTab(app);
        SetInput(app, "#syncRepoUrl", "https://example.com/custom-settings.git");

        app.SwitchToModule("sync");
        string v = CcsApp.WaitUntil(
            () => { try { return app.Driver.FindElement(By.Id("repoUrl")).GetAttribute("value") ?? ""; } catch { return ""; } },
            s => s == "https://example.com/custom-settings.git", 8000);
        Assert.AreEqual("https://example.com/custom-settings.git", v, "초기 설정 화면 저장소 URL 프리필");
    }
}
