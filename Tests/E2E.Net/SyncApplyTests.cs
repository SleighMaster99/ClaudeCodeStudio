// "서버 → 이 PC 적용" 이 이름대로 동작하는지 확인한다.
// 서버와 다르기만 하면 버튼이 켜지고, 이 PC 변경이 사라질 때는 먼저 묻는다.
using System.Diagnostics;
using OpenQA.Selenium;

namespace ClaudeCodeStudio.E2E;

[TestClass]
public class SyncApplyTests
{
    private static string Git(string dir, string args)
    {
        var psi = new ProcessStartInfo("git", args)
        {
            WorkingDirectory = dir,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        using var p = Process.Start(psi)!;
        string o = p.StandardOutput.ReadToEnd() + p.StandardError.ReadToEnd();
        p.WaitForExit();
        if (p.ExitCode != 0 && !args.StartsWith("status") && !args.StartsWith("rev-list"))
            throw new InvalidOperationException($"git {args} 실패 (exit={p.ExitCode}): {o.Trim()}");
        return o.Trim();
    }

    /// <summary>서버(bare) + 이 PC(초기 설정 완료) 픽스처. 정리는 호출자 몫.</summary>
    private sealed class Fixture : IDisposable
    {
        public string Root { get; }
        public string Seed { get; }    // 다른 PC 역할 — 서버에 새 설정을 올릴 때 쓴다
        public string Client { get; }  // 앱이 바라보는 ~/.claude

        public Fixture()
        {
            Root = Path.Combine(Path.GetTempPath(), "ccs-apply-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Root);
            string server = Path.Combine(Root, "server.git");
            Seed = Path.Combine(Root, "seed");
            Client = Path.Combine(Root, "client");
            Directory.CreateDirectory(Seed);
            Directory.CreateDirectory(Client);

            Git(Root, $"init --bare -b main \"{server}\"");
            string url = server.Replace('\\', '/');

            Git(Seed, "init -b main");
            Git(Seed, "config user.email t@t");
            Git(Seed, "config user.name t");
            File.WriteAllText(Path.Combine(Seed, ".gitignore"),
                "/*\n!/.gitignore\n!/CLAUDE.md\n!/settings.json\n!/commands/\n!/hooks/\n!/output-styles/\n");
            File.WriteAllText(Path.Combine(Seed, "settings.json"), "{\"from\":\"server-v1\"}\n");
            Git(Seed, "add -A");
            Git(Seed, "commit -m seed");
            Git(Seed, $"remote add origin \"{url}\"");
            Git(Seed, "push -u origin main");

            // 앱의 CmdBootstrap 과 같은 절차로 이 PC 를 서버에 붙인다.
            Git(Client, "init -b main");
            Git(Client, "config user.email t@t");
            Git(Client, "config user.name t");
            Git(Client, $"remote add origin \"{url}\"");
            Git(Client, "fetch origin");
            Git(Client, "reset --hard origin/main");
            Git(Client, "branch --set-upstream-to=origin/main main");
        }

        /// <summary>다른 PC 가 서버에 새 설정을 올린 상황을 만든다.</summary>
        public void PushServerChange(string body)
        {
            File.WriteAllText(Path.Combine(Seed, "settings.json"), body);
            Git(Seed, "commit -am server-change");
            Git(Seed, "push origin main");
        }

        /// <summary>이 PC 에서 설정을 고친 상황을 만든다(커밋 전).</summary>
        public void EditLocal(string body) => File.WriteAllText(Path.Combine(Client, "settings.json"), body);

        public string ClientSettings => File.ReadAllText(Path.Combine(Client, "settings.json"));

        public void Dispose() { try { Directory.Delete(Root, true); } catch { } }
    }

    private static string Text(CcsApp app, string id)
    {
        try { return app.Driver.FindElement(By.Id(id)).Text; } catch { return ""; }
    }

    // 시작 시 자동 확인이 끝나 상태 카드가 최종 문구로 바뀔 때까지 기다린다.
    private static string WaitForStatus(CcsApp app)
    {
        return CcsApp.WaitUntil(() => Text(app, "statusTitle"),
                                s => s.Length > 0 && !s.Contains("확인 중"), 30000);
    }

    // 셸(top-frame) 의 좌측 탭이 모두 잠겼는지 본다. 확인 후 sync 모듈로 되돌아간다.
    private static bool TabsLocked(CcsApp app)
    {
        app.Driver.SwitchTo().DefaultContent();
        bool locked = app.Driver.FindElements(By.CssSelector(".tab")).All(t => !t.Enabled);
        app.SwitchToModule("sync");
        return locked;
    }

    [TestMethod]
    public void 이_PC_설정만_달라도_서버_적용_버튼이_켜진다()
    {
        using var fx = new Fixture();
        fx.EditLocal("{\"from\":\"local-edit\"}\n");

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);

        string title = WaitForStatus(app);
        Assert.AreEqual("서버와 설정이 다름", title, "상태가 '다르다' 로 읽힌다");
        Assert.IsTrue(app.Driver.FindElement(By.Id("pullBtn")).Enabled,
            "서버에 새 커밋이 없어도 설정이 다르면 적용 버튼이 켜진다");
        StringAssert.Contains(Text(app, "pullSub"), "버림",
            "누르면 이 PC 변경이 사라진다는 것을 버튼이 미리 알려준다");
    }

    [TestMethod]
    public void 적용을_누르면_먼저_묻고_취소하면_그대로다()
    {
        using var fx = new Fixture();
        fx.EditLocal("{\"from\":\"local-edit\"}\n");

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);
        WaitForStatus(app);

        app.Driver.FindElement(By.Id("pullBtn")).Click();
        bool shown = CcsApp.WaitUntil(
            () => { try { return app.Driver.FindElement(By.Id("confirm")).Displayed; } catch { return false; } },
            v => v, 5000);
        Assert.IsTrue(shown, "사라질 것이 있으면 확인 카드가 뜬다");
        StringAssert.Contains(Text(app, "confirmDesc"), "1건",
            "몇 건이 사라지는지 알려준다");
        StringAssert.Contains(Text(app, "confirmDesc"), ".sync-backup",
            "어디에 백업되는지 알려준다");

        app.Driver.FindElement(By.Id("confirmNo")).Click();
        bool gone = CcsApp.WaitUntil(
            () => { try { return !app.Driver.FindElement(By.Id("confirm")).Displayed; } catch { return true; } },
            v => v, 5000);
        Assert.IsTrue(gone, "취소하면 카드가 닫힌다");
        Assert.AreEqual("{\"from\":\"local-edit\"}\n".Replace("\r\n", "\n"),
                        fx.ClientSettings.Replace("\r\n", "\n"),
                        "취소했으므로 이 PC 설정은 그대로다");
    }

    [TestMethod]
    public void 확인_카드가_뜨는_동안_왼쪽_탭도_잠긴다()
    {
        using var fx = new Fixture();
        fx.EditLocal("{\"from\":\"local-edit\"}\n");

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);
        WaitForStatus(app);

        Assert.IsFalse(TabsLocked(app), "평소에는 탭이 눌린다");

        app.Driver.FindElement(By.Id("pullBtn")).Click();
        CcsApp.WaitUntil(() => { try { return app.Driver.FindElement(By.Id("confirm")).Displayed; } catch { return false; } },
                         v => v, 5000);

        // 카드는 모듈 iframe 안에 떠서 좌측 탭 바를 덮지 못한다 — 셸이 대신 탭을 잠근다.
        Assert.IsTrue(CcsApp.WaitUntil(() => TabsLocked(app), v => v, 5000),
            "카드가 떠 있는 동안 왼쪽 탭은 눌리지 않는다");

        app.Driver.FindElement(By.Id("confirmNo")).Click();
        Assert.IsTrue(CcsApp.WaitUntil(() => !TabsLocked(app), v => v, 5000),
            "카드를 닫으면 탭이 다시 눌린다");
    }

    [TestMethod]
    public void 확인하면_서버_설정으로_맞춰진다()
    {
        using var fx = new Fixture();
        fx.EditLocal("{\"from\":\"local-edit\"}\n");

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);
        WaitForStatus(app);

        app.Driver.FindElement(By.Id("pullBtn")).Click();
        CcsApp.WaitUntil(() => { try { return app.Driver.FindElement(By.Id("confirm")).Displayed; } catch { return false; } },
                         v => v, 5000);
        app.Driver.FindElement(By.Id("confirmYes")).Click();

        string title = CcsApp.WaitUntil(() => Text(app, "statusTitle"), s => s == "최신 상태", 30000);
        Assert.AreEqual("최신 상태", title, $"적용 후 서버와 같아진다 (실제: '{title}')");
        StringAssert.Contains(fx.ClientSettings, "server-v1", "파일 내용이 서버 것으로 바뀐다");

        // 사라진 변경은 백업에 남는다 — 되돌릴 길을 없애지 않는다.
        var backups = Directory.GetDirectories(fx.Client, ".sync-backup-*");
        Assert.IsTrue(backups.Length > 0, "덮기 전 .sync-backup 폴더가 생긴다");
        string saved = File.ReadAllText(Path.Combine(backups[0], "settings.json"));
        StringAssert.Contains(saved, "local-edit", "버려진 설정이 백업에 들어 있다");
    }

    [TestMethod]
    public void 양쪽이_갈려도_적용이_성공한다()
    {
        using var fx = new Fixture();
        fx.PushServerChange("{\"from\":\"server-v2\"}\n");   // 서버가 먼저 바뀌고
        fx.EditLocal("{\"from\":\"local-edit\"}\n");         // 이 PC 도 같은 파일을 고쳤다

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);

        string title = WaitForStatus(app);
        Assert.AreEqual("양쪽이 갈렸습니다", title, "양쪽이 달라진 상황임을 알려준다");

        app.Driver.FindElement(By.Id("pullBtn")).Click();
        CcsApp.WaitUntil(() => { try { return app.Driver.FindElement(By.Id("confirm")).Displayed; } catch { return false; } },
                         v => v, 5000);
        app.Driver.FindElement(By.Id("confirmYes")).Click();

        string after = CcsApp.WaitUntil(() => Text(app, "statusTitle"), s => s == "최신 상태", 30000);
        Assert.AreEqual("최신 상태", after, $"실패하지 않고 서버에 맞춰진다 (실제: '{after}')");
        StringAssert.Contains(fx.ClientSettings, "server-v2", "서버의 최신 설정이 적용된다");
    }

    [TestMethod]
    public void 앱을_켜면_서버를_조용히_확인한다()
    {
        using var fx = new Fixture();
        fx.PushServerChange("{\"from\":\"server-v2\"}\n");   // 이 PC 는 아직 모르는 변경

        using var app = CcsApp.Launch(claudeDir: fx.Client);
        app.SwitchToModule("sync");
        app.WaitForSelector("#pullBtn", 10000);

        // 확인이 끝날 때까지 기다리는 동안 진행 오버레이가 한 번도 뜨지 않아야 한다.
        bool overlaySeen = false;
        string title = CcsApp.WaitUntil(
            () =>
            {
                try { if (app.Driver.FindElement(By.Id("busy")).Displayed) overlaySeen = true; } catch { }
                return Text(app, "statusTitle");
            },
            s => s == "동기화 필요", 30000, 100);

        Assert.AreEqual("동기화 필요", title,
            $"⟳ 를 누르지 않아도 서버 변경을 알아낸다 (실제: '{title}')");
        Assert.IsFalse(overlaySeen, "시작 확인은 화면을 덮지 않는다");
        StringAssert.Contains(Text(app, "pullSub"), "1건", "받아올 변경 건수를 보여준다");
    }
}
