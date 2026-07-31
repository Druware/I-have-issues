using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace IssuesKit.Json;

/// <summary>
/// Reads and writes the <c>.issues</c> JSON document format.
/// </summary>
/// <remarks>
/// Output is deliberately diff-friendly, because <c>.issues</c> files live in project
/// repositories: keys are sorted, the JSON is pretty-printed, slashes are not escaped, dates are
/// ISO-8601, and the file ends with a newline. Encoding the same model twice therefore produces
/// identical bytes — and the same bytes the macOS and iOS builds produce, so a document shared
/// between platforms does not churn.
/// <para>
/// Decoding is forward- and backward-tolerant: absent keys fall back to the documented default,
/// unknown keys are ignored, and unknown enum values fall back to that enum's default — with two
/// deliberate exceptions, <c>resolutionKind</c> and <c>remoteLinks[].provider</c>.
/// </para>
/// </remarks>
public static class IssuesJsonCoder
{
    private static readonly UTF8Encoding Utf8WithoutBom = new(encoderShouldEmitUTF8Identifier: false);

    // MARK: - Encoding

    /// <summary>Encodes a model to UTF-8 JSON with a trailing newline.</summary>
    /// <exception cref="EncodingFailedException">The model could not be rendered.</exception>
    public static byte[] Encode(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);

        try
        {
            return Utf8WithoutBom.GetBytes(EncodeToString(model));
        }
        catch (Exception error) when (error is not IssuesException)
        {
            throw new EncodingFailedException(error.Message, error);
        }
    }

    /// <summary>Encodes a model to a JSON string with a trailing newline.</summary>
    /// <exception cref="EncodingFailedException">The model could not be rendered.</exception>
    public static string EncodeToString(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);

        try
        {
            return SwiftJson.Render(EncodeDocument(model)) + "\n";
        }
        catch (Exception error) when (error is not IssuesException)
        {
            throw new EncodingFailedException(error.Message, error);
        }
    }

    private static JsonValue EncodeDocument(IssuesDocumentModel model) =>
        new JsonObjectBuilder()
            .Set("export", EncodeExport(model.Export))
            .Set("integrations", EncodeIntegrations(model.Integrations))
            .Set("issues", model.Issues, EncodeIssue)
            .Set("labels", model.Labels, EncodeLabel)
            .Set("milestones", model.Milestones, EncodeMilestone)
            .Set("people", model.People, EncodePerson)
            .Set("project", EncodeProject(model.Project))
            .Set("schemaVersion", model.SchemaVersion)
            .Build();

    private static JsonValue EncodeExport(ExportSettings export) =>
        new JsonObjectBuilder()
            .Set("preambleMarkdown", export.PreambleMarkdown)
            .Build();

    private static JsonValue EncodeProject(ProjectInfo project) =>
        new JsonObjectBuilder()
            .Set("id", project.Id)
            .Set("name", project.Name)
            .Set("summary", project.Summary)
            .Build();

    private static JsonValue EncodeIntegrations(IntegrationSettings integrations)
    {
        var builder = new JsonObjectBuilder();
        if (integrations.AzureDevOps is { } azure)
        {
            builder.Set("azureDevOps", new JsonObjectBuilder()
                .SetIfPresent("areaPath", azure.AreaPath)
                .Set("defaultWorkItemType", azure.DefaultWorkItemType)
                .SetIfPresent("iterationPath", azure.IterationPath)
                .Set("organization", azure.Organization)
                .Set("project", azure.Project)
                .SetIfPresent("team", azure.Team)
                .Build());
        }

        if (integrations.GitHub is { } github)
        {
            builder.Set("github", new JsonObjectBuilder()
                .Set("defaultAssignees", github.DefaultAssignees)
                .Set("defaultLabels", github.DefaultLabels)
                .SetIfPresent("defaultMilestone", github.DefaultMilestone)
                .Set("owner", github.Owner)
                .Set("repository", github.Repository)
                .Build());
        }

        return builder.Build();
    }

    private static JsonValue EncodeLabel(LabelDefinition label) =>
        new JsonObjectBuilder()
            .SetIfPresent("colorHex", label.ColorHex)
            .Set("description", label.Description)
            .Set("name", label.Name)
            .Build();

    private static JsonValue EncodeMilestone(Milestone milestone) =>
        new JsonObjectBuilder()
            .SetIfPresent("dueOn", milestone.DueOn)
            .Set("isClosed", milestone.IsClosed)
            .Set("name", milestone.Name)
            .Build();

    private static JsonValue EncodePerson(Person person) =>
        new JsonObjectBuilder()
            .Set("displayName", person.DisplayName)
            .Set("email", person.Email)
            .Set("handle", person.Handle)
            .Build();

    private static JsonValue EncodeIssue(Issue issue) =>
        new JsonObjectBuilder()
            .Set("area", issue.Area)
            .Set("assignees", issue.Assignees)
            .SetIfPresent("closedAt", issue.ClosedAt)
            .Set("comments", issue.Comments, EncodeComment)
            .Set("createdAt", issue.CreatedAt)
            .Set("description", issue.Description)
            .Set("environment", issue.Environment)
            .SetIfPresent("estimate", issue.Estimate)
            .Set("labels", issue.Labels)
            .SetIfPresent("milestone", issue.Milestone)
            .Set("notes", issue.Notes)
            .Set("number", issue.Number)
            .Set("priority", issue.Priority.RawValue())
            .Set("relations", issue.Relations, EncodeRelation)
            .Set("remoteLinks", issue.RemoteLinks, EncodeRemoteLink)
            .Set("reported", issue.Reported)
            .Set("reportedBy", issue.ReportedBy)
            .Set("resolution", issue.Resolution)
            .SetIfPresent("resolutionKind", issue.ResolutionKind?.RawValue())
            .Set("status", issue.Status.RawValue())
            .Set("stepsToReproduce", issue.StepsToReproduce)
            .Set("title", issue.Title)
            .Set("type", issue.Type.RawValue())
            .Set("updatedAt", issue.UpdatedAt)
            .Set("uuid", issue.Uuid)
            .Build();

    private static JsonValue EncodeComment(Comment comment) =>
        new JsonObjectBuilder()
            .Set("author", comment.Author)
            .Set("body", comment.Body)
            .Set("createdAt", comment.CreatedAt)
            .Set("id", comment.Id)
            .Build();

    private static JsonValue EncodeRelation(Relation relation) =>
        new JsonObjectBuilder()
            .Set("issueID", relation.IssueId)
            .Set("kind", relation.Kind.RawValue())
            .Build();

    private static JsonValue EncodeRemoteLink(RemoteLink link) =>
        new JsonObjectBuilder()
            .Set("identifier", link.Identifier)
            .SetIfPresent("lastSyncedAt", link.LastSyncedAt)
            .Set("provider", link.Provider.RawValue)
            .SetIfPresent("remoteUpdatedAt", link.RemoteUpdatedAt)
            .SetIfPresent("url", link.Url)
            .Build();

    // MARK: - Decoding

    /// <summary>Decodes UTF-8 JSON into a model.</summary>
    /// <exception cref="MissingSchemaVersionException">The document declares no schema version.</exception>
    /// <exception cref="UnsupportedSchemaVersionException">The document was written by a newer build.</exception>
    /// <exception cref="DecodingFailedException">The JSON is malformed or a value has the wrong type.</exception>
    public static IssuesDocumentModel Decode(ReadOnlySpan<byte> data) =>
        Decode(Utf8WithoutBom.GetString(data));

    /// <inheritdoc cref="Decode(ReadOnlySpan{byte})"/>
    public static IssuesDocumentModel Decode(string json)
    {
        JsonNode? root;
        try
        {
            root = JsonNode.Parse(json);
        }
        catch (JsonException error)
        {
            throw new DecodingFailedException(error.Message, error);
        }

        if (root is not JsonObject document)
        {
            throw new DecodingFailedException("The top level of the document is not a JSON object.");
        }

        if (document["schemaVersion"] is not { } versionNode)
        {
            throw new MissingSchemaVersionException();
        }

        var version = AsInt(versionNode, "schemaVersion");
        if (version > IssuesDocumentModel.SupportedSchemaVersion)
        {
            throw new UnsupportedSchemaVersionException(version, IssuesDocumentModel.SupportedSchemaVersion);
        }

        // Migration seam: only version 1 exists today, so an older version is read as-is. When
        // version 2 lands, branch here — decode the old shape, transform it, and set SchemaVersion
        // to the migrated value so the next save writes the new shape.

        return new IssuesDocumentModel
        {
            SchemaVersion = version,
            Project = DecodeProject(Object(document, "project")),
            Integrations = DecodeIntegrations(Object(document, "integrations")),
            Labels = DecodeList(document, "labels", DecodeLabel),
            Milestones = DecodeList(document, "milestones", DecodeMilestone),
            People = DecodeList(document, "people", DecodePerson),
            Export = DecodeExport(Object(document, "export")),
            Issues = DecodeList(document, "issues", DecodeIssue)
        };
    }

    private static ProjectInfo DecodeProject(JsonObject? node) => node is null
        ? new ProjectInfo()
        : new ProjectInfo
        {
            Id = GuidOr(node, "id", System.Guid.NewGuid()),
            Name = StringOr(node, "name", string.Empty),
            Summary = StringOr(node, "summary", string.Empty)
        };

    private static ExportSettings DecodeExport(JsonObject? node) => node is null
        ? new ExportSettings()
        : new ExportSettings
        {
            PreambleMarkdown = StringOr(node, "preambleMarkdown", ExportSettings.DefaultPreambleMarkdown)
        };

    private static IntegrationSettings DecodeIntegrations(JsonObject? node)
    {
        if (node is null)
        {
            return new IntegrationSettings();
        }

        var settings = new IntegrationSettings();

        if (Object(node, "github") is { } github)
        {
            settings.GitHub = new GitHubIntegration
            {
                Owner = StringOr(github, "owner", string.Empty),
                Repository = StringOr(github, "repository", string.Empty),
                DefaultLabels = StringList(github, "defaultLabels"),
                DefaultAssignees = StringList(github, "defaultAssignees"),
                DefaultMilestone = StringOrNull(github, "defaultMilestone")
            };
        }

        if (Object(node, "azureDevOps") is { } azure)
        {
            settings.AzureDevOps = new AzureDevOpsIntegration
            {
                Organization = StringOr(azure, "organization", string.Empty),
                Project = StringOr(azure, "project", string.Empty),
                Team = StringOrNull(azure, "team"),
                AreaPath = StringOrNull(azure, "areaPath"),
                IterationPath = StringOrNull(azure, "iterationPath"),
                DefaultWorkItemType = StringOr(azure, "defaultWorkItemType", "Issue")
            };
        }

        return settings;
    }

    private static LabelDefinition DecodeLabel(JsonObject node) => new()
    {
        Name = StringOr(node, "name", string.Empty),
        ColorHex = StringOrNull(node, "colorHex"),
        Description = StringOr(node, "description", string.Empty)
    };

    private static Milestone DecodeMilestone(JsonObject node) => new()
    {
        Name = StringOr(node, "name", string.Empty),
        DueOn = DateOrNull(node, "dueOn"),
        IsClosed = BoolOr(node, "isClosed", false)
    };

    private static Person DecodePerson(JsonObject node) => new()
    {
        Handle = StringOr(node, "handle", string.Empty),
        DisplayName = StringOr(node, "displayName", string.Empty),
        Email = StringOr(node, "email", string.Empty)
    };

    private static Issue DecodeIssue(JsonObject node)
    {
        var now = DateTimeOffset.UtcNow;
        return new Issue
        {
            Uuid = GuidOr(node, "uuid", System.Guid.NewGuid()),
            Number = IntOr(node, "number", 0),
            Title = StringOr(node, "title", string.Empty),
            Type = IssueEnums.IssueTypeFromRaw(StringOrNull(node, "type")),
            Priority = IssueEnums.IssuePriorityFromRaw(StringOrNull(node, "priority")),
            Status = IssueEnums.IssueStatusFromRaw(StringOrNull(node, "status")),
            // An unrecognized resolution decodes to null rather than a default.
            ResolutionKind = IssueEnums.ResolutionKindFromRaw(StringOrNull(node, "resolutionKind")),
            Labels = StringList(node, "labels"),
            Assignees = StringList(node, "assignees"),
            Milestone = StringOrNull(node, "milestone"),
            Area = StringOr(node, "area", string.Empty),
            Estimate = DoubleOrNull(node, "estimate"),
            ReportedBy = StringOr(node, "reportedBy", string.Empty),
            Reported = DateOrNull(node, "reported") ?? now,
            CreatedAt = DateOrNull(node, "createdAt") ?? now,
            UpdatedAt = DateOrNull(node, "updatedAt") ?? now,
            ClosedAt = DateOrNull(node, "closedAt"),
            Description = StringOr(node, "description", string.Empty),
            StepsToReproduce = StringList(node, "stepsToReproduce"),
            Environment = StringOr(node, "environment", string.Empty),
            Notes = StringOr(node, "notes", string.Empty),
            Resolution = StringOr(node, "resolution", string.Empty),
            Comments = DecodeList(node, "comments", DecodeComment),
            Relations = DecodeList(node, "relations", DecodeRelation),
            RemoteLinks = DecodeList(node, "remoteLinks", DecodeRemoteLink)
        };
    }

    private static Comment DecodeComment(JsonObject node) => new()
    {
        Id = GuidOr(node, "id", System.Guid.NewGuid()),
        Author = StringOr(node, "author", string.Empty),
        CreatedAt = DateOrNull(node, "createdAt") ?? DateTimeOffset.UtcNow,
        Body = StringOr(node, "body", string.Empty)
    };

    /// <summary>
    /// <c>issueID</c> is required — a relation that points nowhere is not a relation.
    /// </summary>
    private static Relation DecodeRelation(JsonObject node)
    {
        if (node["issueID"] is not { } target)
        {
            throw new DecodingFailedException("A relation is missing its required \"issueID\".");
        }

        return new Relation(
            IssueEnums.RelationKindFromRaw(StringOrNull(node, "kind")),
            ParseGuid(AsString(target, "issueID"), "issueID"));
    }

    /// <summary>
    /// An unrecognized provider is preserved verbatim; it is never coerced to a known provider.
    /// </summary>
    private static RemoteLink DecodeRemoteLink(JsonObject node) => new()
    {
        Provider = new RemoteProvider(StringOrNull(node, "provider") ?? RemoteProvider.GitHub.RawValue),
        Identifier = StringOr(node, "identifier", string.Empty),
        Url = UriOrNull(node, "url"),
        LastSyncedAt = DateOrNull(node, "lastSyncedAt"),
        RemoteUpdatedAt = DateOrNull(node, "remoteUpdatedAt")
    };

    // MARK: - Tolerant readers

    private static JsonObject? Object(JsonObject parent, string key) => parent[key] as JsonObject;

    private static List<T> DecodeList<T>(JsonObject parent, string key, Func<JsonObject, T> decode)
    {
        if (parent[key] is not JsonArray array)
        {
            return [];
        }

        var results = new List<T>(array.Count);
        foreach (var item in array)
        {
            if (item is JsonObject element)
            {
                results.Add(decode(element));
            }
        }

        return results;
    }

    private static List<string> StringList(JsonObject parent, string key)
    {
        if (parent[key] is not JsonArray array)
        {
            return [];
        }

        var results = new List<string>(array.Count);
        foreach (var item in array)
        {
            if (item is not null)
            {
                results.Add(AsString(item, key));
            }
        }

        return results;
    }

    private static string StringOr(JsonObject parent, string key, string fallback) =>
        parent[key] is { } node ? AsString(node, key) : fallback;

    private static string? StringOrNull(JsonObject parent, string key) =>
        parent[key] is { } node ? AsString(node, key) : null;

    private static int IntOr(JsonObject parent, string key, int fallback) =>
        parent[key] is { } node ? AsInt(node, key) : fallback;

    private static bool BoolOr(JsonObject parent, string key, bool fallback)
    {
        if (parent[key] is not { } node)
        {
            return fallback;
        }

        try
        {
            return node.GetValue<bool>();
        }
        catch (Exception error) when (error is InvalidOperationException or FormatException)
        {
            throw new DecodingFailedException($"\"{key}\" is not a boolean.", error);
        }
    }

    private static double? DoubleOrNull(JsonObject parent, string key)
    {
        if (parent[key] is not { } node)
        {
            return null;
        }

        try
        {
            return node.GetValue<double>();
        }
        catch (Exception error) when (error is InvalidOperationException or FormatException)
        {
            throw new DecodingFailedException($"\"{key}\" is not a number.", error);
        }
    }

    private static Guid GuidOr(JsonObject parent, string key, Guid fallback) =>
        parent[key] is { } node ? ParseGuid(AsString(node, key), key) : fallback;

    private static DateTimeOffset? DateOrNull(JsonObject parent, string key)
    {
        if (parent[key] is not { } node)
        {
            return null;
        }

        var text = AsString(node, key);
        if (!DateTimeOffset.TryParse(
                text,
                CultureInfo.InvariantCulture,
                DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal,
                out var value))
        {
            throw new DecodingFailedException($"\"{key}\" is not an ISO-8601 date: \"{text}\".");
        }

        return value;
    }

    /// <summary>A malformed URL is dropped rather than failing the decode, matching the reference build.</summary>
    private static Uri? UriOrNull(JsonObject parent, string key)
    {
        if (parent[key] is not { } node)
        {
            return null;
        }

        return Uri.TryCreate(AsString(node, key), UriKind.RelativeOrAbsolute, out var uri) ? uri : null;
    }

    private static string AsString(JsonNode node, string key)
    {
        try
        {
            return node.GetValue<string>();
        }
        catch (Exception error) when (error is InvalidOperationException or FormatException)
        {
            throw new DecodingFailedException($"\"{key}\" is not a string.", error);
        }
    }

    private static int AsInt(JsonNode node, string key)
    {
        try
        {
            return node.GetValue<int>();
        }
        catch (Exception error) when (error is InvalidOperationException or FormatException)
        {
            throw new DecodingFailedException($"\"{key}\" is not an integer.", error);
        }
    }

    private static Guid ParseGuid(string text, string key)
    {
        if (!System.Guid.TryParse(text, out var value))
        {
            throw new DecodingFailedException($"\"{key}\" is not a UUID: \"{text}\".");
        }

        return value;
    }
}
