package com.druware.issueskit

import java.io.File
import java.time.Instant
import java.util.UUID

/**
 * Locates the repository root by walking up from the test working directory until the Apple
 * sample document is found, so the shared `.issues` fixtures can be read without hardcoding an
 * absolute path or assuming which directory Gradle runs tests from.
 */
object Repo {
    private const val MARKER = "apps/Apple/sample/Example.issues"

    val root: File by lazy {
        generateSequence(File(System.getProperty("user.dir")).absoluteFile) { it.parentFile }
            .firstOrNull { File(it, MARKER).isFile }
            ?: error("Could not locate the repository root (no $MARKER above ${System.getProperty("user.dir")})")
    }

    fun file(relativePath: String): File = File(root, relativePath)
}

/** Reads a fixture bundled into the test resources. */
fun testResource(name: String): String =
    checkNotNull(Repo::class.java.classLoader.getResourceAsStream(name)) { "missing test resource $name" }
        .use { it.readBytes().toString(Charsets.UTF_8) }

/** Reads a fixture bundled into the test resources as raw bytes. */
fun testResourceBytes(name: String): ByteArray =
    checkNotNull(Repo::class.java.classLoader.getResourceAsStream(name)) { "missing test resource $name" }
        .use { it.readBytes() }

/** A whole-second instant, so it survives the ISO-8601 encoding used by [IssuesJSONCoder]. */
val referenceDate: Instant = Instant.ofEpochSecond(1_767_225_600) // 2026-01-01T00:00:00Z

fun day(text: String): Instant = checkNotNull(IssueDate.dateFrom(text)) { "unparsable day $text" }

val issueA: UUID = UUID.fromString("0F5B9C7E-0000-4000-8000-000000000001")
val issueB: UUID = UUID.fromString("0F5B9C7E-0000-4000-8000-000000000002")

/** A model with every field populated, used to prove the JSON round trip is lossless. */
fun makeFullModel(): IssuesDocumentModel {
    val issue = Issue(
        uuid = issueA,
        number = 7,
        title = "Login button does nothing",
        type = IssueType.BUG,
        priority = IssuePriority.HIGH,
        status = IssueStatus.RESOLVED,
        resolutionKind = ResolutionKind.FIXED,
        labels = listOf("ui", "regression"),
        assignees = listOf("dru", "sam"),
        milestone = "v1.0",
        area = "Views",
        estimate = 3.5,
        reportedBy = "Dru",
        reported = day("2026-05-01"),
        createdAt = referenceDate,
        updatedAt = referenceDate.plusSeconds(3600),
        closedAt = referenceDate.plusSeconds(7200),
        description = "The login button is inert.\nIt should authenticate.",
        stepsToReproduce = listOf("Open the app", "Tap Login"),
        environment = "macOS 27.0, build 1234",
        notes = "Possibly a missing action binding.",
        resolution = "Rebound the action.",
        comments = listOf(
            Comment(
                id = UUID.fromString("0F5B9C7E-0000-4000-8000-00000000000A"),
                author = "Sam",
                createdAt = day("2026-05-02"),
                body = "Reproduced on my machine.\nSame stack trace.",
            ),
        ),
        relations = listOf(Relation(RelationKind.BLOCKED_BY, issueB)),
        remoteLinks = listOf(
            RemoteLink(
                provider = RemoteProvider.Github,
                identifier = "412",
                url = "https://github.com/openbcm/i-have-issues/issues/412",
                lastSyncedAt = referenceDate,
                remoteUpdatedAt = referenceDate.plusSeconds(60),
            ),
        ),
    )

    val other = Issue(
        uuid = issueB,
        number = 8,
        title = "Blocked by nothing",
        reported = referenceDate,
        createdAt = referenceDate,
        updatedAt = referenceDate,
    )

    return IssuesDocumentModel(
        project = ProjectInfo(
            id = UUID.fromString("0F5B9C7E-0000-4000-8000-0000000000FF"),
            name = "I Have Issues",
            summary = "A document-based issue tracker.",
        ),
        integrations = IntegrationSettings(
            github = GitHubIntegration(
                owner = "openbcm",
                repository = "i-have-issues",
                defaultLabels = listOf("triage"),
                defaultAssignees = listOf("dru"),
                defaultMilestone = "v1.0",
            ),
            azureDevOps = AzureDevOpsIntegration(
                organization = "openbcm",
                project = "IHaveIssues",
                team = "Core",
                areaPath = "IHaveIssues\\Client",
                iterationPath = "IHaveIssues\\Sprint 1",
                defaultWorkItemType = "Bug",
            ),
        ),
        labels = listOf(LabelDefinition("ui", "#FF00AA", "User interface")),
        milestones = listOf(Milestone("v1.0", day("2026-06-30"), isClosed = false)),
        people = listOf(Person("dru", "Dru", "dru@openbcm.com")),
        export = ExportSettings("# Issues\n\n"),
        issues = listOf(issue, other),
    )
}
