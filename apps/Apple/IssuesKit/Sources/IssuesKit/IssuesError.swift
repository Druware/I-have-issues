import Foundation

/// Errors thrown while reading or writing an issues document.
public enum IssuesError: Error, Equatable, Sendable {
    /// The JSON has no `schemaVersion`, so it is not an `.issues` document.
    case missingSchemaVersion
    /// The document was written by a newer build than this one can read.
    case unsupportedSchemaVersion(found: Int, supported: Int)
    /// The JSON could not be decoded; the payload is the underlying decoding failure.
    case decodingFailed(String)
    /// The model could not be encoded; the payload is the underlying encoding failure.
    case encodingFailed(String)
    /// A legacy markdown file has no level-2 headings, so it is not a recognizable issues file.
    case missingOpenSection
}

extension IssuesError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .missingSchemaVersion:
            "The file does not declare a \"schemaVersion\" and is not a valid issues document."
        case let .unsupportedSchemaVersion(found, supported):
            "This document uses issues format version \(found), but this version of the app "
                + "only reads up to version \(supported). Update the app to open it."
        case let .decodingFailed(detail):
            "The issues document could not be read: \(detail)"
        case let .encodingFailed(detail):
            "The issues document could not be written: \(detail)"
        case .missingOpenSection:
            "The markdown file does not contain an \"## Open\" section and cannot be imported."
        }
    }
}
