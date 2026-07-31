package com.druware.issueskit

import com.druware.issueskit.internal.FoundationJson
import com.druware.issueskit.internal.JsonValue
import java.time.Instant
import java.util.Locale
import java.util.UUID
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * Reads and writes the `.issues` JSON document format.
 *
 * Output is deliberately diff-friendly, because `.issues` files live in project repositories:
 * keys are sorted, the JSON is pretty-printed, slashes are not escaped, dates are ISO-8601, and
 * the file ends with a newline. Encoding the same model twice therefore produces identical bytes —
 * and, by construction, the same bytes the Apple app produces (see [FoundationJson]).
 *
 * Decoding is tolerant in exactly the ways the format documents: absent keys fall back to their
 * documented defaults at every nesting level, unknown keys are ignored, and unknown enum raw
 * values fall back to that enum's default. The two deliberate exceptions are
 * [Issue.resolutionKind], which decodes to `null` rather than inventing a close reason, and
 * [RemoteLink.provider], which preserves an unrecognized value verbatim because a provider is sync
 * identity. [Relation.issueID] is the one field with no default and its absence is fatal.
 */
object IssuesJSONCoder {

    private val parser = Json

    /** Encodes a model to UTF-8 JSON with a trailing newline. */
    fun encode(model: IssuesDocumentModel): ByteArray = encodeToString(model).toByteArray(Charsets.UTF_8)

    /** Encodes a model to JSON text with a trailing newline. */
    fun encodeToString(model: IssuesDocumentModel): String =
        FoundationJson.render(model.toJson()) + "\n"

    /**
     * Decodes UTF-8 JSON into a model.
     *
     * @throws IssuesError.MissingSchemaVersion when the document declares no `schemaVersion`.
     * @throws IssuesError.UnsupportedSchemaVersion when it was written by a newer build.
     * @throws IssuesError.DecodingFailed when the JSON is malformed or a value has the wrong type.
     */
    fun decode(data: ByteArray): IssuesDocumentModel = decode(data.toString(Charsets.UTF_8))

    /** Decodes JSON text into a model. See [decode]. */
    fun decode(text: String): IssuesDocumentModel {
        val root = try {
            parser.parseToJsonElement(text)
        } catch (error: Exception) {
            throw IssuesError.DecodingFailed(error.message ?: error.toString())
        }
        return documentFrom(root.expectObject("<root>"))
    }

    // MARK: - Decoding

    private fun documentFrom(json: JsonObject): IssuesDocumentModel {
        val version = json.int("schemaVersion") ?: throw IssuesError.MissingSchemaVersion
        if (version > IssuesDocumentModel.SUPPORTED_SCHEMA_VERSION) {
            throw IssuesError.UnsupportedSchemaVersion(version, IssuesDocumentModel.SUPPORTED_SCHEMA_VERSION)
        }
        // Migration seam: only version 1 exists today, so an older version is read as-is. When
        // version 2 lands, branch here — decode the old shape, transform it, and set
        // `schemaVersion` to the migrated value so the next save writes the new shape.

        return IssuesDocumentModel(
            schemaVersion = version,
            project = json.obj("project")?.let(::projectFrom) ?: ProjectInfo(),
            integrations = json.obj("integrations")?.let(::integrationsFrom) ?: IntegrationSettings(),
            labels = json.arr("labels")?.mapObjects("labels", ::labelFrom) ?: emptyList(),
            milestones = json.arr("milestones")?.mapObjects("milestones", ::milestoneFrom) ?: emptyList(),
            people = json.arr("people")?.mapObjects("people", ::personFrom) ?: emptyList(),
            export = json.obj("export")?.let(::exportFrom) ?: ExportSettings(),
            issues = json.arr("issues")?.mapObjects("issues", ::issueFrom) ?: emptyList(),
        )
    }

    private fun projectFrom(json: JsonObject) = ProjectInfo(
        id = json.uuid("id") ?: UUID.randomUUID(),
        name = json.string("name") ?: "",
        summary = json.string("summary") ?: "",
    )

    private fun exportFrom(json: JsonObject) = ExportSettings(
        preambleMarkdown = json.string("preambleMarkdown") ?: ExportSettings.DEFAULT_PREAMBLE_MARKDOWN,
    )

    private fun integrationsFrom(json: JsonObject) = IntegrationSettings(
        github = json.obj("github")?.let { source ->
            GitHubIntegration(
                owner = source.string("owner") ?: "",
                repository = source.string("repository") ?: "",
                defaultLabels = source.stringList("defaultLabels") ?: emptyList(),
                defaultAssignees = source.stringList("defaultAssignees") ?: emptyList(),
                defaultMilestone = source.string("defaultMilestone"),
            )
        },
        azureDevOps = json.obj("azureDevOps")?.let { source ->
            AzureDevOpsIntegration(
                organization = source.string("organization") ?: "",
                project = source.string("project") ?: "",
                team = source.string("team"),
                areaPath = source.string("areaPath"),
                iterationPath = source.string("iterationPath"),
                defaultWorkItemType = source.string("defaultWorkItemType") ?: "Issue",
            )
        },
    )

    private fun labelFrom(json: JsonObject) = LabelDefinition(
        name = json.string("name") ?: "",
        colorHex = json.string("colorHex"),
        description = json.string("description") ?: "",
    )

    private fun milestoneFrom(json: JsonObject) = Milestone(
        name = json.string("name") ?: "",
        dueOn = json.instant("dueOn"),
        isClosed = json.bool("isClosed") ?: false,
    )

    private fun personFrom(json: JsonObject) = Person(
        handle = json.string("handle") ?: "",
        displayName = json.string("displayName") ?: "",
        email = json.string("email") ?: "",
    )

    private fun issueFrom(json: JsonObject): Issue {
        val now = Instant.now()
        return Issue(
            uuid = json.uuid("uuid") ?: UUID.randomUUID(),
            number = json.int("number") ?: 0,
            title = json.string("title") ?: "",
            type = json.string("type")?.let(IssueType::fromRaw) ?: IssueType.DEFAULT,
            priority = json.string("priority")?.let(IssuePriority::fromRaw) ?: IssuePriority.DEFAULT,
            status = json.string("status")?.let(IssueStatus::fromRaw) ?: IssueStatus.DEFAULT,
            // Never guesses: an unrecognized close reason would misreport why work stopped.
            resolutionKind = json.string("resolutionKind")?.let(ResolutionKind::fromRaw),
            labels = json.stringList("labels") ?: emptyList(),
            assignees = json.stringList("assignees") ?: emptyList(),
            milestone = json.string("milestone"),
            area = json.string("area") ?: "",
            estimate = json.double("estimate"),
            reportedBy = json.string("reportedBy") ?: "",
            reported = json.instant("reported") ?: now,
            createdAt = json.instant("createdAt") ?: now,
            updatedAt = json.instant("updatedAt") ?: now,
            closedAt = json.instant("closedAt"),
            description = json.string("description") ?: "",
            stepsToReproduce = json.stringList("stepsToReproduce") ?: emptyList(),
            environment = json.string("environment") ?: "",
            notes = json.string("notes") ?: "",
            resolution = json.string("resolution") ?: "",
            comments = json.arr("comments")?.mapObjects("comments", ::commentFrom) ?: emptyList(),
            relations = json.arr("relations")?.mapObjects("relations", ::relationFrom) ?: emptyList(),
            remoteLinks = json.arr("remoteLinks")?.mapObjects("remoteLinks", ::remoteLinkFrom) ?: emptyList(),
        )
    }

    private fun commentFrom(json: JsonObject) = Comment(
        id = json.uuid("id") ?: UUID.randomUUID(),
        author = json.string("author") ?: "",
        createdAt = json.instant("createdAt") ?: Instant.now(),
        body = json.string("body") ?: "",
    )

    private fun relationFrom(json: JsonObject) = Relation(
        kind = json.string("kind")?.let(RelationKind::fromRaw) ?: RelationKind.DEFAULT,
        // The one field with no default — a relation pointing nowhere is not a relation.
        issueID = json.uuid("issueID")
            ?: throw IssuesError.DecodingFailed("relations[] entry is missing the required \"issueID\" key"),
    )

    private fun remoteLinkFrom(json: JsonObject) = RemoteLink(
        // Absence takes the default; an unrecognized value present in the file is preserved.
        provider = json.string("provider")?.let(RemoteProvider::fromRaw) ?: RemoteProvider.Github,
        identifier = json.string("identifier") ?: "",
        url = json.string("url"),
        lastSyncedAt = json.instant("lastSyncedAt"),
        remoteUpdatedAt = json.instant("remoteUpdatedAt"),
    )

    // MARK: - Encoding

    private fun IssuesDocumentModel.toJson() = obj {
        put("schemaVersion", schemaVersion)
        put("project", project.toJson())
        put("integrations", integrations.toJson())
        put("labels", labels.map { it.toJson() })
        put("milestones", milestones.map { it.toJson() })
        put("people", people.map { it.toJson() })
        put("export", export.toJson())
        put("issues", issues.map { it.toJson() })
    }

    private fun ProjectInfo.toJson() = obj {
        put("id", id)
        put("name", name)
        put("summary", summary)
    }

    private fun ExportSettings.toJson() = obj { put("preambleMarkdown", preambleMarkdown) }

    private fun IntegrationSettings.toJson() = obj {
        putIfPresent("github", github?.toJson())
        putIfPresent("azureDevOps", azureDevOps?.toJson())
    }

    private fun GitHubIntegration.toJson() = obj {
        put("owner", owner)
        put("repository", repository)
        put("defaultLabels", defaultLabels)
        put("defaultAssignees", defaultAssignees)
        putIfPresent("defaultMilestone", defaultMilestone)
    }

    private fun AzureDevOpsIntegration.toJson() = obj {
        put("organization", organization)
        put("project", project)
        putIfPresent("team", team)
        putIfPresent("areaPath", areaPath)
        putIfPresent("iterationPath", iterationPath)
        put("defaultWorkItemType", defaultWorkItemType)
    }

    private fun LabelDefinition.toJson() = obj {
        put("name", name)
        putIfPresent("colorHex", colorHex)
        put("description", description)
    }

    private fun Milestone.toJson() = obj {
        put("name", name)
        putIfPresent("dueOn", dueOn)
        put("isClosed", isClosed)
    }

    private fun Person.toJson() = obj {
        put("handle", handle)
        put("displayName", displayName)
        put("email", email)
    }

    private fun Issue.toJson() = obj {
        put("uuid", uuid)
        put("number", number)
        put("title", title)
        put("type", type.raw)
        put("priority", priority.raw)
        put("status", status.raw)
        putIfPresent("resolutionKind", resolutionKind?.raw)
        put("labels", labels)
        put("assignees", assignees)
        putIfPresent("milestone", milestone)
        put("area", area)
        putIfPresent("estimate", estimate)
        put("reportedBy", reportedBy)
        put("reported", reported)
        put("createdAt", createdAt)
        put("updatedAt", updatedAt)
        putIfPresent("closedAt", closedAt)
        put("description", description)
        put("stepsToReproduce", stepsToReproduce)
        put("environment", environment)
        put("notes", notes)
        put("resolution", resolution)
        put("comments", comments.map { it.toJson() })
        put("relations", relations.map { it.toJson() })
        put("remoteLinks", remoteLinks.map { it.toJson() })
    }

    private fun Comment.toJson() = obj {
        put("id", id)
        put("author", author)
        put("createdAt", createdAt)
        put("body", body)
    }

    private fun Relation.toJson() = obj {
        put("kind", kind.raw)
        put("issueID", issueID)
    }

    private fun RemoteLink.toJson() = obj {
        put("provider", provider.raw)
        put("identifier", identifier)
        putIfPresent("url", url)
        putIfPresent("lastSyncedAt", lastSyncedAt)
        putIfPresent("remoteUpdatedAt", remoteUpdatedAt)
    }

    // MARK: - Encoding helpers

    private class ObjectScope {
        val entries = LinkedHashMap<String, JsonValue>()

        fun put(key: String, value: String) { entries[key] = JsonValue.Str(value) }
        fun put(key: String, value: Int) { entries[key] = JsonValue.Num(value.toString()) }
        fun put(key: String, value: Boolean) { entries[key] = JsonValue.Bool(value) }
        fun put(key: String, value: UUID) { entries[key] = JsonValue.Str(value.toString().uppercase(Locale.ROOT)) }
        fun put(key: String, value: Instant) { entries[key] = JsonValue.Str(IssueTimestamp.format(value)) }
        fun put(key: String, value: JsonValue) { entries[key] = value }

        @JvmName("putStrings")
        fun put(key: String, values: List<String>) {
            entries[key] = JsonValue.Arr(values.map { JsonValue.Str(it) })
        }

        @JvmName("putObjects")
        fun put(key: String, values: List<JsonValue>) { entries[key] = JsonValue.Arr(values) }

        /** Mirrors Swift's `encodeIfPresent`: a null optional writes no key at all. */
        fun putIfPresent(key: String, value: String?) { if (value != null) put(key, value) }

        @JvmName("putInstantIfPresent")
        fun putIfPresent(key: String, value: Instant?) { if (value != null) put(key, value) }

        @JvmName("putDoubleIfPresent")
        fun putIfPresent(key: String, value: Double?) {
            if (value != null) entries[key] = JsonValue.Num(FoundationJson.numberLiteral(value))
        }

        @JvmName("putValueIfPresent")
        fun putIfPresent(key: String, value: JsonValue?) { if (value != null) put(key, value) }
    }

    private inline fun obj(build: ObjectScope.() -> Unit): JsonValue.Obj =
        JsonValue.Obj(ObjectScope().apply(build).entries)

    // MARK: - Decoding helpers

    private fun JsonElement.expectObject(path: String): JsonObject =
        this as? JsonObject ?: throw IssuesError.DecodingFailed("expected an object at \"$path\"")

    private fun JsonObject.value(key: String): JsonElement? =
        this[key]?.takeUnless { it is JsonNull }

    private fun JsonObject.primitive(key: String): JsonPrimitive? =
        value(key)?.let {
            it as? JsonPrimitive ?: throw IssuesError.DecodingFailed("\"$key\" is not a scalar value")
        }

    private fun JsonObject.string(key: String): String? = primitive(key)?.let {
        if (!it.isString) throw IssuesError.DecodingFailed("\"$key\" is not a string")
        it.content
    }

    private fun JsonObject.int(key: String): Int? = primitive(key)?.let {
        it.content.toIntOrNull() ?: throw IssuesError.DecodingFailed("\"$key\" is not an integer")
    }

    private fun JsonObject.double(key: String): Double? = primitive(key)?.let {
        it.content.toDoubleOrNull() ?: throw IssuesError.DecodingFailed("\"$key\" is not a number")
    }

    private fun JsonObject.bool(key: String): Boolean? = primitive(key)?.let {
        when (it.content) {
            "true" -> true
            "false" -> false
            else -> throw IssuesError.DecodingFailed("\"$key\" is not a boolean")
        }
    }

    private fun JsonObject.uuid(key: String): UUID? = string(key)?.let {
        try {
            UUID.fromString(it)
        } catch (_: IllegalArgumentException) {
            throw IssuesError.DecodingFailed("\"$key\" is not a UUID: $it")
        }
    }

    private fun JsonObject.instant(key: String): Instant? = string(key)?.let {
        IssueTimestamp.parse(it) ?: throw IssuesError.DecodingFailed("\"$key\" is not an ISO-8601 date: $it")
    }

    private fun JsonObject.obj(key: String): JsonObject? = value(key)?.expectObject(key)

    private fun JsonObject.arr(key: String): JsonArray? = value(key)?.let {
        it as? JsonArray ?: throw IssuesError.DecodingFailed("\"$key\" is not an array")
    }

    private fun JsonObject.stringList(key: String): List<String>? = arr(key)?.map { element ->
        val primitive = element as? JsonPrimitive
        if (primitive == null || !primitive.isString) {
            throw IssuesError.DecodingFailed("\"$key\" contains a non-string element")
        }
        primitive.content
    }

    private fun <T> JsonArray.mapObjects(path: String, transform: (JsonObject) -> T): List<T> =
        map { transform(it.expectObject(path)) }
}
