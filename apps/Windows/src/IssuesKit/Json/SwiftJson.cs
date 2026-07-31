using System.Globalization;
using System.Text;

namespace IssuesKit.Json;

/// <summary>
/// A JSON value being built for output.
/// </summary>
/// <remarks>
/// Deliberately not <c>System.Text.Json</c>: the <c>.issues</c> format is defined by what the
/// reference Swift implementation emits, and <c>Utf8JsonWriter</c> cannot produce it. See
/// <see cref="SwiftJson"/> for the specific differences.
/// </remarks>
internal abstract record JsonValue
{
    internal sealed record Object(List<KeyValuePair<string, JsonValue>> Members) : JsonValue;

    internal sealed record Array(List<JsonValue> Items) : JsonValue;

    internal sealed record Text(string Value) : JsonValue;

    /// <summary>A number carried as its already-rendered literal, so formatting happens once.</summary>
    internal sealed record Number(string Literal) : JsonValue;

    internal sealed record Boolean(bool Value) : JsonValue;
}

/// <summary>
/// Builds a JSON object member by member. <c>Set</c> always writes the key; <c>SetIfPresent</c>
/// omits it when the value is <c>null</c>, matching how the reference implementation encodes
/// optionals.
/// </summary>
/// <remarks>
/// Members may be added in any order — <see cref="SwiftJson.Render"/> sorts keys on output.
/// </remarks>
internal sealed class JsonObjectBuilder
{
    private readonly List<KeyValuePair<string, JsonValue>> _members = [];

    public JsonValue.Object Build() => new(_members);

    private JsonObjectBuilder Add(string key, JsonValue value)
    {
        _members.Add(new KeyValuePair<string, JsonValue>(key, value));
        return this;
    }

    // MARK: - Always written

    public JsonObjectBuilder Set(string key, string value) => Add(key, new JsonValue.Text(value));

    public JsonObjectBuilder Set(string key, int value) =>
        Add(key, new JsonValue.Number(value.ToString(CultureInfo.InvariantCulture)));

    public JsonObjectBuilder Set(string key, bool value) => Add(key, new JsonValue.Boolean(value));

    public JsonObjectBuilder Set(string key, Guid value) => Add(key, new JsonValue.Text(SwiftJson.Guid(value)));

    public JsonObjectBuilder Set(string key, DateTimeOffset value) =>
        Add(key, new JsonValue.Text(SwiftJson.Date(value)));

    public JsonObjectBuilder Set(string key, JsonValue value) => Add(key, value);

    public JsonObjectBuilder Set(string key, IEnumerable<string> values) =>
        Add(key, new JsonValue.Array([.. values.Select(value => (JsonValue)new JsonValue.Text(value))]));

    public JsonObjectBuilder Set<T>(string key, IEnumerable<T> values, Func<T, JsonValue> encode) =>
        Add(key, new JsonValue.Array([.. values.Select(encode)]));

    // MARK: - Omitted when absent

    public JsonObjectBuilder SetIfPresent(string key, string? value) =>
        value is null ? this : Set(key, value);

    public JsonObjectBuilder SetIfPresent(string key, DateTimeOffset? value) =>
        value is null ? this : Set(key, value.Value);

    public JsonObjectBuilder SetIfPresent(string key, JsonValue? value) =>
        value is null ? this : Set(key, value);

    public JsonObjectBuilder SetIfPresent(string key, double? value) =>
        value is null ? this : Add(key, new JsonValue.Number(SwiftJson.Double(value.Value)));

    public JsonObjectBuilder SetIfPresent(string key, Uri? value) =>
        value is null ? this : Set(key, value.OriginalString);
}

/// <summary>
/// Renders JSON exactly as the reference Swift implementation does, so an <c>.issues</c> file
/// written on Windows is byte-identical to one written on macOS and a shared file does not churn
/// in git when it moves between platforms.
/// </summary>
/// <remarks>
/// Foundation's <c>JSONEncoder</c> with <c>[.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]</c>
/// differs from <c>Utf8JsonWriter</c> in four ways, all reproduced here:
/// <list type="bullet">
/// <item>a space on both sides of the name separator — <c>"key" : value</c>, not <c>"key": value</c>;</item>
/// <item>an empty object or array spans three lines, with a blank line between the brackets;</item>
/// <item>only <c>"</c>, <c>\</c>, and control characters are escaped — slashes and non-ASCII are literal;</item>
/// <item>two-space indentation, LF line endings, and keys sorted by ordinal comparison.</item>
/// </list>
/// </remarks>
internal static class SwiftJson
{
    private const string Indent = "  ";

    /// <summary>Renders a value as a complete JSON document, without a trailing newline.</summary>
    public static string Render(JsonValue value)
    {
        var builder = new StringBuilder();
        Write(builder, value, depth: 0);
        return builder.ToString();
    }

    private static void Write(StringBuilder builder, JsonValue value, int depth)
    {
        switch (value)
        {
            case JsonValue.Object obj:
                WriteMembers(
                    builder,
                    depth,
                    '{',
                    '}',
                    // Ordinal ordering is what `.sortedKeys` produces; a culture-aware sort would
                    // put keys in a different order on some machines and break byte-identity.
                    [.. obj.Members.OrderBy(member => member.Key, StringComparer.Ordinal)],
                    (target, member, childDepth) =>
                    {
                        WriteString(target, member.Key);
                        target.Append(" : ");
                        Write(target, member.Value, childDepth);
                    });
                break;

            case JsonValue.Array array:
                WriteMembers(
                    builder,
                    depth,
                    '[',
                    ']',
                    array.Items,
                    (target, item, childDepth) => Write(target, item, childDepth));
                break;

            case JsonValue.Text text:
                WriteString(builder, text.Value);
                break;

            case JsonValue.Number number:
                builder.Append(number.Literal);
                break;

            case JsonValue.Boolean boolean:
                builder.Append(boolean.Value ? "true" : "false");
                break;

            default:
                throw new InvalidOperationException($"Unhandled JSON value {value.GetType().Name}.");
        }
    }

    private static void WriteMembers<T>(
        StringBuilder builder,
        int depth,
        char open,
        char close,
        List<T> items,
        Action<StringBuilder, T, int> writeItem)
    {
        builder.Append(open).Append('\n');

        for (var index = 0; index < items.Count; index++)
        {
            AppendIndent(builder, depth + 1);
            writeItem(builder, items[index], depth + 1);
            if (index < items.Count - 1)
            {
                builder.Append(',');
            }

            builder.Append('\n');
        }

        // An empty container still emits the blank line the reference encoder leaves behind.
        if (items.Count == 0)
        {
            builder.Append('\n');
        }

        AppendIndent(builder, depth);
        builder.Append(close);
    }

    private static void AppendIndent(StringBuilder builder, int depth)
    {
        for (var level = 0; level < depth; level++)
        {
            builder.Append(Indent);
        }
    }

    /// <summary>
    /// Escapes only what JSON requires. Slashes stay literal (<c>.withoutEscapingSlashes</c>) and
    /// non-ASCII is emitted as-is, so em dashes and accented text survive readable in the file.
    /// </summary>
    private static void WriteString(StringBuilder builder, string value)
    {
        builder.Append('"');
        foreach (var character in value)
        {
            switch (character)
            {
                case '"':
                    builder.Append("\\\"");
                    break;
                case '\\':
                    builder.Append("\\\\");
                    break;
                case '\n':
                    builder.Append("\\n");
                    break;
                case '\r':
                    builder.Append("\\r");
                    break;
                case '\t':
                    builder.Append("\\t");
                    break;
                case '\b':
                    builder.Append("\\b");
                    break;
                case '\f':
                    builder.Append("\\f");
                    break;
                default:
                    if (character < 0x20)
                    {
                        builder.Append(CultureInfo.InvariantCulture, $"\\u{(int)character:x4}");
                    }
                    else
                    {
                        builder.Append(character);
                    }

                    break;
            }
        }

        builder.Append('"');
    }

    // MARK: - Scalar formatting

    /// <summary>
    /// ISO-8601 at second precision in UTC, e.g. <c>2026-05-01T00:00:00Z</c>.
    /// Sub-second precision is truncated — a timestamp does not survive a round trip at finer
    /// than one-second resolution.
    /// </summary>
    public static string Date(DateTimeOffset value) =>
        value.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", CultureInfo.InvariantCulture);

    /// <summary>Uppercase, hyphen-separated — the form the reference implementation writes.</summary>
    public static string Guid(Guid value) =>
        value.ToString("D", CultureInfo.InvariantCulture).ToUpperInvariant();

    /// <summary>
    /// The shortest literal that round trips. A whole number loses its fractional part, so an
    /// estimate of <c>5.0</c> is written <c>5</c> and re-reads as <c>5</c>.
    /// </summary>
    public static string Double(double value) =>
        value.ToString("R", CultureInfo.InvariantCulture);
}
