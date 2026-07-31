package com.druware.issueskit

import java.time.Instant
import java.time.LocalDate
import java.time.OffsetDateTime
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import java.time.temporal.ChronoUnit

/**
 * Fixed date handling for the `YYYY-MM-DD` reported dates in the document.
 *
 * A single fixed-offset (UTC) calendar guarantees that parsing then formatting a date is a stable
 * identity regardless of the device's time zone, which is what keeps issue round trips lossless
 * and stops a "reported" day from shifting for users east or west of GMT.
 */
object IssueDate {

    private val dayFormatter: DateTimeFormatter =
        DateTimeFormatter.ofPattern("uuuu-MM-dd").withZone(ZoneOffset.UTC)

    /** Parses a `YYYY-MM-DD` string into a day-normalized instant, or `null` if malformed. */
    fun dateFrom(text: String): Instant? = try {
        LocalDate.parse(text.trimSpaces(), dayFormatter).atStartOfDay(ZoneOffset.UTC).toInstant()
    } catch (_: java.time.format.DateTimeParseException) {
        null
    }

    /** Formats an instant as `YYYY-MM-DD` in UTC. */
    fun stringFrom(date: Instant): String = dayFormatter.format(date)

    /** The current date normalized to the start of its day in UTC. */
    fun today(): Instant = Instant.now().truncatedTo(ChronoUnit.DAYS)
}

/**
 * ISO-8601 timestamps as written by the `.issues` format: UTC, whole seconds, `Z` suffix.
 *
 * Apple's `JSONEncoder.dateEncodingStrategy = .iso8601` truncates sub-second precision, so a
 * timestamp never survives a round trip finer than one second. This mirrors that exactly.
 */
internal object IssueTimestamp {

    private val formatter: DateTimeFormatter =
        DateTimeFormatter.ofPattern("uuuu-MM-dd'T'HH:mm:ss'Z'").withZone(ZoneOffset.UTC)

    fun format(instant: Instant): String = formatter.format(instant.truncatedTo(ChronoUnit.SECONDS))

    /** Accepts `Z` and numeric-offset forms, with or without fractional seconds. */
    fun parse(text: String): Instant? = try {
        Instant.parse(text)
    } catch (_: java.time.format.DateTimeParseException) {
        try {
            OffsetDateTime.parse(text).toInstant()
        } catch (_: java.time.format.DateTimeParseException) {
            null
        }
    }
}

/**
 * Trims the characters Swift's `CharacterSet.whitespaces` covers — horizontal tab plus Unicode
 * space separators — and deliberately not newlines, so the markdown parsers behave identically.
 */
internal fun String.trimSpaces(): String = trim { it.isIssueSpace() }

internal fun Char.isIssueSpace(): Boolean =
    this == '\t' || Character.getType(this) == Character.SPACE_SEPARATOR.toInt()
