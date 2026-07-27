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

    /// <summary>pwsh(PowerShell 7) 가 PATH 에 있는지 — 모듈의 폴백 판단과 같은 기준.</summary>
    private static bool PwshInstalled()
    {
        try
        {
            var psi = new ProcessStartInfo("where", "pwsh")
            {
                RedirectStandardOutput = true, RedirectStandardError = true,
                UseShellExecute = false, CreateNoWindow = true
            };
            using var p = Process.Start(psi)!;
            p.StandardOutput.ReadToEnd();
            p.WaitForExit(5000);
            return p.ExitCode == 0;
        }
        catch { return false; }
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

            // 설정이 없는 상태의 기본 실행 셸은 pwsh
            Assert.AreEqual("pwsh", app.Driver.FindElement(By.Id("optShell")).GetAttribute("value"),
                "기본 실행 셸 = pwsh");

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

            // 저장 형식은 들여쓴 JSON (설치본에 배포되고 손으로도 고치는 파일)
            string cfg = File.ReadAllText(Path.Combine(root, "config.json"));
            StringAssert.Contains(cfg, "\"bar_width\": 16", "막대 폭 기록");
            StringAssert.Contains(cfg, "\"warn_pct\": 30", "주의 임계값 기록");
            StringAssert.Contains(cfg, "\"crit_pct\": 70", "위험 임계값 기록");
            StringAssert.Contains(cfg, "\"icon_set\": \"nerd\"", "아이콘 세트 기록");
            StringAssert.Contains(cfg, "\n", "사람이 읽을 수 있게 여러 줄로 저장");

            // 저장&적용 → settings.json statusLine 이 기본 셸(pwsh)로, 기존 필드는 보존
            app.Driver.FindElement(By.Id("applyBtn")).Click();
            string applyToast = CcsApp.WaitUntil(
                () => { try { return app.Driver.FindElement(By.Id("toast")).Text; } catch { return ""; } },
                t => t.Contains("적용 완료"), 8000);
            Assert.IsTrue(applyToast.Contains("적용 완료"), $"적용 결과 토스트 (실제: '{applyToast}')");

            // 기본 셸은 pwsh 이지만, pwsh 가 없는 PC 에서는 powershell 로 폴백된다
            string s = File.ReadAllText(settingsPath);
            string expected = PwshInstalled() ? "pwsh -NoProfile" : "powershell -NoProfile";
            StringAssert.Contains(s, "statusLine", "settings.json 에 statusLine 기록");
            StringAssert.Contains(s, expected, $"기본 실행 셸 반영 (기대: {expected})");
            StringAssert.Contains(s, "\"model\"", "settings.json 기존 필드 보존");

            // powershell 로 바꿔 적용하면 그쪽으로 기록된다 (양방향 확인)
            SetSelect(app, "#optShell", "powershell");
            app.Driver.FindElement(By.Id("applyBtn")).Click();
            bool switched = CcsApp.WaitUntil(
                () => { try { return File.ReadAllText(settingsPath).Contains("powershell -NoProfile"); } catch { return false; } },
                v => v, 8000);
            Assert.IsTrue(switched, "powershell 선택 시 해당 셸로 기록");
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

            // 커밋 메시지 형식: 치환 + 인자 인용 확인.
            // 따옴표를 포함하고 백슬래시로 끝나는 값(경로 표기)이라, 인용이 부실하면 제목이 깨진다.
            OpenSettingsTab(app);
            SetInput(app, "#syncCommitMsg", "sync {host} \"D:\\Repo\\\"");
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
                () => Git("log -1 --pretty=%s main", bare).Trim(), s => s.StartsWith("sync "), 10000);
            string expected = $"sync {Environment.MachineName} \"D:\\Repo\\\"";
            Assert.IsTrue(string.Equals(subject, expected, StringComparison.OrdinalIgnoreCase),
                $"커밋 메시지 치환 + 따옴표·백슬래시 보존 (기대: '{expected}', 실제: '{subject}')");
        }
        finally { try { Directory.Delete(baseDir, true); } catch { } }
    }

    [TestMethod]
    public void 이력_미커밋_변경행_표시와_태그_분리()
    {
        var (work, baseDir) = MakeSyncFixture(3);
        try
        {
            // 1) 변경 없음(clean) — 최신 커밋 행이 '현재 PC'·'서버' 태그를 함께 가진다
            using (var app = CcsApp.Launch(claudeDir: work))
            {
                OpenModuleTab(app, "sync");
                CcsApp.WaitUntil(() => app.Driver.FindElements(By.CssSelector(".hist-row")).Count, n => n == 3, 8000);
                Assert.AreEqual(0, app.Driver.FindElements(By.CssSelector(".hist-row.uncommitted")).Count,
                    "변경 없음: 미커밋 행 없음");
                // 본문 문구에도 '서버' 라는 낱말이 들어가므로 태그는 클래스로 판별한다
                var top = app.Driver.FindElements(By.CssSelector(".hist-row"))[0];
                Assert.AreEqual(1, top.FindElements(By.CssSelector(".tag-current")).Count, "최신 커밋 행에 '현재 PC' 태그");
                Assert.AreEqual(1, top.FindElements(By.CssSelector(".tag-remote")).Count, "최신 커밋 행에 '서버' 태그");
            }

            // 2) 미커밋 변경 2건 — '현재 PC' 가 맨 위 미커밋 행으로 올라가고 커밋 행에는 '서버'만 남는다
            File.WriteAllText(Path.Combine(work, "CLAUDE.md"), "local change");
            File.WriteAllText(Path.Combine(work, "settings.json"), "{\"rev\":99}");
            using (var app = CcsApp.Launch(claudeDir: work))
            {
                OpenModuleTab(app, "sync");
                int un = CcsApp.WaitUntil(
                    () => app.Driver.FindElements(By.CssSelector(".hist-row.uncommitted")).Count, n => n == 1, 8000);
                Assert.AreEqual(1, un, "미커밋 행 1개 표시");

                var rows = app.Driver.FindElements(By.CssSelector(".hist-row"));
                Assert.AreEqual(4, rows.Count, "미커밋 행 + 커밋 3건");
                StringAssert.Contains(rows[0].Text, "2건", "변경 파일 수 표시");
                Assert.AreEqual(1, rows[0].FindElements(By.CssSelector(".tag-current")).Count, "미커밋 행이 '현재 PC' 태그 보유");
                Assert.AreEqual(0, rows[0].FindElements(By.CssSelector(".tag-remote")).Count, "미커밋 행에 '서버' 태그 없음");
                Assert.AreEqual(0, rows[1].FindElements(By.CssSelector(".tag-current")).Count, "커밋 행에서 '현재 PC' 태그 제거됨");
                Assert.AreEqual(1, rows[1].FindElements(By.CssSelector(".tag-remote")).Count, "커밋 행에 '서버' 태그 유지");
            }
        }
        finally { try { Directory.Delete(baseDir, true); } catch { } }
    }

    [TestMethod]
    public void 동기화_진행_오버레이_클릭차단과_해제()
    {
        var (work, baseDir) = MakeSyncFixture(2);
        try
        {
            using var app = CcsApp.Launch(claudeDir: work);
            OpenModuleTab(app, "sync");
            app.WaitForSelector("#busy", 8000);

            // 평상시에는 숨겨져 있다
            Assert.IsFalse(IsShown(app, "#busy"), "대기 상태: 오버레이 숨김");

            // 진행 중 상태를 만들면 화면 전체를 덮어 버튼 클릭이 오버레이에 가로막힌다
            app.Js.ExecuteScript("document.getElementById('busy').hidden = false;");
            Assert.IsTrue(IsShown(app, "#busy"), "진행 중: 오버레이 노출");
            bool blocked = (bool)app.Js.ExecuteScript(@"
                var b = document.getElementById('pushBtn').getBoundingClientRect();
                var el = document.elementFromPoint(b.left + b.width / 2, b.top + b.height / 2);
                var busy = document.getElementById('busy');
                return el === busy || busy.contains(el);");
            Assert.IsTrue(blocked, "반영 버튼 위치의 클릭이 오버레이에 차단됨");

            // 실제 원격 작업을 돌리고 나면 응답 수신과 함께 다시 해제된다
            app.Js.ExecuteScript("document.getElementById('busy').hidden = true;");
            app.Driver.FindElement(By.Id("refreshBtn")).Click();
            bool released = CcsApp.WaitUntil(() => !IsShown(app, "#busy"), v => v, 15000);
            Assert.IsTrue(released, "작업 완료 후 오버레이 해제");
            Assert.IsTrue(app.Driver.FindElement(By.Id("refreshBtn")).Enabled, "작업 후 조작 가능");
        }
        finally { try { Directory.Delete(baseDir, true); } catch { } }
    }

    // ---------- 설정 탭 카테고리 / 검색 ----------
    private static bool IsShown(CcsApp app, string css)
    {
        try { return app.Driver.FindElement(By.CssSelector(css)).Displayed; } catch { return false; }
    }

    private static int VisibleRowCount(CcsApp app) =>
        app.Driver.FindElements(By.CssSelector(".set-row"))
                  .Count(e => { try { return e.Displayed; } catch { return false; } });

    [TestMethod]
    public void 설정_카테고리_탭_전환()
    {
        using var app = CcsApp.Launch();
        OpenSettingsTab(app);

        // 기본 카테고리 = 일반 (시작 탭 / 창 크기)
        Assert.IsTrue(IsShown(app, "#startTab"), "일반: 시작 탭 노출");
        Assert.IsTrue(IsShown(app, "#winSize"), "일반: 창 크기 노출");
        Assert.IsFalse(IsShown(app, "#theme"), "일반: 화면 설정 비노출");
        Assert.IsFalse(IsShown(app, "#syncHist"), "일반: 동기화 설정 비노출");

        app.Driver.FindElement(By.CssSelector(".set-tab[data-setcat=display]")).Click();
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, "#theme"), v => v, 3000), "화면: 테마 노출");
        Assert.IsFalse(IsShown(app, "#startTab"), "화면: 일반 설정 비노출");

        app.Driver.FindElement(By.CssSelector(".set-tab[data-setcat=sync]")).Click();
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, "#syncHist"), v => v, 3000), "동기화: 이력 개수 노출");
        Assert.IsFalse(IsShown(app, "#theme"), "동기화: 화면 설정 비노출");
    }

    [TestMethod]
    public void 설정_검색_전카테고리_필터()
    {
        using var app = CcsApp.Launch();
        OpenSettingsTab(app);

        // 현재 카테고리(일반)가 아닌 동기화 항목을 실제 타이핑으로 검색 → 카테고리를 넘어 노출
        app.WaitForSelector("#setSearch", 5000).SendKeys("템플릿");
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, "#syncCommitMsg"), v => v, 3000),
            "'템플릿' 검색 → 다른 카테고리의 커밋 메시지 형식 노출");
        Assert.AreEqual(1, VisibleRowCount(app), "일치하는 행만 노출");
        Assert.IsFalse(IsShown(app, ".set-tabs"), "검색 중 카테고리 탭 숨김");
        StringAssert.Contains(app.Driver.FindElement(By.Id("setSearchCount")).Text, "1개", "일치 개수 표시");

        // 영문 동의어(data-kw) 매칭
        SetInput(app, "#setSearch", "resolution");
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, "#winSize"), v => v, 3000),
            "'resolution' 검색 → 창 크기 노출(동의어 키워드)");

        // 결과 없음
        SetInput(app, "#setSearch", "존재하지않는설정");
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, "#setNoResult"), v => v, 3000), "결과 없음 안내 표시");
        Assert.AreEqual(0, VisibleRowCount(app), "일치 행 없음");

        // 지우기 → 카테고리 화면 복귀
        app.Driver.FindElement(By.Id("setSearchClear")).Click();
        Assert.IsTrue(CcsApp.WaitUntil(() => IsShown(app, ".set-tabs") && IsShown(app, "#startTab"), v => v, 3000),
            "검색 해제 후 카테고리 탭/일반 카테고리 복귀");
        Assert.IsFalse(IsShown(app, "#theme"), "검색 해제 후 비활성 카테고리는 다시 숨김");
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
