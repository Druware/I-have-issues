package com.druware.ihaveissues.document

/**
 * MIME types used by the Storage Access Framework pickers.
 *
 * `.issues` has no registered MIME type, so what a provider reports for one is decided by whatever
 * mapping that provider happens to have. `MimeTypeMap` has no entry for the extension, so
 * `DocumentsContract`-backed providers (external storage, Downloads) fall back to
 * `application/octet-stream`; providers that sniff content, or that were handed the file by an app
 * declaring `application/json`, report JSON instead; a few report `text/plain`.
 *
 * Filtering on only the plausible types therefore greys out real `.issues` files on some devices,
 * which is worse than showing too much. The lists below name the types the format actually resolves
 * to and end with a wildcard so a document is never unpickable — the wildcard is the fallback, not
 * the intent.
 */
object DocumentTypes {

    /** The type a newly created document is registered with. `.issues` is JSON. */
    const val ISSUES_MIME_TYPE = "application/json"

    /** The type markdown exports are created with. */
    const val MARKDOWN_MIME_TYPE = "text/markdown"

    /** The default name offered when creating a document. */
    const val DEFAULT_DOCUMENT_NAME = "Untitled.issues"

    /** Accepted when opening an `.issues` document. */
    val OPEN_ISSUES_TYPES = arrayOf(
        ISSUES_MIME_TYPE,
        "application/octet-stream",
        "text/plain",
        "*/*",
    )

    /** Accepted when importing legacy markdown. */
    val OPEN_MARKDOWN_TYPES = arrayOf(
        MARKDOWN_MIME_TYPE,
        "text/x-markdown",
        "text/plain",
        "*/*",
    )
}
