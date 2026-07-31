using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace IHaveIssues;

public partial class App : Application
{
    /// <summary>Where unhandled failures are recorded, so a crash leaves something to read.</summary>
    private static string LogPath => Path.Combine(
        System.Environment.GetFolderPath(System.Environment.SpecialFolder.LocalApplicationData),
        "IHaveIssues",
        "error.log");

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        DispatcherUnhandledException += OnUnhandledException;

        // The window is created here rather than by StartupUri so a path passed on the command line
        // — which is how Explorer opens a double-clicked .issues file — can be handed to it.
        var path = e.Args.Length > 0 ? e.Args[0] : null;
        MainWindow = new MainWindow(path);
        MainWindow.Show();
    }

    /// <summary>
    /// Reports a failure that reached the dispatcher instead of letting the process disappear, and
    /// keeps the app alive so the user still has a chance to save. The full exception goes to a log
    /// file — the dialog shows only the message, which is rarely enough to diagnose anything.
    /// </summary>
    private void OnUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        var detail = Record(e.Exception);
        MessageBox.Show(
            $"{e.Exception.Message}\n\n{detail}",
            "I Have Issues",
            MessageBoxButton.OK,
            MessageBoxImage.Error);
        e.Handled = true;
    }

    /// <summary>Appends the exception to the log, returning a line describing where it went.</summary>
    private static string Record(Exception exception)
    {
        try
        {
            var path = LogPath;
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.AppendAllText(path, $"{DateTimeOffset.Now:O}\n{exception}\n\n");
            return $"Details were written to {path}";
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            return "The failure could not be written to a log file.";
        }
    }
}
