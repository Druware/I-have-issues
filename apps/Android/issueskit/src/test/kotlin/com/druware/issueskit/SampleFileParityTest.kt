package com.druware.issueskit

import kotlinx.serialization.json.Json
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

/**
 * The definitive byte-parity tests.
 *
 * `.issues` documents are shared between the Apple and Android apps and committed to git, so a
 * formatting difference in Android's encoder would turn every Android save into a whole-file diff.
 * These read the repository's real documents and assert byte-identical output.
 */
class SampleFileParityTest {

    private val json = Json

    @Test
    fun `Example issues round trips byte for byte`() {
        val original = Repo.file("apps/Apple/sample/Example.issues").readBytes()
        val reencoded = IssuesJSONCoder.encode(IssuesJSONCoder.decode(original))

        assertEquals(
            original.toString(Charsets.UTF_8),
            reencoded.toString(Charsets.UTF_8),
            "Android's encoder must reproduce the Apple app's bytes exactly",
        )
    }

    /**
     * `IHaveIssues-Issues.issues` cannot round trip byte-identically — and does not do so through
     * the Apple app either. One issue object in it was hand-edited: `closedAt` and
     * `resolutionKind` were appended after `type` instead of in sorted position, which no encoder
     * with `.sortedKeys` can reproduce. Re-encoding therefore *normalizes* it.
     *
     * The assertion is not weakened: Android's output is compared against the bytes Apple's own
     * `IssuesJSONCoder` produces for this file, captured verbatim as a fixture.
     */
    @Test
    fun `IHaveIssues issues re-encodes to the same bytes the Apple app produces`() {
        val original = Repo.file("apps/Apple/IHaveIssues-Issues.issues").readBytes()
        val appleCanonical = testResourceBytes("IHaveIssues-Issues.canonical.issues")
        val reencoded = IssuesJSONCoder.encode(IssuesJSONCoder.decode(original))

        assertEquals(appleCanonical.toString(Charsets.UTF_8), reencoded.toString(Charsets.UTF_8))
    }

    @Test
    fun `the only difference in IHaveIssues issues is key order`() {
        val original = Repo.file("apps/Apple/IHaveIssues-Issues.issues").readText()
        val reencoded = IssuesJSONCoder.encodeToString(IssuesJSONCoder.decode(original))

        assertNotEquals(original, reencoded, "the fixture is known to carry hand-edited key order")
        assertEquals(
            json.parseToJsonElement(original),
            json.parseToJsonElement(reencoded),
            "normalization must not change any value, only the order keys are written in",
        )
    }

    @Test
    fun `re-encoding is idempotent for both sample documents`() {
        for (path in listOf("apps/Apple/sample/Example.issues", "apps/Apple/IHaveIssues-Issues.issues")) {
            val once = IssuesJSONCoder.encodeToString(IssuesJSONCoder.decode(Repo.file(path).readText()))
            val twice = IssuesJSONCoder.encodeToString(IssuesJSONCoder.decode(once))
            assertEquals(once, twice, "second save of $path must not diff against the first")
        }
    }

    @Test
    fun `the default preamble matches the one the Apple documents store`() {
        val model = IssuesJSONCoder.decode(Repo.file("apps/Apple/sample/Example.issues").readText())
        assertEquals(ExportSettings.DEFAULT_PREAMBLE_MARKDOWN, model.export.preambleMarkdown)
    }

    @Test
    fun `the sample document decodes into the expected shape`() {
        val model = IssuesJSONCoder.decode(Repo.file("apps/Apple/sample/Example.issues").readText())

        assertEquals(1, model.schemaVersion)
        assertEquals("I Have Issues", model.project.name)
        assertEquals(3, model.issues.size)
        assertEquals(2, model.openIssues.size)
        assertEquals(1, model.resolvedIssues.size)
        assertEquals(4, model.nextNumber)

        val first = model.issues.first()
        assertEquals(IssueType.BUG, first.type)
        assertEquals(IssueStatus.RESOLVED, first.status)
        assertEquals(ResolutionKind.FIXED, first.resolutionKind)
        assertEquals(3.5, first.estimate)
        assertEquals(RemoteProvider.Github, first.remoteLinks.single().provider)
        assertEquals(RelationKind.BLOCKS, first.relations.single().kind)
        assertTrue(model.integrations.github != null)
        assertEquals("openbcm", model.integrations.github?.owner)
    }
}
