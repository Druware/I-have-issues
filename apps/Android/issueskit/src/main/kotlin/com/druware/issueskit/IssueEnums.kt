package com.druware.issueskit

/**
 * An enum persisted by its stable [raw] value and presented by a separate [displayName].
 *
 * Raw values are part of the on-disk `.issues` format and must never change. Display strings
 * are presentation only — reword them freely without touching stored documents.
 */
interface DisplayNamed {
    val raw: String
    val displayName: String
}

/** Matches a case by its human-facing display string, case-insensitively. */
internal fun <T : DisplayNamed> Array<T>.byDisplayName(displayName: String): T? {
    val needle = displayName.trimSpaces().lowercase()
    return firstOrNull { it.displayName.lowercase() == needle }
}

/** Matches a case by its stable raw value, or `null` when unrecognized. */
internal fun <T : DisplayNamed> Array<T>.byRaw(raw: String): T? = firstOrNull { it.raw == raw }

// MARK: - Issue classification

/** The kind of an issue entry. */
enum class IssueType(override val raw: String, override val displayName: String) : DisplayNamed {
    BUG("bug", "Bug"),
    FEATURE("feature", "Feature"),
    TASK("task", "Task"),
    QUESTION("question", "Question");

    companion object {
        /** The default applied when an entry omits or carries an unrecognized type. */
        val DEFAULT = TASK

        fun fromRaw(raw: String): IssueType = entries.toTypedArray().byRaw(raw) ?: DEFAULT

        fun fromDisplayName(displayName: String): IssueType? =
            entries.toTypedArray().byDisplayName(displayName)
    }
}

/** The urgency of an issue entry. */
enum class IssuePriority(override val raw: String, override val displayName: String) : DisplayNamed {
    LOW("low", "Low"),
    MEDIUM("medium", "Medium"),
    HIGH("high", "High"),
    CRITICAL("critical", "Critical");

    companion object {
        /** The default applied when an entry omits or carries an unrecognized priority. */
        val DEFAULT = MEDIUM

        fun fromRaw(raw: String): IssuePriority = entries.toTypedArray().byRaw(raw) ?: DEFAULT

        fun fromDisplayName(displayName: String): IssuePriority? =
            entries.toTypedArray().byDisplayName(displayName)
    }
}

/** The lifecycle state of an issue entry. */
enum class IssueStatus(override val raw: String, override val displayName: String) : DisplayNamed {
    OPEN("open", "Open"),
    IN_PROGRESS("inProgress", "In Progress"),
    BLOCKED("blocked", "Blocked"),
    RESOLVED("resolved", "Resolved");

    companion object {
        /** The default applied when an entry omits or carries an unrecognized status. */
        val DEFAULT = OPEN

        fun fromRaw(raw: String): IssueStatus = entries.toTypedArray().byRaw(raw) ?: DEFAULT

        fun fromDisplayName(displayName: String): IssueStatus? =
            entries.toTypedArray().byDisplayName(displayName)
    }
}

/**
 * Why an issue was closed.
 *
 * Maps onto GitHub's `state_reason` and Azure DevOps' *Resolved Reason*. Unlike the other issue
 * enums this one has no default: an unrecognized raw value decodes to `null`, because inventing a
 * close reason would misreport why work stopped.
 */
enum class ResolutionKind(override val raw: String, override val displayName: String) : DisplayNamed {
    FIXED("fixed", "Fixed"),
    WONT_FIX("wontFix", "Won't Fix"),
    DUPLICATE("duplicate", "Duplicate"),
    CANNOT_REPRODUCE("cannotReproduce", "Cannot Reproduce"),
    BY_DESIGN("byDesign", "By Design");

    companion object {
        /** Unrecognized values decode to `null` rather than to a guessed default. */
        fun fromRaw(raw: String): ResolutionKind? = entries.toTypedArray().byRaw(raw)

        fun fromDisplayName(displayName: String): ResolutionKind? =
            entries.toTypedArray().byDisplayName(displayName)
    }
}

// MARK: - Links

/** How one issue relates to another within the same document. */
enum class RelationKind(override val raw: String, override val displayName: String) : DisplayNamed {
    BLOCKS("blocks", "Blocks"),
    BLOCKED_BY("blockedBy", "Blocked By"),
    DUPLICATE_OF("duplicateOf", "Duplicate Of"),
    RELATED_TO("relatedTo", "Related To"),
    PARENT("parent", "Parent"),
    CHILD("child", "Child");

    companion object {
        /**
         * The default applied to an unrecognized relation, chosen because it is the weakest claim
         * the format can make about two issues.
         */
        val DEFAULT = RELATED_TO

        fun fromRaw(raw: String): RelationKind = entries.toTypedArray().byRaw(raw) ?: DEFAULT

        fun fromDisplayName(displayName: String): RelationKind? =
            entries.toTypedArray().byDisplayName(displayName)
    }
}

/**
 * The external issue tracker a [RemoteLink] points at.
 *
 * Unlike the other issue enums this one does **not** fall back to a default. A provider is sync
 * identity: coercing an unrecognized provider to [Github] would point a GitHub sync at whatever
 * issue happens to carry the same identifier in the user's repository, and re-saving would make
 * the mislabelling permanent. An unknown value is therefore preserved verbatim in [Other] and
 * re-encodes byte-identically.
 *
 * Build [Other] only by decoding. Refer to known providers by their own case so that [Github] and
 * `Other("github")` never both exist in a document — [fromRaw] enforces this.
 */
sealed interface RemoteProvider : DisplayNamed {

    data object Github : RemoteProvider {
        override val raw = "github"
        override val displayName = "GitHub"
    }

    data object AzureDevOps : RemoteProvider {
        override val raw = "azureDevOps"
        override val displayName = "Azure DevOps"
    }

    /** A provider this build does not know about, carrying its raw value unchanged. */
    data class Other(override val raw: String) : RemoteProvider {
        override val displayName: String get() = raw
    }

    /**
     * Whether this link is specifically GitHub. Sync code must gate on this — an unknown provider
     * is never GitHub, however similar its raw value looks.
     */
    val isGitHub: Boolean get() = this == Github

    /** Whether this link is specifically Azure DevOps. */
    val isAzureDevOps: Boolean get() = this == AzureDevOps

    companion object {
        /**
         * The providers this build can actually sync with — what UI pickers should offer.
         * Deliberately not the full set of cases: [Other] has no finite domain.
         *
         * Evaluated lazily: the interface's static initializer runs while `Github` is still being
         * constructed, so an eager list would capture a null entry.
         */
        val selectableCases: List<RemoteProvider> by lazy { listOf(Github, AzureDevOps) }

        /** Never coerces: an unrecognized raw value is preserved verbatim. */
        fun fromRaw(raw: String): RemoteProvider = when (raw) {
            Github.raw -> Github
            AzureDevOps.raw -> AzureDevOps
            else -> Other(raw)
        }
    }
}
