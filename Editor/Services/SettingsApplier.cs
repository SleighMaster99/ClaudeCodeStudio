using System.IO;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace StatusBarEditor.Services;

public static class SettingsApplier
{
    private static readonly JsonSerializerOptions WriteOptions = new()
    {
        WriteIndented = true,
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping
    };

    public static void Apply()
    {
        var path = ProjectPaths.ClaudeSettingsPath;
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);

        JsonObject root;
        if (File.Exists(path))
        {
            var text = File.ReadAllText(path);
            root = JsonNode.Parse(text) as JsonObject ?? new JsonObject();
        }
        else
        {
            root = new JsonObject();
        }

        var cmd = $"powershell -NoProfile -ExecutionPolicy Bypass -File \"{ProjectPaths.StatusLineScriptPath}\"";
        root["statusLine"] = new JsonObject
        {
            ["type"] = "command",
            ["command"] = cmd
        };

        File.WriteAllText(path, root.ToJsonString(WriteOptions));
    }
}
