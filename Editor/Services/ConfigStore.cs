using System.Collections.Generic;
using System.IO;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Serialization;
using StatusBarEditor.Models;

namespace StatusBarEditor.Services;

public static class ConfigStore
{
    private static readonly JsonSerializerOptions WriteOptions = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping
    };

    public static Layout Load()
    {
        var path = ProjectPaths.ConfigPath;
        if (!File.Exists(path)) return DefaultLayout();

        try
        {
            using var fs = File.OpenRead(path);
            var dto = JsonSerializer.Deserialize<ConfigDto>(fs);
            if (dto?.Rows == null) return DefaultLayout();

            var layout = new Layout();
            foreach (var rowDto in dto.Rows)
            {
                var row = new Row();
                if (rowDto != null)
                    foreach (var it in rowDto)
                        if (!string.IsNullOrEmpty(it?.Type))
                            row.Items.Add(new Item(it.Type, it.Value));
                layout.Rows.Add(row);
            }
            if (layout.Rows.Count == 0) layout.Rows.Add(new Row());
            return layout;
        }
        catch
        {
            return DefaultLayout();
        }
    }

    public static void Save(Layout layout)
    {
        var dto = new ConfigDto();
        foreach (var row in layout.Rows)
        {
            var rowDto = new List<ItemDto>();
            foreach (var it in row.Items)
                rowDto.Add(new ItemDto { Type = it.Type, Value = it.Value });
            dto.Rows.Add(rowDto);
        }
        var json = JsonSerializer.Serialize(dto, WriteOptions);
        File.WriteAllText(ProjectPaths.ConfigPath, json);
    }

    private static Layout DefaultLayout()
    {
        var layout = new Layout();
        var row = new Row();
        row.Items.Add(new Item("model"));
        row.Items.Add(new Item("space"));
        row.Items.Add(new Item("sep_pipe"));
        row.Items.Add(new Item("space"));
        row.Items.Add(new Item("dir_short"));
        layout.Rows.Add(row);
        var row2 = new Row();
        row2.Items.Add(new Item("git_branch"));
        row2.Items.Add(new Item("space"));
        row2.Items.Add(new Item("sep_pipe"));
        row2.Items.Add(new Item("space"));
        row2.Items.Add(new Item("time"));
        layout.Rows.Add(row2);
        return layout;
    }

    private class ConfigDto
    {
        [JsonPropertyName("rows")]
        public List<List<ItemDto>> Rows { get; set; } = new();
    }

    private class ItemDto
    {
        [JsonPropertyName("type")] public string Type { get; set; } = "";
        [JsonPropertyName("value")] public string? Value { get; set; }
    }
}
