using System;
using System.IO;

namespace StatusBarEditor.Services;

public static class ProjectPaths
{
    public static string ProjectRoot { get; } = FindProjectRoot();

    public static string ConfigPath => Path.Combine(ProjectRoot, "config.json");

    public static string ClaudeSettingsPath
        => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".claude", "settings.json");

    public static string StatusLineScriptPath => Path.Combine(ProjectRoot, "StatusLine.ps1");

    private static string FindProjectRoot()
    {
        var dir = AppContext.BaseDirectory;
        for (int i = 0; i < 8; i++)
        {
            if (File.Exists(Path.Combine(dir, "StatusLine.ps1"))) return dir;
            var parent = Directory.GetParent(dir)?.FullName;
            if (string.IsNullOrEmpty(parent)) break;
            dir = parent;
        }
        return Environment.CurrentDirectory;
    }
}
