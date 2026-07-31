namespace IssuesKit;

/// <summary>
/// Base type for failures raised while reading or writing an issues document.
/// </summary>
/// <remarks>
/// Every message is user-facing. A file written by a newer build must explain itself
/// ("this document uses issues format version 2…") rather than surface as a generic
/// "corrupt file", so the UI can present <see cref="Exception.Message"/> verbatim.
/// </remarks>
public abstract class IssuesException(string message, Exception? innerException = null)
    : Exception(message, innerException);

/// <summary>The JSON has no <c>schemaVersion</c>, so it is not an <c>.issues</c> document.</summary>
public sealed class MissingSchemaVersionException()
    : IssuesException("The file does not declare a \"schemaVersion\" and is not a valid issues document.");

/// <summary>The document was written by a newer build than this one can read.</summary>
public sealed class UnsupportedSchemaVersionException(int found, int supported)
    : IssuesException(
        $"This document uses issues format version {found}, but this version of the app "
        + $"only reads up to version {supported}. Update the app to open it.")
{
    /// <summary>The version the document declares.</summary>
    public int Found { get; } = found;

    /// <summary>The highest version this build understands.</summary>
    public int Supported { get; } = supported;
}

/// <summary>The JSON could not be decoded; the detail is the underlying decoding failure.</summary>
public sealed class DecodingFailedException(string detail, Exception? innerException = null)
    : IssuesException($"The issues document could not be read: {detail}", innerException)
{
    public string Detail { get; } = detail;
}

/// <summary>The model could not be encoded; the detail is the underlying encoding failure.</summary>
public sealed class EncodingFailedException(string detail, Exception? innerException = null)
    : IssuesException($"The issues document could not be written: {detail}", innerException)
{
    public string Detail { get; } = detail;
}

/// <summary>
/// A legacy markdown file has no level-2 headings, so it is not a recognizable issues file.
/// </summary>
public sealed class MissingOpenSectionException()
    : IssuesException("The markdown file does not contain an \"## Open\" section and cannot be imported.");
