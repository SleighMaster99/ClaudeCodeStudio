using System.Collections.Generic;
using System.Linq;
using StatusBarEditor.Models;

namespace StatusBarEditor.ViewModels;

public class PaletteGroup
{
    public string Name { get; }
    public IReadOnlyList<ItemTypeInfo> Items { get; }
    public PaletteGroup(string name)
    {
        Name = name;
        Items = ItemCatalog.All.Where(i => i.Category == name).ToList();
    }
}
