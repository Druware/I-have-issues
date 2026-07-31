import IssuesKit
import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    /// The `.issues` JSON document format this app owns, declared in `UTExportedTypeDeclarations`.
    static let issues = UTType(exportedAs: "com.openbcm.issues")

    /// Markdown text (`net.daringfireball.markdown`), declared as an *imported* type.
    ///
    /// Markdown is no longer a document type — it is only the target of "Export as Markdown…"
    /// and the source of "Import Legacy Markdown…".
    static let issuesMarkdown = UTType(importedAs: "net.daringfireball.markdown")
}

/// A value-type document wrapping the parsed issues model for SwiftUI's `DocumentGroup`.
struct IssuesDocument: FileDocument {
    var model: IssuesDocumentModel

    init() {
        model = .makeEmpty()
    }

    static let readableContentTypes: [UTType] = [.issues]
    static let writableContentTypes: [UTType] = [.issues]

    /// Reads an `.issues` file.
    ///
    /// A decode failure propagates as ``IssuesError``, which is a `LocalizedError`. That is
    /// deliberate: a file written by a newer build must explain itself ("this document uses
    /// issues format version 2…") rather than surface as a generic "corrupt file".
    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents else {
            throw CocoaError(.fileReadCorruptFile)
        }
        model = try IssuesJSONCoder.decode(data)
    }

    func fileWrapper(configuration _: WriteConfiguration) throws -> FileWrapper {
        FileWrapper(regularFileWithContents: try IssuesJSONCoder.encode(model))
    }
}

/// A plain-text payload for `.fileExporter`, used only by "Export as Markdown…".
///
/// The `.issues` JSON is the source of truth; this type exists purely to hand already-rendered
/// markdown to the system save panel.
struct MarkdownExportDocument: FileDocument {
    var text: String

    init(text: String) {
        self.text = text
    }

    static let readableContentTypes: [UTType] = [.issuesMarkdown]

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents,
              let text = String(data: data, encoding: .utf8) else {
            throw CocoaError(.fileReadCorruptFile)
        }
        self.text = text
    }

    func fileWrapper(configuration _: WriteConfiguration) throws -> FileWrapper {
        FileWrapper(regularFileWithContents: Data(text.utf8))
    }
}
