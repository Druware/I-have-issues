using System.Diagnostics;

namespace IHaveIssues.Presentation;

/// <summary>
/// Opens a link from a document in the user's browser.
/// </summary>
/// <remarks>
/// Only <c>http</c>, <c>https</c>, and <c>mailto</c> are followed. Links reach this app from
/// <c>.issues</c> files, which are pulled from repositories and written by other people and by
/// coding agents; handing an arbitrary URI to the shell would let a document launch a local file or
/// a registered protocol handler just because someone clicked a link that looked ordinary.
/// </remarks>
public static class ExternalLink
{
    /// <summary>Whether <paramref name="uri"/> is one this app is willing to hand to the shell.</summary>
    public static bool IsSafe(Uri? uri) =>
        uri is { IsAbsoluteUri: true }
        && (uri.Scheme == Uri.UriSchemeHttp
            || uri.Scheme == Uri.UriSchemeHttps
            || uri.Scheme == Uri.UriSchemeMailto);

    /// <summary>Opens the link, doing nothing at all when its scheme is not allowed.</summary>
    public static void Open(Uri? uri)
    {
        if (!IsSafe(uri))
        {
            return;
        }

        try
        {
            Process.Start(new ProcessStartInfo(uri!.AbsoluteUri) { UseShellExecute = true });
        }
        catch (Exception error) when (error is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
            // No browser is registered, or the shell refused. Nothing useful to tell the user.
        }
    }
}
