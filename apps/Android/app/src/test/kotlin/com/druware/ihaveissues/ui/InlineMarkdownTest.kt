package com.druware.ihaveissues.ui

import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

/** Inline markdown spans, matching what Apple's `.inlineOnlyPreservingWhitespace` parsing renders. */
class InlineMarkdownTest {

    @Test
    fun `plain text carries no spans`() {
        val rendered = inlineMarkdown("Tapping Login has no effect.")

        assertEquals("Tapping Login has no effect.", rendered.text)
        assertTrue(rendered.spanStyles.isEmpty())
    }

    @Test
    fun `bold delimiters are removed and the run is styled`() {
        val rendered = inlineMarkdown("Tapping **Login** has no effect.")

        assertEquals("Tapping Login has no effect.", rendered.text)
        val span = rendered.spanStyles.single()
        assertEquals(FontWeight.Bold, span.item.fontWeight)
        assertEquals("Login", rendered.text.substring(span.start, span.end))
    }

    @Test
    fun `italic and code spans are recognized`() {
        val rendered = inlineMarkdown("see *later* or `code`")

        assertEquals("see later or code", rendered.text)
        assertEquals(2, rendered.spanStyles.size)
        assertEquals(FontStyle.Italic, rendered.spanStyles[0].item.fontStyle)
        assertEquals("code", rendered.text.substring(rendered.spanStyles[1].start, rendered.spanStyles[1].end))
    }

    @Test
    fun `an unclosed delimiter stays literal`() {
        val rendered = inlineMarkdown("2 * 3 is not emphasis")

        assertEquals("2 * 3 is not emphasis", rendered.text)
        assertTrue(rendered.spanStyles.isEmpty())
    }

    @Test
    fun `line breaks are preserved`() {
        assertEquals("one\ntwo", inlineMarkdown("one\ntwo").text)
    }

    @Test
    fun `a heading marker is left alone because parsing is inline only`() {
        assertEquals("# Not a heading", inlineMarkdown("# Not a heading").text)
    }
}
