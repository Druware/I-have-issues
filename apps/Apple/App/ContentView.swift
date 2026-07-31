import IssuesKit
import SwiftUI
import UniformTypeIdentifiers

/// The document's root view: a sidebar list of issues beside a detail card, with add / edit / delete.
struct ContentView: View {
    @Binding var document: IssuesDocument

    @State private var selection: Issue.ID?
    @State private var draft: Issue?
    @State private var isConfirmingDelete = false
    @State private var isShowingGitHubSync = false
    @State private var isShowingProjectSettings = false
    @State private var isExportingMarkdown = false
    @State private var isImportingMarkdown = false
    @State private var pendingImportURL: URL?
    @State private var errorAlert: ErrorAlert?

    /// A failure worth interrupting the user for, carrying its own message.
    private struct ErrorAlert {
        var title: String
        var message: String
    }

    var body: some View {
        NavigationSplitView {
            // Without an explicit column width the sidebar collapses to a width where
            // `IssueRowView` has only a few points left for the title.
            sidebar
                .navigationSplitViewColumnWidth(min: 240, ideal: 300, max: 520)
        } detail: {
            detail
        }
        .sheet(item: $draft) { issue in
            IssueEditView(issue: issue) { saved in
                apply(saved)
            }
        }
        .sheet(isPresented: $isShowingGitHubSync) {
            GitHubSyncView(document: $document)
        }
        .sheet(isPresented: $isShowingProjectSettings) {
            ProjectSettingsView(document: $document)
        }
        .focusedSceneValue(\.documentCommands, commandActions)
        .fileExporter(
            isPresented: $isExportingMarkdown,
            document: markdownExportDocument,
            contentType: .issuesMarkdown,
            defaultFilename: exportFilename,
            onCompletion: handleExport
        )
        .fileImporter(
            isPresented: $isImportingMarkdown,
            allowedContentTypes: [.issuesMarkdown, .plainText],
            onCompletion: handleImportSelection
        )
        .confirmationDialog(
            "Replace this document with the imported markdown?",
            isPresented: importConfirmation,
            titleVisibility: .visible
        ) {
            Button("Replace Contents", role: .destructive) { confirmImport() }
            Button("Cancel", role: .cancel) { pendingImportURL = nil }
        } message: {
            Text("Every issue and setting in this document is discarded and replaced by the markdown file.")
        }
        .alert(
            errorAlert?.title ?? "",
            isPresented: errorPresentation,
            presenting: errorAlert
        ) { _ in
            Button("OK", role: .cancel) {}
        } message: { alert in
            Text(alert.message)
        }
    }

    // MARK: - Sidebar

    private var sidebar: some View {
        List(selection: $selection) {
            issueSection(title: "Open", issues: document.model.openIssues, emptyText: "No open issues")
            issueSection(title: "Resolved", issues: document.model.resolvedIssues, emptyText: "No resolved issues")
        }
        .navigationTitle(navigationTitle)
        .safeAreaInset(edge: .bottom, spacing: 0) {
            HStack(spacing: 4) {
                Button(action: beginAdd) {
                    Label("Add Issue", systemImage: "plus")
                }
                Button { isShowingProjectSettings = true } label: {
                    Label("Project Settings", systemImage: "gearshape")
                }
                Button { isShowingGitHubSync = true } label: {
                    Label("Sync to GitHub", systemImage: "arrow.triangle.2.circlepath")
                }
                Spacer()
                Menu {
                    Button { isExportingMarkdown = true } label: {
                        Label("Export as Markdown…", systemImage: "square.and.arrow.up")
                    }
                    Button { isImportingMarkdown = true } label: {
                        Label("Import Legacy Markdown…", systemImage: "square.and.arrow.down")
                    }
                } label: {
                    Label("Markdown", systemImage: "ellipsis.circle")
                }
            }
            .labelStyle(.iconOnly)
            .buttonStyle(.borderless)
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .background(.bar)
        }
    }

    private var navigationTitle: String {
        let name = document.model.project.name.trimmingCharacters(in: .whitespaces)
        return name.isEmpty ? "Issues" : name
    }

    @ViewBuilder
    private func issueSection(title: String, issues: [Issue], emptyText: String) -> some View {
        Section(title) {
            if issues.isEmpty {
                Text(emptyText)
                    .foregroundStyle(.secondary)
                    .font(.callout)
            } else {
                ForEach(issues) { issue in
                    IssueRowView(issue: issue).tag(issue.id)
                }
            }
        }
    }

    // MARK: - Detail

    @ViewBuilder
    private var detail: some View {
        if let issue = selectedIssue {
            IssueDetailView(issue: issue, model: document.model)
                .toolbar {
                    ToolbarItem {
                        Button(action: beginEdit) {
                            Label("Edit", systemImage: "pencil")
                        }
                    }
                    ToolbarItem {
                        Button(role: .destructive) {
                            isConfirmingDelete = true
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                    }
                }
                .confirmationDialog(
                    "Delete \(issue.displayNumber)?",
                    isPresented: $isConfirmingDelete,
                    titleVisibility: .visible
                ) {
                    Button("Delete Issue", role: .destructive) { delete(issue) }
                    Button("Cancel", role: .cancel) {}
                } message: {
                    Text("This removes the issue from the document. This cannot be undone from here.")
                }
        } else {
            ContentUnavailableView(
                "No Issue Selected",
                systemImage: "ladybug",
                description: Text("Select an issue from the sidebar, or add a new one.")
            )
        }
    }

    // MARK: - Derived state

    private var selectedIssue: Issue? {
        guard let selection else { return nil }
        return document.model.issue(withID: selection)
    }

    private var commandActions: DocumentCommandActions {
        DocumentCommandActions(
            documentID: document.model.project.id,
            showProjectSettings: { isShowingProjectSettings = true },
            exportMarkdown: { isExportingMarkdown = true },
            importMarkdown: { isImportingMarkdown = true }
        )
    }

    private var importConfirmation: Binding<Bool> {
        Binding(
            get: { pendingImportURL != nil },
            set: { if !$0 { pendingImportURL = nil } }
        )
    }

    private var errorPresentation: Binding<Bool> {
        Binding(
            get: { errorAlert != nil },
            set: { if !$0 { errorAlert = nil } }
        )
    }

    // MARK: - Actions

    private func beginAdd() {
        draft = Issue(number: document.model.nextNumber)
    }

    private func beginEdit() {
        draft = selectedIssue
    }

    private func apply(_ issue: Issue) {
        if let index = document.model.issues.firstIndex(where: { $0.id == issue.id }) {
            document.model.issues[index] = issue
        } else {
            document.model.issues.append(issue)
        }
        selection = issue.id
    }

    private func delete(_ issue: Issue) {
        document.model.issues.removeAll { $0.id == issue.id }
        if selection == issue.id { selection = nil }
    }

    // MARK: - Markdown export

    /// Rendered only while the exporter is on screen, so a document with many issues is not
    /// serialized on every redraw.
    private var markdownExportDocument: MarkdownExportDocument? {
        guard isExportingMarkdown else { return nil }
        return MarkdownExportDocument(text: IssuesMarkdownSerializer.export(document.model))
    }

    private var exportFilename: String {
        let name = document.model.project.name.trimmingCharacters(in: .whitespaces)
        return name.isEmpty ? "Issues" : name
    }

    private func handleExport(_ result: Result<URL, any Error>) {
        if case let .failure(error) = result {
            errorAlert = ErrorAlert(title: "Export Failed", message: error.localizedDescription)
        }
    }

    // MARK: - Legacy markdown import

    private func handleImportSelection(_ result: Result<URL, any Error>) {
        switch result {
        case let .success(url):
            pendingImportURL = url
        case let .failure(error):
            errorAlert = ErrorAlert(title: "Import Failed", message: error.localizedDescription)
        }
    }

    private func confirmImport() {
        guard let url = pendingImportURL else { return }
        pendingImportURL = nil

        // A file chosen from the importer lives outside the sandbox, so reading it needs the
        // security-scoped resource opened and closed around the read.
        let isScoped = url.startAccessingSecurityScopedResource()
        defer {
            if isScoped { url.stopAccessingSecurityScopedResource() }
        }

        do {
            let data = try Data(contentsOf: url)
            guard let markdown = String(data: data, encoding: .utf8) else {
                throw CocoaError(.fileReadInapplicableStringEncoding)
            }
            document.model = try LegacyMarkdownImporter().importDocument(from: markdown)
            selection = nil
        } catch {
            errorAlert = ErrorAlert(title: "Import Failed", message: error.localizedDescription)
        }
    }
}
