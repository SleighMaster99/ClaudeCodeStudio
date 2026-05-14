using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using StatusBarEditor.Controls;
using StatusBarEditor.Models;
using StatusBarEditor.Services;
using StatusBarEditor.ViewModels;

namespace StatusBarEditor;

public partial class MainWindow : Window
{
    private readonly MainViewModel _vm = new();

    private Point _dragStartPos;
    private const string DragFormat = "StatusBarEditor.DragPayload";

    private DragAdorner? _adorner;
    private AdornerLayer? _adornerLayer;
    private Point _ghostOffset;  // 마우스 클릭 위치 ↔ 카드 좌상단 오프셋

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _vm;
        LoadFromDisk();
    }

    private void LoadFromDisk()
    {
        var loaded = ConfigStore.Load();
        _vm.Layout.Rows.Clear();
        foreach (var r in loaded.Rows) _vm.Layout.Rows.Add(r);
        if (_vm.Layout.Rows.Count == 0) _vm.Layout.Rows.Add(new Row());
        _vm.ClearDirty();
    }

    // === 드래그 시작 ===

    private void DragSource_MouseDown(object sender, MouseButtonEventArgs e)
    {
        _dragStartPos = e.GetPosition(null);
        if (sender is FrameworkElement fe)
            _ghostOffset = e.GetPosition(fe);
    }

    private void StartAdorner(FrameworkElement source)
    {
        StopAdorner();
        _adornerLayer = AdornerLayer.GetAdornerLayer(RootGrid);
        if (_adornerLayer == null) return;
        _adorner = new DragAdorner(RootGrid, source);
        _adornerLayer.Add(_adorner);
    }

    private void StopAdorner()
    {
        if (_adornerLayer != null && _adorner != null)
            _adornerLayer.Remove(_adorner);
        _adorner = null;
        _adornerLayer = null;
    }

    private void Window_PreviewDragOver(object sender, DragEventArgs e)
    {
        if (_adorner == null) return;
        var pos = e.GetPosition(RootGrid);
        _adorner.UpdatePosition(new Point(pos.X - _ghostOffset.X, pos.Y - _ghostOffset.Y));
    }

    private void Window_PreviewQueryContinueDrag(object sender, QueryContinueDragEventArgs e)
    {
        // 드래그가 끝났거나(EscapeCancel/Drop) 마우스 좌클릭 해제되면 adorner 정리.
        if (e.Action == DragAction.Cancel || e.Action == DragAction.Drop) StopAdorner();
    }

    private bool PastDragThreshold(MouseEventArgs e)
    {
        var pos = e.GetPosition(null);
        return Math.Abs(pos.X - _dragStartPos.X) >= SystemParameters.MinimumHorizontalDragDistance
            || Math.Abs(pos.Y - _dragStartPos.Y) >= SystemParameters.MinimumVerticalDragDistance;
    }

    private void PaletteItem_MouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed) return;
        if (sender is not FrameworkElement fe || fe.Tag is not ItemTypeInfo info) return;
        if (!PastDragThreshold(e)) return;

        var payload = new DragPayload { Kind = DragKind.Palette, PaletteKey = info.Key };
        var data = new DataObject(DragFormat, payload);
        StartAdorner(fe);
        try { DragDrop.DoDragDrop(fe, data, DragDropEffects.Copy | DragDropEffects.Move); }
        finally { StopAdorner(); }
    }

    private void PreviewItem_MouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed) return;
        if (sender is not FrameworkElement fe || fe.Tag is not Item item) return;
        if (!PastDragThreshold(e)) return;

        var sourceRow = FindRowOf(item);
        if (sourceRow == null) return;

        var payload = new DragPayload { Kind = DragKind.PreviewItem, Item = item, SourceRow = sourceRow };
        var data = new DataObject(DragFormat, payload);
        StartAdorner(fe);
        try { DragDrop.DoDragDrop(fe, data, DragDropEffects.Copy | DragDropEffects.Move); }
        finally { StopAdorner(); }
    }

    private void RowHandle_MouseMove(object sender, MouseEventArgs e)
    {
        if (e.LeftButton != MouseButtonState.Pressed) return;
        if (sender is not FrameworkElement fe || fe.Tag is not Row row) return;
        if (!PastDragThreshold(e)) return;

        var payload = new DragPayload { Kind = DragKind.Row, RowRef = row };
        var data = new DataObject(DragFormat, payload);
        // 행 핸들은 텍스트라 ghost 대상이 작아 보임. 대신 행 본체를 ghost로 사용.
        var rowBorder = FindAncestor<Border>(fe, b => b.Tag is Row r && ReferenceEquals(r, row));
        StartAdorner(rowBorder ?? fe);
        try { DragDrop.DoDragDrop(fe, data, DragDropEffects.Copy | DragDropEffects.Move); }
        finally { StopAdorner(); }
    }

    private static T? FindAncestor<T>(DependencyObject start, Func<T, bool>? predicate = null) where T : DependencyObject
    {
        var cur = start;
        while (cur != null)
        {
            if (cur is T t && (predicate == null || predicate(t))) return t;
            cur = VisualTreeHelper.GetParent(cur);
        }
        return null;
    }

    // === 우클릭 (삭제) ===

    private void PreviewItem_RightClick(object sender, MouseButtonEventArgs e)
    {
        if (sender is not FrameworkElement fe || fe.Tag is not Item item) return;
        var row = FindRowOf(item);
        if (row == null) return;
        row.Items.Remove(item);
        _vm.MarkDirty();
        e.Handled = true;
    }

    // === 드롭 대상: 줄(Row) ===

    private void Row_DragOver(object sender, DragEventArgs e)
    {
        var p = GetPayload(e);
        if (p == null) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        // 행 자체를 행 위에 드롭하는 건 무의미 → 거부 (줄 사이 영역으로 가야 함)
        if (p.Kind == DragKind.Row) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        e.Effects = DragDropEffects.Move;
        if (sender is Border b) b.Background = new SolidColorBrush(Color.FromRgb(232, 242, 255));
        e.Handled = true;
    }

    private void Row_DragLeave(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = new SolidColorBrush(Color.FromRgb(252, 252, 252));
    }

    private void Row_Drop(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = new SolidColorBrush(Color.FromRgb(252, 252, 252));
        if (sender is not FrameworkElement fe || fe.Tag is not Row targetRow) return;
        var p = GetPayload(e);
        if (p == null) return;
        if (p.Kind == DragKind.Row) return; // 무시

        var insertIndex = ComputeRowInsertIndex(fe, e.GetPosition(fe).X, targetRow);

        if (p.Kind == DragKind.Palette && p.PaletteKey != null)
        {
            var newItem = MakeItemFromPalette(p.PaletteKey);
            if (newItem == null) return;
            targetRow.Items.Insert(Math.Clamp(insertIndex, 0, targetRow.Items.Count), newItem);
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.PreviewItem && p.Item != null && p.SourceRow != null)
        {
            var srcRow = p.SourceRow;
            var srcIdx = srcRow.Items.IndexOf(p.Item);
            if (srcIdx < 0) return;
            srcRow.Items.RemoveAt(srcIdx);
            var adj = insertIndex;
            if (ReferenceEquals(srcRow, targetRow) && srcIdx < adj) adj--;
            adj = Math.Clamp(adj, 0, targetRow.Items.Count);
            targetRow.Items.Insert(adj, p.Item);
            CompactEmptyRows();
            _vm.MarkDirty();
        }
        e.Handled = true;
    }

    private int ComputeRowInsertIndex(FrameworkElement rowBorder, double clientX, Row row)
    {
        // 행 안의 ItemsControl을 찾아 자식 항목 위치로 인덱스 결정
        var itemsControl = FindDescendant<ItemsControl>(rowBorder, ic => ic.ItemsSource is System.Collections.IEnumerable);
        if (itemsControl == null) return row.Items.Count;
        var insertIndex = row.Items.Count;
        for (int i = 0; i < row.Items.Count; i++)
        {
            if (itemsControl.ItemContainerGenerator.ContainerFromIndex(i) is not ContentPresenter cp) continue;
            var transform = cp.TransformToAncestor(rowBorder);
            var topLeft = transform.Transform(new Point(0, 0));
            if (clientX < topLeft.X + cp.ActualWidth / 2) { insertIndex = i; break; }
        }
        return insertIndex;
    }

    // === 드롭 대상: 줄 사이 영역 ===

    private void InterRowZone_DragOver(object sender, DragEventArgs e)
    {
        var p = GetPayload(e);
        if (p == null) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        e.Effects = DragDropEffects.Move;
        if (sender is Border b) b.Background = new SolidColorBrush(Color.FromRgb(120, 180, 250));
        e.Handled = true;
    }

    private void InterRowZone_DragLeave(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = Brushes.Transparent;
    }

    private void InterRowZone_Drop(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = Brushes.Transparent;
        if (sender is not FrameworkElement fe || fe.Tag is not Row anchorRow) return;
        var p = GetPayload(e);
        if (p == null) return;

        var anchorIdx = _vm.Layout.Rows.IndexOf(anchorRow);
        if (anchorIdx < 0) return;
        // 줄 사이 영역은 anchorRow의 "위"에 새 줄을 삽입. 즉 anchorIdx 위치에 새 행 추가.
        var insertIndex = anchorIdx;

        if (p.Kind == DragKind.Palette && p.PaletteKey != null)
        {
            var newItem = MakeItemFromPalette(p.PaletteKey);
            if (newItem == null) return;
            var newRow = new Row();
            newRow.Items.Add(newItem);
            _vm.Layout.Rows.Insert(insertIndex, newRow);
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.PreviewItem && p.Item != null && p.SourceRow != null)
        {
            var srcRow = p.SourceRow;
            srcRow.Items.Remove(p.Item);
            var newRow = new Row();
            newRow.Items.Add(p.Item);
            _vm.Layout.Rows.Insert(insertIndex, newRow);
            CompactEmptyRows();
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.Row && p.RowRef != null)
        {
            var src = p.RowRef;
            var srcIdx = _vm.Layout.Rows.IndexOf(src);
            if (srcIdx < 0) return;
            var target = insertIndex;
            if (srcIdx < target) target--;
            _vm.Layout.Rows.RemoveAt(srcIdx);
            target = Math.Clamp(target, 0, _vm.Layout.Rows.Count);
            _vm.Layout.Rows.Insert(target, src);
            _vm.MarkDirty();
        }
        e.Handled = true;
    }

    // === 드롭 대상: 마지막 줄 아래 (trailing zone) ===

    private void TrailingZone_DragOver(object sender, DragEventArgs e)
    {
        var p = GetPayload(e);
        if (p == null) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        e.Effects = DragDropEffects.Move;
        if (sender is Border b) b.Background = new SolidColorBrush(Color.FromRgb(120, 180, 250));
        e.Handled = true;
    }

    private void TrailingZone_DragLeave(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = Brushes.Transparent;
    }

    private void TrailingZone_Drop(object sender, DragEventArgs e)
    {
        if (sender is Border b) b.Background = Brushes.Transparent;
        var p = GetPayload(e);
        if (p == null) return;
        var insertIndex = _vm.Layout.Rows.Count; // 맨 끝

        if (p.Kind == DragKind.Palette && p.PaletteKey != null)
        {
            var newItem = MakeItemFromPalette(p.PaletteKey);
            if (newItem == null) return;
            var newRow = new Row();
            newRow.Items.Add(newItem);
            _vm.Layout.Rows.Insert(insertIndex, newRow);
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.PreviewItem && p.Item != null && p.SourceRow != null)
        {
            p.SourceRow.Items.Remove(p.Item);
            var newRow = new Row();
            newRow.Items.Add(p.Item);
            _vm.Layout.Rows.Insert(insertIndex, newRow);
            CompactEmptyRows();
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.Row && p.RowRef != null)
        {
            var src = p.RowRef;
            var srcIdx = _vm.Layout.Rows.IndexOf(src);
            if (srcIdx < 0) return;
            _vm.Layout.Rows.RemoveAt(srcIdx);
            _vm.Layout.Rows.Add(src);
            _vm.MarkDirty();
        }
        e.Handled = true;
    }

    // === 드롭 대상: 팔레트 영역 (삭제) ===

    private void PaletteArea_DragOver(object sender, DragEventArgs e)
    {
        var p = GetPayload(e);
        if (p == null) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        if (p.Kind == DragKind.Palette) { e.Effects = DragDropEffects.None; e.Handled = true; return; }
        e.Effects = DragDropEffects.Move;
        e.Handled = true;
    }

    private void PaletteArea_Drop(object sender, DragEventArgs e)
    {
        var p = GetPayload(e);
        if (p == null) return;
        if (p.Kind == DragKind.PreviewItem && p.Item != null && p.SourceRow != null)
        {
            p.SourceRow.Items.Remove(p.Item);
            CompactEmptyRows();
            _vm.MarkDirty();
        }
        else if (p.Kind == DragKind.Row && p.RowRef != null)
        {
            if (_vm.Layout.Rows.Count > 1)
            {
                _vm.Layout.Rows.Remove(p.RowRef);
                _vm.MarkDirty();
            }
        }
        e.Handled = true;
    }

    // === 버튼 ===

    private void AddRow_Click(object sender, RoutedEventArgs e)
    {
        _vm.Layout.Rows.Add(new Row());
        _vm.MarkDirty();
    }

    private void DeleteRow_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not FrameworkElement fe || fe.Tag is not Row row) return;
        if (_vm.Layout.Rows.Count <= 1) return; // 최소 1줄 유지
        _vm.Layout.Rows.Remove(row);
        _vm.MarkDirty();
    }

    private void Reload_Click(object sender, RoutedEventArgs e)
    {
        LoadFromDisk();
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            ConfigStore.Save(_vm.Layout);
            SettingsApplier.Apply();
            _vm.ClearDirty();
            MessageBox.Show("저장 완료.\nClaude Code 다음 실행 시 적용됩니다.", "완료", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"저장 실패: {ex.Message}", "오류", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    // === 헬퍼 ===

    private Row? FindRowOf(Item item)
    {
        foreach (var r in _vm.Layout.Rows)
            if (r.Items.Contains(item)) return r;
        return null;
    }

    private static DragPayload? GetPayload(DragEventArgs e)
        => e.Data.GetDataPresent(DragFormat) ? e.Data.GetData(DragFormat) as DragPayload : null;

    private Item? MakeItemFromPalette(string key)
    {
        if (key == "text")
        {
            var text = PromptText("텍스트 입력", "");
            if (text == null) return null;
            return new Item("text", text);
        }
        return new Item(key);
    }

    private void CompactEmptyRows()
    {
        // 최소 1줄 유지. 빈 줄이 여러 개면 정리.
        if (_vm.Layout.Rows.Count <= 1) return;
        for (int i = _vm.Layout.Rows.Count - 1; i >= 0; i--)
        {
            if (_vm.Layout.Rows[i].Items.Count == 0 && _vm.Layout.Rows.Count > 1)
                _vm.Layout.Rows.RemoveAt(i);
        }
    }

    private static T? FindDescendant<T>(DependencyObject root, Func<T, bool>? predicate = null) where T : DependencyObject
    {
        int n = VisualTreeHelper.GetChildrenCount(root);
        for (int i = 0; i < n; i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is T t && (predicate == null || predicate(t))) return t;
            var d = FindDescendant<T>(child, predicate);
            if (d != null) return d;
        }
        return null;
    }

    private static string? PromptText(string title, string defaultValue)
    {
        var w = new Window
        {
            Title = title,
            Width = 380, Height = 130,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Owner = Application.Current.MainWindow,
            ResizeMode = ResizeMode.NoResize
        };
        var grid = new Grid { Margin = new Thickness(12) };
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(8) });
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        var tb = new TextBox { Text = defaultValue };
        Grid.SetRow(tb, 0);
        grid.Children.Add(tb);

        var btnPanel = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        Grid.SetRow(btnPanel, 2);
        var ok = new Button { Content = "확인", Width = 75, Height = 28, Margin = new Thickness(0, 0, 6, 0), IsDefault = true };
        var cancel = new Button { Content = "취소", Width = 75, Height = 28, IsCancel = true };
        btnPanel.Children.Add(ok); btnPanel.Children.Add(cancel);
        grid.Children.Add(btnPanel);
        w.Content = grid;

        string? result = null;
        ok.Click += (s, e) => { result = tb.Text; w.DialogResult = true; };
        tb.Focus(); tb.SelectAll();
        var ok2 = w.ShowDialog();
        return ok2 == true ? result : null;
    }
}

internal enum DragKind { Palette, PreviewItem, Row }
internal class DragPayload
{
    public DragKind Kind { get; init; }
    public string? PaletteKey { get; init; }
    public Item? Item { get; init; }
    public Row? SourceRow { get; init; }
    public Row? RowRef { get; init; }
}
