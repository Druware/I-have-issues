namespace IssuesKit;

/// <summary>
/// List comparison for the document model's value equality.
/// </summary>
/// <remarks>
/// The model mirrors a Swift value-type model where equality is structural. C# lists compare by
/// reference, so every model type that owns a list routes through here.
/// </remarks>
internal static class ModelEquality
{
    /// <summary>Whether two lists hold equal elements in the same order.</summary>
    public static bool ListEquals<T>(List<T> left, List<T> right) =>
        ReferenceEquals(left, right) || left.SequenceEqual(right);

    /// <summary>A copy of every element, for the draft-editing pattern.</summary>
    public static List<T> CopyOf<T>(List<T> source, Func<T, T> copyElement) =>
        source.Select(copyElement).ToList();
}
