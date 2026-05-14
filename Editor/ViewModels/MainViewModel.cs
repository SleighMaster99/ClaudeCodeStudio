using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using StatusBarEditor.Models;

namespace StatusBarEditor.ViewModels;

public class MainViewModel : INotifyPropertyChanged
{
    public Layout Layout { get; } = new();
    public IReadOnlyList<PaletteGroup> PaletteTabs { get; }
    public PaletteGroup Separators { get; }

    private bool _isDirty;
    public bool IsDirty
    {
        get => _isDirty;
        set { if (_isDirty != value) { _isDirty = value; OnPropertyChanged(); } }
    }

    public MainViewModel()
    {
        PaletteTabs = ItemCatalog.CategoryOrder.Select(c => new PaletteGroup(c)).ToList();
        Separators = new PaletteGroup(ItemCatalog.SeparatorCategory);
    }

    public void MarkDirty() => IsDirty = true;
    public void ClearDirty() => IsDirty = false;

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
