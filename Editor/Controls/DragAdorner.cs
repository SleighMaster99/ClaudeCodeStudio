using System.Windows;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Shapes;

namespace StatusBarEditor.Controls;

public class DragAdorner : Adorner
{
    private readonly Rectangle _ghost;
    private Point _position;

    public DragAdorner(UIElement adornedElement, FrameworkElement source, double opacity = 0.65)
        : base(adornedElement)
    {
        IsHitTestVisible = false;

        var brush = new VisualBrush(source)
        {
            Stretch = Stretch.None,
            AlignmentX = AlignmentX.Left,
            AlignmentY = AlignmentY.Top
        };
        _ghost = new Rectangle
        {
            Width = source.ActualWidth,
            Height = source.ActualHeight,
            Fill = brush,
            Opacity = opacity,
            IsHitTestVisible = false,
            Effect = new System.Windows.Media.Effects.DropShadowEffect
            {
                BlurRadius = 8,
                ShadowDepth = 2,
                Opacity = 0.4
            }
        };
        AddVisualChild(_ghost);
    }

    public void UpdatePosition(Point pos)
    {
        _position = pos;
        var layer = AdornerLayer.GetAdornerLayer(AdornedElement);
        layer?.Update(AdornedElement);
    }

    protected override int VisualChildrenCount => 1;
    protected override Visual GetVisualChild(int index) => _ghost;

    protected override Size MeasureOverride(Size constraint)
    {
        _ghost.Measure(constraint);
        return _ghost.DesiredSize;
    }

    protected override Size ArrangeOverride(Size finalSize)
    {
        _ghost.Arrange(new Rect(new Point(0, 0), _ghost.DesiredSize));
        return finalSize;
    }

    public override GeneralTransform GetDesiredTransform(GeneralTransform transform)
    {
        var group = new GeneralTransformGroup();
        if (transform != null) group.Children.Add(transform);
        group.Children.Add(new TranslateTransform(_position.X, _position.Y));
        return group;
    }
}
