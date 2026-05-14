using System.Collections.ObjectModel;

namespace StatusBarEditor.Models;

public class Row
{
    public ObservableCollection<Item> Items { get; } = new();
}
