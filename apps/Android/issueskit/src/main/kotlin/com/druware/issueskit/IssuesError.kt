package com.druware.issueskit

/** Errors thrown while reading or writing an issues document. */
sealed class IssuesError(message: String) : Exception(message) {

    /** The JSON has no `schemaVersion`, so it is not an `.issues` document. */
    data object MissingSchemaVersion : IssuesError(
        "The file does not declare a \"schemaVersion\" and is not a valid issues document."
    ) {
        private fun readResolve(): Any = MissingSchemaVersion
    }

    /** The document was written by a newer build than this one can read. */
    data class UnsupportedSchemaVersion(val found: Int, val supported: Int) : IssuesError(
        "This document uses issues format version $found, but this version of the app " +
            "only reads up to version $supported. Update the app to open it."
    )

    /** The JSON could not be decoded; the payload is the underlying decoding failure. */
    data class DecodingFailed(val detail: String) : IssuesError(
        "The issues document could not be read: $detail"
    )

    /** The model could not be encoded; the payload is the underlying encoding failure. */
    data class EncodingFailed(val detail: String) : IssuesError(
        "The issues document could not be written: $detail"
    )

    /** A legacy markdown file has no level-2 headings, so it is not a recognizable issues file. */
    data object MissingOpenSection : IssuesError(
        "The markdown file does not contain an \"## Open\" section and cannot be imported."
    ) {
        private fun readResolve(): Any = MissingOpenSection
    }
}
