package com.druware.issueskit.internal

import java.util.Locale
import kotlin.math.abs
import kotlin.math.floor

/**
 * A minimal JSON tree used only as the intermediate representation for writing.
 *
 * Numbers carry a pre-rendered literal so the writer never has to re-derive formatting.
 */
internal sealed interface JsonValue {
    data class Obj(val entries: Map<String, JsonValue>) : JsonValue
    data class Arr(val items: List<JsonValue>) : JsonValue
    data class Str(val value: String) : JsonValue
    data class Num(val literal: String) : JsonValue
    data class Bool(val value: Boolean) : JsonValue
}

/**
 * Writes JSON byte-for-byte the way Apple's `JSONEncoder` does with
 * `[.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]`.
 *
 * `.issues` files are committed to project repositories, so Android must emit exactly what the
 * Apple app emits or every save would produce a whole-file diff. `kotlinx.serialization` cannot
 * produce this shape — it writes `": "` rather than `" : "`, indents four spaces, does not sort
 * keys, and renders empty containers as `[]`/`{}` — so the writer is hand-rolled.
 *
 * The exact quirks, verified against Foundation on macOS:
 * - two-space indent per nesting level;
 * - `" : "` between key and value, with a space on **both** sides;
 * - keys sorted by Unicode code unit at every nesting level;
 * - `/` never escaped;
 * - an empty array or object renders as its opener, a blank line, then the closer at the
 *   opener's own indent — `[\n\n  ]`, not `[]`;
 * - control characters below U+0020 escape as `\b`, `\f`, `\n`, `\r`, `\t`, or lowercase
 *   `\u00xx`; every other character, including non-ASCII, is emitted raw as UTF-8;
 * - a whole-valued double renders without a decimal point (`5`, not `5.0`).
 */
internal object FoundationJson {

    private const val BACKSPACE = '\u0008'
    private const val FORM_FEED = '\u000C'
    private const val INT64_LIMIT = 9.223372036854776E18

    fun render(root: JsonValue): String = StringBuilder().apply { write(root, 0, this) }.toString()

    private fun write(value: JsonValue, level: Int, out: StringBuilder) {
        when (value) {
            is JsonValue.Obj -> writeContainer('{', '}', value.entries.isEmpty(), level, out) {
                value.entries.entries
                    .sortedBy { it.key }
                    .forEachIndexed { index, (key, child) ->
                        if (index > 0) out.append(',')
                        out.append('\n').append(indent(level + 1))
                        writeString(key, out)
                        out.append(" : ")
                        write(child, level + 1, out)
                    }
            }

            is JsonValue.Arr -> writeContainer('[', ']', value.items.isEmpty(), level, out) {
                value.items.forEachIndexed { index, child ->
                    if (index > 0) out.append(',')
                    out.append('\n').append(indent(level + 1))
                    write(child, level + 1, out)
                }
            }

            is JsonValue.Str -> writeString(value.value, out)
            is JsonValue.Num -> out.append(value.literal)
            is JsonValue.Bool -> out.append(if (value.value) "true" else "false")
        }
    }

    private inline fun writeContainer(
        open: Char,
        close: Char,
        isEmpty: Boolean,
        level: Int,
        out: StringBuilder,
        body: () -> Unit,
    ) {
        out.append(open)
        if (isEmpty) {
            out.append("\n\n").append(indent(level)).append(close)
            return
        }
        body()
        out.append('\n').append(indent(level)).append(close)
    }

    private fun indent(level: Int): String = "  ".repeat(level)

    private fun writeString(value: String, out: StringBuilder) {
        out.append('"')
        for (ch in value) {
            when {
                ch == '"' -> out.append("\\\"")
                ch == '\\' -> out.append("\\\\")
                ch == '\n' -> out.append("\\n")
                ch == '\r' -> out.append("\\r")
                ch == '\t' -> out.append("\\t")
                ch == BACKSPACE -> out.append("\\b")
                ch == FORM_FEED -> out.append("\\f")
                ch < ' ' -> out.append(String.format(Locale.ROOT, "\\u%04x", ch.code))
                else -> out.append(ch)
            }
        }
        out.append('"')
    }

    /**
     * Renders a double the way Foundation does: whole values within `Int64` range collapse to an
     * integer literal, everything else uses the shortest round-tripping decimal form.
     */
    fun numberLiteral(value: Double): String =
        if (value.isFinite() && value == floor(value) && abs(value) < INT64_LIMIT) {
            value.toLong().toString()
        } else {
            value.toString()
        }
}
