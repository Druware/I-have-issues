using System.Text;

namespace IssuesKit.Tests;

/// <summary>
/// Reads the files copied verbatim from the Apple project, so both platforms are exercised
/// against the same ground truth.
/// </summary>
/// <remarks>
/// Line endings are normalized to LF on read. The canonical committed form of these files is LF —
/// that is what the format specifies and what both apps write — but git checks them out with CRLF
/// wherever <c>core.autocrlf</c> is on, which is the default on Windows. Comparing against the raw
/// working-tree bytes would make byte-identity tests pass or fail according to a git setting rather
/// than according to the encoder. The sibling <c>.gitattributes</c> asks git to leave them alone;
/// this normalization keeps the suite honest even where that is overridden.
/// </remarks>
internal static class Fixtures
{
    /// <summary>The canonical LF bytes of a fixture, for byte-for-byte comparisons.</summary>
    public static byte[] Bytes(string fileName) => Encoding.UTF8.GetBytes(Text(fileName));

    /// <summary>The canonical LF text of a fixture.</summary>
    public static string Text(string fileName)
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Fixtures", fileName);
        return File.ReadAllText(path).Replace("\r\n", "\n", StringComparison.Ordinal);
    }
}
