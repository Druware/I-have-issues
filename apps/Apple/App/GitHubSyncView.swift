import IssuesKit
import SwiftUI

struct GitHubSyncView: View {
    @Binding var document: IssuesDocument
    @Environment(\.dismiss) private var dismiss

    /// Only ever holds what the user types into the field. A stored token is never read back into
    /// the view: the sheet needs to know that one exists, not what it is.
    @State private var enteredToken = ""

    /// Set in `.task`, not here: the Keychain item is scoped to the document's repository, and the
    /// document is not available while the `@State` initial value is being built.
    @State private var hasStoredToken = false

    @State private var isSyncing = false
    @State private var syncResult: GitHubSyncService.SyncResult?
    @State private var errorMessage: String?

    /// Owner and repository belong to the document, not to this sheet — they are edited once,
    /// in Project Settings, so they are never duplicated as editable fields here.
    private var integration: GitHubIntegration? {
        document.model.integrations.github
    }

    private var canSync: Bool {
        guard let integration else { return false }
        return !integration.owner.trimmingCharacters(in: .whitespaces).isEmpty
            && !integration.repository.trimmingCharacters(in: .whitespaces).isEmpty
            && (hasStoredToken || !enteredToken.trimmingCharacters(in: .whitespaces).isEmpty)
            && !isSyncing
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    if let integration {
                        LabeledContent("Owner", value: integration.owner)
                        LabeledContent("Repository", value: integration.repository)
                    } else {
                        Label("No GitHub repository configured", systemImage: "exclamationmark.triangle")
                            .foregroundStyle(.secondary)
                    }
                } header: {
                    Text("GitHub Repository")
                } footer: {
                    Text("Set the owner and repository in Project Settings.")
                }

                Section {
                    SecureField("Personal access token", text: $enteredToken)
                    if hasStoredToken {
                        HStack {
                            Label("Token saved", systemImage: "key.fill")
                                .foregroundStyle(.secondary)
                            Spacer()
                            Button("Remove", role: .destructive) { removeStoredToken() }
                                .buttonStyle(.borderless)
                        }
                    }
                } header: {
                    Text("Authentication")
                } footer: {
                    Text(
                        "Requires the repo scope. Create one at github.com/settings/tokens. "
                            + "The token is stored in the Keychain, never in the document. "
                            + "Entering a token replaces the saved one; leave the field blank to keep it."
                    )
                }

                if let errorMessage {
                    Section {
                        Label(errorMessage, systemImage: "xmark.circle.fill")
                            .foregroundStyle(.red)
                    }
                }

                if let result = syncResult {
                    Section("Last Sync") {
                        if result.created > 0 {
                            Label("\(result.created) created", systemImage: "plus.circle.fill")
                                .foregroundStyle(.green)
                        }
                        if result.updated > 0 {
                            Label("\(result.updated) updated", systemImage: "checkmark.circle.fill")
                                .foregroundStyle(.blue)
                        }
                        if result.failed > 0 {
                            Label("\(result.failed) failed", systemImage: "xmark.circle.fill")
                                .foregroundStyle(.red)
                        }
                        ForEach(result.errors, id: \.self) { error in
                            Text(error)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }
            .task {
                guard let account = GitHubKeychain.account(for: integration) else { return }
                hasStoredToken = GitHubKeychain.hasToken(for: account)
            }
            .navigationTitle("Sync to GitHub")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") {
                        if saveSettings() { dismiss() }
                    }
                }
                ToolbarItem(placement: .confirmationAction) {
                    if isSyncing {
                        ProgressView()
                            #if os(macOS)
                            .scaleEffect(0.7)
                            #endif
                    } else {
                        Button("Sync") { performSync() }
                            .disabled(!canSync)
                    }
                }
            }
        }
    }

    /// Stores a typed token against the document's repository, and reports whether it is now safe
    /// to leave the sheet. With no repository configured there is no account to scope the item to,
    /// so a typed token cannot be stored anywhere — say so instead of dropping it on the floor.
    ///
    /// A blank field means "keep what is stored", not "delete it": the field starts empty every
    /// time, so leaving it alone must not be destructive. Removal is the Remove button's job.
    private func saveSettings() -> Bool {
        guard let account = GitHubKeychain.account(for: integration) else {
            guard enteredToken.isEmpty else {
                errorMessage = "Set the owner and repository in Project Settings before saving a token."
                return false
            }
            return true
        }
        if !enteredToken.isEmpty {
            GitHubKeychain.save(enteredToken, for: account)
            hasStoredToken = true
        }
        return true
    }

    private func removeStoredToken() {
        guard let account = GitHubKeychain.account(for: integration) else { return }
        GitHubKeychain.delete(for: account)
        hasStoredToken = false
        enteredToken = ""
    }

    /// Reads the token back from the Keychain rather than from view state, so the secret only
    /// exists for the length of this call.
    private func performSync() {
        guard let integration, saveSettings() else { return }
        guard let account = GitHubKeychain.account(for: integration),
              let token = GitHubKeychain.load(for: account) else {
            errorMessage = "No GitHub token saved for this repository. Enter one and try again."
            return
        }
        let service = GitHubSyncService(token: token, integration: integration)
        isSyncing = true
        errorMessage = nil
        syncResult = nil
        Task {
            do {
                let (updatedIssues, result) = try await service.sync(issues: document.model.issues)
                document.model.issues = updatedIssues
                syncResult = result
            } catch {
                errorMessage = error.localizedDescription
            }
            isSyncing = false
        }
    }
}
