package com.druware.issueskit

/**
 * Per-project issue-tracker configuration carried inside the `.issues` document.
 *
 * **No credentials belong in this type, ever.** `.issues` files are committed to project
 * repositories, so a token stored here would be published to everyone with read access and to
 * every fork and CI log. Personal access tokens, OAuth tokens, and passwords belong in the
 * platform keystore, keyed by the values below (owner/repository, organization/project). This type
 * holds only the non-secret coordinates needed to find the remote tracker.
 */
data class IntegrationSettings(
    val github: GitHubIntegration? = null,
    val azureDevOps: AzureDevOpsIntegration? = null,
)

/**
 * Where a project's issues live on GitHub, plus the defaults applied to newly pushed issues.
 *
 * Contains no token — see [IntegrationSettings] for why.
 */
data class GitHubIntegration(
    val owner: String = "",
    val repository: String = "",
    val defaultLabels: List<String> = emptyList(),
    val defaultAssignees: List<String> = emptyList(),
    val defaultMilestone: String? = null,
)

/**
 * Where a project's work items live in Azure DevOps, plus the defaults applied to new items.
 *
 * Contains no token — see [IntegrationSettings] for why.
 */
data class AzureDevOpsIntegration(
    val organization: String = "",
    val project: String = "",
    val team: String? = null,
    val areaPath: String? = null,
    val iterationPath: String? = null,
    val defaultWorkItemType: String = "Issue",
)
