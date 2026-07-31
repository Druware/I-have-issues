import IssuesKit
import SwiftUI

/// Per-document project identity and issue-tracker coordinates, presented as a sheet.
///
/// Like ``IssueEditView`` this edits a local draft and only writes back on Save, so cancelling
/// discards everything.
///
/// > Important: No credentials are edited or stored here. `.issues` documents are committed to
/// > project repositories, so tokens live in the Keychain (see ``GitHubKeychain``) and only the
/// > non-secret coordinates below are written to the file.
struct ProjectSettingsView: View {
    @Binding var document: IssuesDocument
    @Environment(\.dismiss) private var dismiss

    @State private var name: String
    @State private var summary: String

    @State private var isGitHubConfigured: Bool
    @State private var githubOwner: String
    @State private var githubRepository: String
    @State private var githubLabels: String
    @State private var githubAssignees: String
    @State private var githubMilestone: String

    @State private var isAzureConfigured: Bool
    @State private var azureOrganization: String
    @State private var azureProject: String
    @State private var azureTeam: String
    @State private var azureAreaPath: String
    @State private var azureIterationPath: String
    @State private var azureWorkItemType: String

    init(document: Binding<IssuesDocument>) {
        _document = document
        let model = document.wrappedValue.model

        _name = State(initialValue: model.project.name)
        _summary = State(initialValue: model.project.summary)

        // When the document has no GitHub block, offer whatever the pre-document build left in
        // UserDefaults as a starting point. Nothing is written until the user saves the sheet.
        let github = model.integrations.github
        let defaults = UserDefaults.standard
        _isGitHubConfigured = State(initialValue: github != nil)
        _githubOwner = State(initialValue: github?.owner ?? defaults.string(forKey: "github.owner") ?? "")
        _githubRepository = State(initialValue: github?.repository ?? defaults.string(forKey: "github.repo") ?? "")
        _githubLabels = State(initialValue: github?.defaultLabels.joined(separator: ", ") ?? "")
        _githubAssignees = State(initialValue: github?.defaultAssignees.joined(separator: ", ") ?? "")
        _githubMilestone = State(initialValue: github?.defaultMilestone ?? "")

        let azure = model.integrations.azureDevOps
        _isAzureConfigured = State(initialValue: azure != nil)
        _azureOrganization = State(initialValue: azure?.organization ?? "")
        _azureProject = State(initialValue: azure?.project ?? "")
        _azureTeam = State(initialValue: azure?.team ?? "")
        _azureAreaPath = State(initialValue: azure?.areaPath ?? "")
        _azureIterationPath = State(initialValue: azure?.iterationPath ?? "")
        _azureWorkItemType = State(initialValue: azure?.defaultWorkItemType ?? "Issue")
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Project") {
                    TextField("Name", text: $name)
                    TextField("Summary", text: $summary, axis: .vertical)
                }

                gitHubSection
                azureSection
            }
            .formStyle(.grouped)
            .navigationTitle("Project Settings")
            #if os(iOS)
                .navigationBarTitleDisplayMode(.inline)
            #endif
                .toolbar {
                    ToolbarItem(placement: .cancellationAction) {
                        Button("Cancel") { dismiss() }
                    }
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Save") { save() }
                    }
                }
        }
        #if os(macOS)
        .frame(minWidth: 460, minHeight: 560)
        #endif
    }

    // MARK: - Integrations

    @ViewBuilder
    private var gitHubSection: some View {
        Section {
            Toggle("Configure GitHub", isOn: $isGitHubConfigured)
            if isGitHubConfigured {
                TextField("Owner", text: $githubOwner)
                    #if os(iOS)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    #endif
                TextField("Repository", text: $githubRepository)
                    #if os(iOS)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    #endif
                TextField("Default labels", text: $githubLabels)
                TextField("Default assignees", text: $githubAssignees)
                TextField("Default milestone", text: $githubMilestone)
            }
        } header: {
            Text("GitHub")
        } footer: {
            Text(
                "Separate multiple labels or assignees with commas. "
                    + "The personal access token is kept in the Keychain and is never saved into "
                    + "this document — .issues files are meant to be committed to a repository."
            )
        }
    }

    @ViewBuilder
    private var azureSection: some View {
        Section {
            Toggle("Configure Azure DevOps", isOn: $isAzureConfigured)
            if isAzureConfigured {
                TextField("Organization", text: $azureOrganization)
                    #if os(iOS)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    #endif
                TextField("Project", text: $azureProject)
                TextField("Team", text: $azureTeam)
                TextField("Area path", text: $azureAreaPath)
                TextField("Iteration path", text: $azureIterationPath)
                TextField("Default work item type", text: $azureWorkItemType)
            }
        } header: {
            Text("Azure DevOps")
        } footer: {
            Text("Team, area path, and iteration path are optional. Credentials stay in the Keychain.")
        }
    }

    // MARK: - Saving

    private func save() {
        document.model.project.name = name.trimmingCharacters(in: .whitespaces)
        document.model.project.summary = summary
        document.model.integrations.github = isGitHubConfigured ? gitHubIntegration() : nil
        document.model.integrations.azureDevOps = isAzureConfigured ? azureIntegration() : nil
        dismiss()
    }

    private func gitHubIntegration() -> GitHubIntegration {
        GitHubIntegration(
            owner: githubOwner.trimmingCharacters(in: .whitespaces),
            repository: githubRepository.trimmingCharacters(in: .whitespaces),
            defaultLabels: splitEntries(githubLabels),
            defaultAssignees: splitEntries(githubAssignees),
            defaultMilestone: optional(githubMilestone)
        )
    }

    private func azureIntegration() -> AzureDevOpsIntegration {
        AzureDevOpsIntegration(
            organization: azureOrganization.trimmingCharacters(in: .whitespaces),
            project: azureProject.trimmingCharacters(in: .whitespaces),
            team: optional(azureTeam),
            areaPath: optional(azureAreaPath),
            iterationPath: optional(azureIterationPath),
            defaultWorkItemType: optional(azureWorkItemType) ?? "Issue"
        )
    }

    /// Splits a comma-separated field into trimmed, non-empty entries.
    private func splitEntries(_ text: String) -> [String] {
        text.split(separator: ",", omittingEmptySubsequences: true)
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    /// An optional field is `nil` when blank, never an empty string.
    private func optional(_ text: String) -> String? {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        return trimmed.isEmpty ? nil : trimmed
    }
}
