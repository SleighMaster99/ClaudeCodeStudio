using System.Collections.ObjectModel;

namespace StatusBarEditor.Models;

public class Layout
{
    public ObservableCollection<Row> Rows { get; } = new();
}
