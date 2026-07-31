import Foundation

// MARK: - Integration settings

/// Per-project issue-tracker configuration carried inside the `.issues` document.
///
/// > Important: **No credentials belong in this type, ever.** `.issues` files are committed to
/// > project repositories, so a token stored here would be published to everyone with read
/// > access and to every fork and CI log. Personal access tokens, OAuth tokens, and passwords
/// > live in the Keychain, keyed by the values below (owner/repository, organization/project).
/// > This type holds only the non-secret coordinates needed to find the remote tracker.
public struct IntegrationSettings: Codable, Equatable, Sendable {
    public var github: GitHubIntegration?
    public var azureDevOps: AzureDevOpsIntegration?

    public init(github: GitHubIntegration? = nil, azureDevOps: AzureDevOpsIntegration? = nil) {
        self.github = github
        self.azureDevOps = azureDevOps
    }

    private enum CodingKeys: String, CodingKey {
        case github, azureDevOps
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            github: try container.decodeIfPresent(GitHubIntegration.self, forKey: .github),
            azureDevOps: try container.decodeIfPresent(AzureDevOpsIntegration.self, forKey: .azureDevOps)
        )
    }
}

// MARK: - GitHub

/// Where a project's issues live on GitHub, plus the defaults applied to newly pushed issues.
///
/// Contains no token — see ``IntegrationSettings`` for why.
public struct GitHubIntegration: Codable, Equatable, Sendable {
    public var owner: String
    public var repository: String
    public var defaultLabels: [String]
    public var defaultAssignees: [String]
    public var defaultMilestone: String?

    public init(
        owner: String = "",
        repository: String = "",
        defaultLabels: [String] = [],
        defaultAssignees: [String] = [],
        defaultMilestone: String? = nil
    ) {
        self.owner = owner
        self.repository = repository
        self.defaultLabels = defaultLabels
        self.defaultAssignees = defaultAssignees
        self.defaultMilestone = defaultMilestone
    }

    private enum CodingKeys: String, CodingKey {
        case owner, repository, defaultLabels, defaultAssignees, defaultMilestone
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            owner: try container.decodeIfPresent(String.self, forKey: .owner) ?? "",
            repository: try container.decodeIfPresent(String.self, forKey: .repository) ?? "",
            defaultLabels: try container.decodeIfPresent([String].self, forKey: .defaultLabels) ?? [],
            defaultAssignees: try container.decodeIfPresent([String].self, forKey: .defaultAssignees) ?? [],
            defaultMilestone: try container.decodeIfPresent(String.self, forKey: .defaultMilestone)
        )
    }
}

// MARK: - Azure DevOps

/// Where a project's work items live in Azure DevOps, plus the defaults applied to new items.
///
/// Contains no token — see ``IntegrationSettings`` for why.
public struct AzureDevOpsIntegration: Codable, Equatable, Sendable {
    public var organization: String
    public var project: String
    public var team: String?
    public var areaPath: String?
    public var iterationPath: String?
    public var defaultWorkItemType: String

    public init(
        organization: String = "",
        project: String = "",
        team: String? = nil,
        areaPath: String? = nil,
        iterationPath: String? = nil,
        defaultWorkItemType: String = "Issue"
    ) {
        self.organization = organization
        self.project = project
        self.team = team
        self.areaPath = areaPath
        self.iterationPath = iterationPath
        self.defaultWorkItemType = defaultWorkItemType
    }

    private enum CodingKeys: String, CodingKey {
        case organization, project, team, areaPath, iterationPath, defaultWorkItemType
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            organization: try container.decodeIfPresent(String.self, forKey: .organization) ?? "",
            project: try container.decodeIfPresent(String.self, forKey: .project) ?? "",
            team: try container.decodeIfPresent(String.self, forKey: .team),
            areaPath: try container.decodeIfPresent(String.self, forKey: .areaPath),
            iterationPath: try container.decodeIfPresent(String.self, forKey: .iterationPath),
            defaultWorkItemType: try container.decodeIfPresent(String.self, forKey: .defaultWorkItemType) ?? "Issue"
        )
    }
}
