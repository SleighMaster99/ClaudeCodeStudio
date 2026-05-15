using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Media;

namespace StatusBarEditor.Models;

public class Item : INotifyPropertyChanged
{
    private string _type = "";
    private string? _value;

    public Item() { }
    public Item(string type, string? value = null) { _type = type; _value = value; }

    public string Type
    {
        get => _type;
        set
        {
            if (_type == value) return;
            _type = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(Label));
            OnPropertyChanged(nameof(Sample));
            OnPropertyChanged(nameof(Description));
            OnPropertyChanged(nameof(Background));
            OnPropertyChanged(nameof(IsTextItem));
        }
    }

    public string? Value
    {
        get => _value;
        set
        {
            if (_value == value) return;
            _value = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(Sample));
        }
    }

    public string Label => ItemCatalog.Find(_type)?.Label ?? _type;

    public string Sample
    {
        get
        {
            if (_type == "text")
                return string.IsNullOrEmpty(_value) ? "(빈 텍스트)" : _value!;
            return ItemCatalog.Find(_type)?.Sample ?? "?";
        }
    }

    public string Description => ItemCatalog.Find(_type)?.Description ?? "";

    public bool IsTextItem => _type == "text";

    public Brush Background => ItemCatalog.GetBackground(_type);

    public Item Clone() => new(_type, _value);

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
