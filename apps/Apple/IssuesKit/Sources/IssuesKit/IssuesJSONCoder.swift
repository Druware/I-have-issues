import Foundation

/// Reads and writes the `.issues` JSON document format.
///
/// Output is deliberately diff-friendly, because `.issues` files live in project
/// repositories: keys are sorted, the JSON is pretty-printed, slashes are not escaped, dates
/// are ISO-8601, and the file ends with a newline. Encoding the same model twice therefore
/// produces identical bytes.
public enum IssuesJSONCoder {
    /// Encodes a model to UTF-8 JSON with a trailing newline.
    /// - Throws: ``IssuesError/encodingFailed(_:)``.
    public static func encode(_ model: IssuesDocumentModel) throws(IssuesError) -> Data {
        var data: Data
        do {
            data = try encoder.encode(model)
        } catch {
            throw IssuesError.encodingFailed(String(describing: error))
        }
        if data.last != newline { data.append(newline) }
        return data
    }

    /// Decodes UTF-8 JSON into a model.
    /// - Throws: ``IssuesError/missingSchemaVersion``,
    ///   ``IssuesError/unsupportedSchemaVersion(found:supported:)``, or
    ///   ``IssuesError/decodingFailed(_:)``.
    public static func decode(_ data: Data) throws(IssuesError) -> IssuesDocumentModel {
        do {
            return try decoder.decode(IssuesDocumentModel.self, from: data)
        } catch let error as IssuesError {
            throw error
        } catch {
            throw IssuesError.decodingFailed(String(describing: error))
        }
    }

    // MARK: - Coders

    private static let newline = UInt8(ascii: "\n")

    private static let encoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }()

    private static let decoder: JSONDecoder = {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }()
}
