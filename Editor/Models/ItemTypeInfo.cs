using System.Windows.Media;

namespace StatusBarEditor.Models;

public record ItemTypeInfo(string Key, string Label, string Sample, string Description, string Category)
{
    public Brush Background => ItemCatalog.GetBackground(Key);
}
