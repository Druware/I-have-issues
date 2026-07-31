package com.druware.ihaveissues.ui

import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle

/**
 * Renders the inline markdown an issue body can contain.
 *
 * The Apple app parses these fields with `AttributedString(markdown:)` using
 * `.inlineOnlyPreservingWhitespace`, so `**bold**` renders but a `# heading` stays literal and line
 * breaks are kept. This does the same three spans — bold, italic and code — because there is no
 * markdown renderer on the Android platform and pulling one in is out of scope. Anything it does not
 * recognize is left exactly as typed, which is the same failure mode Apple has.
 */
internal fun inlineMarkdown(text: String): AnnotatedString = buildAnnotatedString {
    var index = 0
    val literal = StringBuilder()

    fun flush() {
        if (literal.isNotEmpty()) {
            append(literal.toString())
            literal.setLength(0)
        }
    }

    while (index < text.length) {
        val span = matchSpan(text, index)
        if (span == null) {
            literal.append(text[index])
            index++
            continue
        }
        flush()
        withStyle(span.style) { append(span.content) }
        index = span.end
    }
    flush()
}

private class InlineSpan(val style: SpanStyle, val content: String, val end: Int)

private val BOLD = SpanStyle(fontWeight = FontWeight.Bold)
private val ITALIC = SpanStyle(fontStyle = FontStyle.Italic)
private val CODE = SpanStyle(fontFamily = FontFamily.Monospace)

/** Matches a delimited run starting at [start], longest delimiter first so `**` beats `*`. */
private fun matchSpan(text: String, start: Int): InlineSpan? =
    match(text, start, "**", BOLD)
        ?: match(text, start, "__", BOLD)
        ?: match(text, start, "*", ITALIC)
        ?: match(text, start, "_", ITALIC)
        ?: match(text, start, "`", CODE)

private fun match(text: String, start: Int, delimiter: String, style: SpanStyle): InlineSpan? {
    if (!text.startsWith(delimiter, start)) return null
    val contentStart = start + delimiter.length
    val contentEnd = text.indexOf(delimiter, contentStart)
    // An unclosed delimiter, or an empty one, is literal text — not a span.
    if (contentEnd <= contentStart) return null
    return InlineSpan(style, text.substring(contentStart, contentEnd), contentEnd + delimiter.length)
}
