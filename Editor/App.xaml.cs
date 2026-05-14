using System.Windows;
using System.Windows.Threading;

namespace StatusBarEditor;

public partial class App : Application
{
    public App()
    {
        DispatcherUnhandledException += OnDispatcherUnhandledException;
    }

    private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        MessageBox.Show(
            $"[{e.Exception.GetType().Name}] {e.Exception.Message}\n\n{e.Exception.StackTrace}",
            "예외 발생",
            MessageBoxButton.OK,
            MessageBoxImage.Error);
        e.Handled = true;
    }
}
