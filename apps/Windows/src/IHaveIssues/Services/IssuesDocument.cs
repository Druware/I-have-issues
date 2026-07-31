using System.IO;
using System.Text;
using IHaveIssues.Mvvm;
using IssuesKit;
using IssuesKit.Json;

namespace IHaveIssues.Services;

/// <summary>
/// An open <c>.issues</c> document: the parsed model, where it came from, and whether it has
/// unsaved changes.
/// </summary>
/// <remarks>
/// The Apple build gets open, save, and dirty tracking from SwiftUI's <c>DocumentGroup</c>. WPF has
/// no equivalent, so this type provides them.
/// <para>
/// "Dirty" is decided by comparing the encoded document with the bytes last written to disk. That
/// is exact rather than approximate — the coder is deterministic, so identical models always encode
/// identically — and it means undoing an edit by hand correctly clears the indicator.
/// </para>
/// </remarks>
public sealed class IssuesDocument : ObservableObject
{
    private static readonly UTF8Encoding Utf8WithoutBom = new(encoderShouldEmitUTF8Identifier: false);

    private string _savedJson;

    private IssuesDocument(IssuesDocumentModel model, string? filePath)
    {
        Model = model;
        FilePath = filePath;
        _savedJson = IssuesJsonCoder.EncodeToString(model);
    }

    /// <summary>The parsed document. Mutate it, then call <see cref="MarkChanged"/>.</summary>
    public IssuesDocumentModel Model { get; private set; }

    /// <summary>Where the document was opened from or last saved to, or <c>null</c> when untitled.</summary>
    public string? FilePath { get; private set; }

    /// <summary>The file name for the window title, or a placeholder when the document is untitled.</summary>
    public string DisplayName => FilePath is null ? "Untitled" : Path.GetFileName(FilePath);

    /// <summary>Whether the in-memory document differs from what is on disk.</summary>
    public bool IsDirty => !string.Equals(IssuesJsonCoder.EncodeToString(Model), _savedJson, StringComparison.Ordinal);

    /// <summary>An empty document at the current schema version.</summary>
    public static IssuesDocument New() => new(IssuesDocumentModel.MakeEmpty(), filePath: null);

    /// <summary>
    /// Reads an <c>.issues</c> file.
    /// </summary>
    /// <exception cref="IssuesException">
    /// The file is not a valid issues document. The message explains why — a document written by a
    /// newer build says so, rather than surfacing as a generic "corrupt file".
    /// </exception>
    /// <exception cref="IOException">The file could not be read.</exception>
    public static IssuesDocument Open(string path) =>
        new(IssuesJsonCoder.Decode(File.ReadAllBytes(path)), path);

    /// <summary>
    /// Replaces the document's contents, as "Import Legacy Markdown…" does. The path is kept, so the
    /// next save writes the imported document over the file that is already open.
    /// </summary>
    public void Replace(IssuesDocumentModel model)
    {
        Model = model;
        OnPropertyChanged(nameof(Model));
        MarkChanged();
    }

    /// <summary>Announces that <see cref="Model"/> was mutated, so the dirty indicator re-evaluates.</summary>
    public void MarkChanged() => OnPropertyChanged(nameof(IsDirty));

    /// <summary>
    /// Writes the document to <paramref name="path"/> and treats that as the new clean state.
    /// </summary>
    /// <exception cref="EncodingFailedException">The model could not be rendered.</exception>
    /// <exception cref="IOException">The file could not be written.</exception>
    public void Save(string path)
    {
        var json = IssuesJsonCoder.EncodeToString(Model);

        // Written through a temporary file in the same directory and moved into place, so an
        // interrupted save cannot leave a half-written document where the original used to be.
        var temporary = path + ".tmp";
        File.WriteAllText(temporary, json, Utf8WithoutBom);
        File.Move(temporary, path, overwrite: true);

        _savedJson = json;
        FilePath = path;
        OnPropertyChanged(nameof(FilePath));
        OnPropertyChanged(nameof(DisplayName));
        MarkChanged();
    }

    /// <summary>Renders the document as markdown and writes it to <paramref name="path"/>.</summary>
    /// <exception cref="IOException">The file could not be written.</exception>
    public void ExportMarkdown(string path) =>
        File.WriteAllText(path, IssuesMarkdownSerializer.Export(Model), Utf8WithoutBom);

    /// <summary>Reads a legacy markdown file into a fresh model, without applying it.</summary>
    /// <exception cref="MissingOpenSectionException">The file is not a recognizable issues file.</exception>
    /// <exception cref="IOException">The file could not be read.</exception>
    public static IssuesDocumentModel ReadLegacyMarkdown(string path) =>
        new LegacyMarkdownImporter().ImportDocument(File.ReadAllText(path));
}
