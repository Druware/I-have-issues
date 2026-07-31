package com.druware.issueskit

import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

class IssuesJSONCoderTest {

    // MARK: - 1. JSON round trip

    @Test
    fun `full model round trips through JSON`() {
        val model = makeFullModel()
        assertEquals(model, IssuesJSONCoder.decode(IssuesJSONCoder.encode(model)))
    }

    @Test
    fun `encoded JSON ends with a newline`() {
        assertEquals('\n'.code.toByte(), IssuesJSONCoder.encode(makeFullModel()).last())
    }

    // MARK: - 2. Minimal document

    @Test
    fun `minimal document decodes with defaults`() {
        val json = """
            {
              "schemaVersion": 1,
              "project": { "id": "0F5B9C7E-0000-4000-8000-0000000000FF" },
              "issues": []
            }
        """.trimIndent()
        val model = IssuesJSONCoder.decode(json)

        assertEquals(1, model.schemaVersion)
        assertTrue(model.project.name.isEmpty())
        assertTrue(model.project.summary.isEmpty())
        assertEquals(IntegrationSettings(), model.integrations)
        assertTrue(model.labels.isEmpty())
        assertTrue(model.milestones.isEmpty())
        assertTrue(model.people.isEmpty())
        assertEquals(ExportSettings.DEFAULT_PREAMBLE_MARKDOWN, model.export.preambleMarkdown)
        assertTrue(model.issues.isEmpty())
    }

    @Test
    fun `minimal issue decodes with defaults`() {
        val model = IssuesJSONCoder.decode("""{ "schemaVersion": 1, "issues": [ { "title": "Bare" } ] }""")
        val issue = model.issues.single()

        assertEquals("Bare", issue.title)
        assertEquals(0, issue.number)
        assertEquals(IssueType.TASK, issue.type)
        assertEquals(IssuePriority.MEDIUM, issue.priority)
        assertEquals(IssueStatus.OPEN, issue.status)
        assertNull(issue.resolutionKind)
        assertTrue(issue.labels.isEmpty())
        assertTrue(issue.assignees.isEmpty())
        assertNull(issue.milestone)
        assertNull(issue.estimate)
        assertNull(issue.closedAt)
        assertTrue(issue.stepsToReproduce.isEmpty())
        assertTrue(issue.environment.isEmpty())
        assertTrue(issue.comments.isEmpty())
        assertTrue(issue.relations.isEmpty())
        assertTrue(issue.remoteLinks.isEmpty())
    }

    @Test
    fun `document without schemaVersion throws`() {
        assertEquals(
            IssuesError.MissingSchemaVersion,
            assertFailsWith<IssuesError> { IssuesJSONCoder.decode("""{ "issues": [] }""") },
        )
    }

    // MARK: - 3. Unknown keys

    @Test
    fun `unknown JSON keys are ignored`() {
        val json = """
            {
              "schemaVersion": 1,
              "futureTopLevelField": { "anything": [1, 2, 3] },
              "project": { "name": "Demo", "futureProjectField": true },
              "issues": [ { "title": "Kept", "futureIssueField": "ignored" } ]
            }
        """.trimIndent()
        val model = IssuesJSONCoder.decode(json)

        assertEquals("Demo", model.project.name)
        assertEquals("Kept", model.issues.single().title)
    }

    // MARK: - 4. Unknown enum raw values

    @Test
    fun `unknown enum raw values fall back to defaults`() {
        val json = """
            {
              "schemaVersion": 1,
              "issues": [
                {
                  "title": "From the future",
                  "type": "epic",
                  "priority": "whenever",
                  "status": "vibing",
                  "resolutionKind": "ascended",
                  "relations": [ { "kind": "supersedes", "issueID": "0F5B9C7E-0000-4000-8000-000000000002" } ],
                  "remoteLinks": [ { "provider": "jira", "identifier": "X-1" } ]
                }
              ]
            }
        """.trimIndent()
        val issue = IssuesJSONCoder.decode(json).issues.single()

        assertEquals(IssueType.TASK, issue.type)
        assertEquals(IssuePriority.MEDIUM, issue.priority)
        assertEquals(IssueStatus.OPEN, issue.status)
        assertNull(issue.resolutionKind)
        assertEquals(RelationKind.RELATED_TO, issue.relations.single().kind)
        // A provider is sync identity, so it is preserved verbatim rather than defaulted.
        assertEquals(RemoteProvider.Other("jira"), issue.remoteLinks.single().provider)
    }

    @Test
    fun `unknown remote provider is preserved not coerced to github`() {
        val json = """
            {
              "schemaVersion": 1,
              "issues": [
                { "title": "Synced elsewhere", "remoteLinks": [ { "provider": "gitlab", "identifier": "77" } ] }
              ]
            }
        """.trimIndent()
        val link = IssuesJSONCoder.decode(json).issues.single().remoteLinks.single()

        assertEquals(RemoteProvider.Other("gitlab"), link.provider)
        assertNotEquals(RemoteProvider.Github, link.provider)
        assertNotEquals(RemoteProvider.AzureDevOps, link.provider)
        assertFalse(link.provider.isGitHub)
        assertFalse(link.provider.isAzureDevOps)
        assertEquals("gitlab", link.provider.displayName)
        assertEquals("gitlab", link.provider.raw)
    }

    @Test
    fun `unknown remote provider re-encodes verbatim`() {
        val link = RemoteLink(RemoteProvider.Other("gitlab"), identifier = "77")
        val model = IssuesDocumentModel(issues = listOf(Issue(number = 1, remoteLinks = listOf(link))))

        val json = IssuesJSONCoder.encodeToString(model)
        assertContains(json, "\"provider\" : \"gitlab\"")
        assertFalse(json.contains("\"provider\" : \"github\""))

        val reencoded = IssuesJSONCoder.encodeToString(IssuesJSONCoder.decode(json))
        assertEquals(json, reencoded)
    }

    @Test
    fun `known remote providers round trip and are selectable`() {
        assertEquals(RemoteProvider.Github, RemoteProvider.fromRaw("github"))
        assertEquals(RemoteProvider.AzureDevOps, RemoteProvider.fromRaw("azureDevOps"))
        assertTrue(RemoteProvider.Github.isGitHub)
        assertTrue(RemoteProvider.AzureDevOps.isAzureDevOps)
        assertEquals(listOf(RemoteProvider.Github, RemoteProvider.AzureDevOps), RemoteProvider.selectableCases)
        assertEquals(listOf("GitHub", "Azure DevOps"), RemoteProvider.selectableCases.map { it.displayName })
    }

    @Test
    fun `absent remote provider falls back to github`() {
        val json = """{ "schemaVersion": 1, "issues": [ { "remoteLinks": [ { "identifier": "9" } ] } ] }"""
        val link = IssuesJSONCoder.decode(json).issues.single().remoteLinks.single()
        assertEquals(RemoteProvider.Github, link.provider)
    }

    // MARK: - 5. Required relation target

    @Test
    fun `relation without issueID throws`() {
        val json = """{ "schemaVersion": 1, "issues": [ { "relations": [ { "kind": "blocks" } ] } ] }"""
        val error = assertFailsWith<IssuesError.DecodingFailed> { IssuesJSONCoder.decode(json) }
        assertContains(error.detail, "issueID")
    }

    @Test
    fun `relation with issueID decodes`() {
        val json = """
            { "schemaVersion": 1, "issues": [ { "relations":
              [ { "issueID": "0F5B9C7E-0000-4000-8000-000000000002" } ] } ] }
        """.trimIndent()
        val relation = IssuesJSONCoder.decode(json).issues.single().relations.single()
        assertEquals(issueB, relation.issueID)
        assertEquals(RelationKind.RELATED_TO, relation.kind)
    }

    // MARK: - 6. Schema version gate

    @Test
    fun `newer schema version throws`() {
        assertEquals(
            IssuesError.UnsupportedSchemaVersion(99, 1),
            assertFailsWith<IssuesError> { IssuesJSONCoder.decode("""{ "schemaVersion": 99, "issues": [] }""") },
        )
    }

    @Test
    fun `supported schema version decodes`() {
        assertEquals(1, IssuesJSONCoder.decode("""{ "schemaVersion": 1, "issues": [] }""").schemaVersion)
    }

    @Test
    fun `older schema version is accepted`() {
        assertEquals(0, IssuesJSONCoder.decode("""{ "schemaVersion": 0, "issues": [] }""").schemaVersion)
    }

    // MARK: - 7. Deterministic, Apple-shaped encoding

    @Test
    fun `encoding is deterministic`() {
        val model = makeFullModel()
        assertContentEqualsBytes(IssuesJSONCoder.encode(model), IssuesJSONCoder.encode(model))
    }

    @Test
    fun `encoded keys are sorted`() {
        val json = IssuesJSONCoder.encodeToString(makeFullModel())
        // Pretty-printed output indents top-level keys by exactly two spaces.
        val topLevelKeys = json.split("\n")
            .filter { it.startsWith("  \"") }
            .map { it.drop(3).substringBefore('"') }

        assertEquals(
            listOf("export", "integrations", "issues", "labels", "milestones", "people", "project", "schemaVersion"),
            topLevelKeys,
        )
        assertEquals(topLevelKeys.sorted(), topLevelKeys)
    }

    @Test
    fun `encoded JSON does not escape slashes`() {
        val json = IssuesJSONCoder.encodeToString(makeFullModel())
        assertContains(json, "https://github.com/openbcm/i-have-issues/issues/412")
        assertFalse(json.contains("\\/"))
    }

    @Test
    fun `encoder separates keys and values with a space on both sides`() {
        val json = IssuesJSONCoder.encodeToString(makeFullModel())
        assertContains(json, "\"schemaVersion\" : 1")
        assertFalse(json.contains("\"schemaVersion\": 1"))
    }

    @Test
    fun `empty containers render as an opener, a blank line, and the closer`() {
        val model = IssuesDocumentModel(issues = listOf(Issue(number = 1, reported = referenceDate)))
        val json = IssuesJSONCoder.encodeToString(model)

        assertContains(json, "  \"integrations\" : {\n\n  },\n")
        assertContains(json, "      \"labels\" : [\n\n      ],\n")
        assertFalse(json.contains("[]"))
        assertFalse(json.contains("{}"))
    }

    @Test
    fun `whole estimates encode without a decimal point`() {
        val json = IssuesJSONCoder.encodeToString(
            IssuesDocumentModel(issues = listOf(Issue(number = 1, estimate = 5.0))),
        )
        assertContains(json, "\"estimate\" : 5,")

        val fractional = IssuesJSONCoder.encodeToString(
            IssuesDocumentModel(issues = listOf(Issue(number = 1, estimate = 3.5))),
        )
        assertContains(fractional, "\"estimate\" : 3.5,")
    }

    @Test
    fun `timestamps encode as whole-second UTC and truncate sub-second precision`() {
        val model = IssuesDocumentModel(
            issues = listOf(
                Issue(
                    number = 1,
                    reported = day("2026-05-01"),
                    createdAt = referenceDate.plusNanos(750_000_000),
                    updatedAt = referenceDate,
                ),
            ),
        )
        val json = IssuesJSONCoder.encodeToString(model)
        assertContains(json, "\"reported\" : \"2026-05-01T00:00:00Z\"")
        assertContains(json, "\"createdAt\" : \"2026-01-01T00:00:00Z\"")
    }

    @Test
    fun `malformed JSON reports a decoding failure`() {
        assertFailsWith<IssuesError.DecodingFailed> { IssuesJSONCoder.decode("{ not json") }
    }

    @Test
    fun `a value of the wrong type reports a decoding failure`() {
        assertFailsWith<IssuesError.DecodingFailed> {
            IssuesJSONCoder.decode("""{ "schemaVersion": 1, "issues": [ { "title": 5 } ] }""")
        }
    }

    private fun assertContentEqualsBytes(expected: ByteArray, actual: ByteArray) =
        assertEquals(expected.toString(Charsets.UTF_8), actual.toString(Charsets.UTF_8))
}
