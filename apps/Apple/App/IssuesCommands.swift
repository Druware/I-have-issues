import SwiftUI

/// The document-level actions the macOS menu bar invokes on the front-most document.
///
/// `ContentView` publishes these with `focusedSceneValue`; the closures only raise sheets, so
/// the menu never needs to reach into the document itself.
struct DocumentCommandActions: Equatable {
    /// Identifies the document these actions belong to — closures cannot be compared, and the
    /// bindings they capture stay valid for the lifetime of the scene.
    var documentID: UUID
    var showProjectSettings: () -> Void
    var exportMarkdown: () -> Void
    var importMarkdown: () -> Void

    static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.documentID == rhs.documentID
    }
}

extension FocusedValues {
    @Entry var documentCommands: DocumentCommandActions?
}

/// File-menu entries for the commands that have no `DocumentGroup` equivalent.
///
/// iOS has no menu bar, so the same actions are also reachable from the toolbar menu in
/// `ContentView`.
struct IssuesCommands: Commands {
    @FocusedValue(\.documentCommands) private var commands

    var body: some Commands {
        CommandGroup(after: .importExport) {
            Button("Import Legacy Markdown…") { commands?.importMarkdown() }
                .disabled(commands == nil)
            Button("Export as Markdown…") { commands?.exportMarkdown() }
                .disabled(commands == nil)
        }
        CommandGroup(after: .appSettings) {
            Button("Project Settings…") { commands?.showProjectSettings() }
                .disabled(commands == nil)
        }
    }
}
