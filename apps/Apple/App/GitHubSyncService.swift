import Foundation
import IssuesKit

struct GitHubSyncService {
    struct SyncResult {
        var created: Int = 0
        var updated: Int = 0
        var failed: Int = 0
        var errors: [String] = []
    }

    enum SyncError: LocalizedError {
        case unauthorized
        case apiError(String)
        case invalidResponse
        case invalidRepository
        case invalidRemoteIdentifier(String)

        var errorDescription: String? {
            switch self {
            case .unauthorized: "Invalid or expired GitHub token. Check your personal access token."
            case .apiError(let msg): msg
            case .invalidResponse: "Unexpected response from GitHub API."
            case .invalidRepository:
                "The GitHub owner and repository in Project Settings do not form a valid URL."
            case .invalidRemoteIdentifier(let identifier):
                "The existing GitHub link \"\(identifier)\" is not an issue number."
            }
        }
    }

    /// A GitHub issue as this app reads it back from a create or update response.
    private struct RemoteIssue {
        var number: Int
        var htmlURL: URL?
        var updatedAt: Date?
    }

    let token: String
    /// The per-document GitHub coordinates. Carries no credentials — the token lives in the
    /// Keychain (see ``GitHubKeychain``) and never touches the `.issues` file.
    let integration: GitHubIntegration

    /// Syncs all issues to GitHub. Returns the updated issue array (a `.github` `RemoteLink`
    /// filled in for newly created issues, refreshed timestamps for existing ones) and a result
    /// summary.
    /// Throws on auth failure; per-issue failures are accumulated in the result.
    func sync(issues: [Issue]) async throws -> ([Issue], SyncResult) {
        var updated = issues
        var result = SyncResult()
        let milestones = try await milestoneNumbersIfNeeded(for: issues)

        for i in updated.indices {
            do {
                if let link = updated[i].remoteLinks.firstIndex(where: { $0.provider.isGitHub }) {
                    let identifier = updated[i].remoteLinks[link].identifier
                    guard let ghNum = Int(identifier) else {
                        throw SyncError.invalidRemoteIdentifier(identifier)
                    }
                    let remote = try await updateIssue(updated[i], number: ghNum, milestones: milestones)
                    record(remote, into: &updated[i].remoteLinks[link])
                    result.updated += 1
                } else {
                    var remote = try await createIssue(updated[i], milestones: milestones)
                    if updated[i].isResolved {
                        remote = try await updateIssue(updated[i], number: remote.number, milestones: milestones)
                            ?? remote
                    }
                    updated[i].remoteLinks.append(RemoteLink(
                        provider: .github,
                        identifier: String(remote.number),
                        url: remote.htmlURL,
                        lastSyncedAt: Date(),
                        remoteUpdatedAt: remote.updatedAt
                    ))
                    result.created += 1
                }
            } catch SyncError.unauthorized {
                throw SyncError.unauthorized
            } catch {
                result.failed += 1
                result.errors.append("\(updated[i].displayNumber): \(error.localizedDescription)")
            }
        }

        return (updated, result)
    }

    /// Refreshes an existing link in place. The remote fields are only overwritten when the
    /// response actually carried them, so a terse reply never erases what we already knew.
    private func record(_ remote: RemoteIssue?, into link: inout RemoteLink) {
        link.lastSyncedAt = Date()
        guard let remote else { return }
        if let url = remote.htmlURL { link.url = url }
        if let updatedAt = remote.updatedAt { link.remoteUpdatedAt = updatedAt }
    }

    private func createIssue(_ issue: Issue, milestones: [String: Int]) async throws -> RemoteIssue {
        var req = try apiRequest(path: ["issues"], method: "POST")
        req.httpBody = try JSONSerialization.data(
            withJSONObject: payload(for: issue, milestones: milestones, includeState: false)
        )
        let data = try await perform(req)
        guard let remote = remoteIssue(from: data) else {
            throw SyncError.invalidResponse
        }
        return remote
    }

    private func updateIssue(_ issue: Issue, number: Int, milestones: [String: Int]) async throws -> RemoteIssue? {
        var req = try apiRequest(path: ["issues", String(number)], method: "PATCH")
        req.httpBody = try JSONSerialization.data(
            withJSONObject: payload(for: issue, milestones: milestones, includeState: true)
        )
        let data = try await perform(req)
        return remoteIssue(from: data)
    }

    private func remoteIssue(from data: Data) -> RemoteIssue? {
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let number = json["number"] as? Int else {
            return nil
        }
        return RemoteIssue(
            number: number,
            htmlURL: (json["html_url"] as? String).flatMap { URL(string: $0) },
            updatedAt: (json["updated_at"] as? String).flatMap { try? Date($0, strategy: .iso8601) }
        )
    }

    // MARK: - Payload

    private func payload(for issue: Issue, milestones: [String: Int], includeState: Bool) -> [String: Any] {
        var body: [String: Any] = [
            "title": issueTitle(issue),
            "body": issueBody(issue)
        ]
        let labels = merged(issue.labels, integration.defaultLabels)
        if !labels.isEmpty { body["labels"] = labels }
        let assignees = merged(issue.assignees, integration.defaultAssignees)
        if !assignees.isEmpty { body["assignees"] = assignees }
        // GitHub identifies a milestone by its number, not its title, so a name with no match in
        // the repository is omitted — sending the title would fail the whole request.
        if let name = issue.milestone ?? integration.defaultMilestone, let number = milestones[name] {
            body["milestone"] = number
        }
        if includeState { body["state"] = issue.isResolved ? "closed" : "open" }
        return body
    }

    /// The issue's own values first, then any document defaults it does not already carry.
    private func merged(_ own: [String], _ defaults: [String]) -> [String] {
        own + defaults.filter { !own.contains($0) }
    }

    /// Maps milestone titles to GitHub milestone numbers, fetched once per sync and only when
    /// some issue (or the document default) actually names one.
    private func milestoneNumbersIfNeeded(for issues: [Issue]) async throws -> [String: Int] {
        let isNamed = issues.contains { $0.milestone?.isEmpty == false }
            || integration.defaultMilestone?.isEmpty == false
        guard isNamed else { return [:] }

        let req = try apiRequest(path: ["milestones"], method: "GET", query: [
            URLQueryItem(name: "state", value: "all"),
            URLQueryItem(name: "per_page", value: "100")
        ])
        let data = try await perform(req)
        guard let items = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else {
            return [:]
        }
        return items.reduce(into: [:]) { result, item in
            if let title = item["title"] as? String, let number = item["number"] as? Int {
                result[title] = number
            }
        }
    }

    // MARK: - Requests

    /// Characters safe to leave unescaped in a single path segment. Deliberately excludes `/`
    /// so a stray slash in an owner or repository cannot invent extra path components.
    private static let pathSegmentAllowed = CharacterSet.alphanumerics
        .union(CharacterSet(charactersIn: "-._~"))

    /// Builds a request against `api.github.com`, percent-encoding every path segment.
    ///
    /// Owner and repository are typed by the user, so they may contain spaces or anything else a
    /// URL cannot carry raw. This throws rather than force-unwrapping a `nil` URL.
    private func apiRequest(path: [String], method: String, query: [URLQueryItem] = []) throws -> URLRequest {
        let segments = ["repos", integration.owner, integration.repository] + path
        var encoded: [String] = []
        for segment in segments {
            guard !segment.isEmpty,
                  let escaped = segment.addingPercentEncoding(withAllowedCharacters: Self.pathSegmentAllowed) else {
                throw SyncError.invalidRepository
            }
            encoded.append(escaped)
        }

        var components = URLComponents()
        components.scheme = "https"
        components.host = "api.github.com"
        components.percentEncodedPath = "/" + encoded.joined(separator: "/")
        components.queryItems = query.isEmpty ? nil : query
        guard let url = components.url else {
            throw SyncError.invalidRepository
        }

        var req = URLRequest(url: url)
        req.httpMethod = method
        req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        req.setValue("2022-11-28", forHTTPHeaderField: "X-GitHub-Api-Version")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        return req
    }

    private func perform(_ request: URLRequest) async throws -> Data {
        let (data, response) = try await URLSession.shared.data(for: request)
        guard let http = response as? HTTPURLResponse else { return data }
        switch http.statusCode {
        case 200 ..< 300:
            return data
        case 401:
            throw SyncError.unauthorized
        default:
            let msg = (try? JSONSerialization.jsonObject(with: data) as? [String: Any])?["message"] as? String
                ?? "HTTP \(http.statusCode)"
            throw SyncError.apiError(msg)
        }
    }

    private func issueTitle(_ issue: Issue) -> String {
        issue.title.isEmpty ? "Untitled Issue" : issue.title
    }

    private func issueBody(_ issue: Issue) -> String {
        var parts: [String] = []

        parts.append(
            "**Type:** \(issue.type.displayName) | **Priority:** \(issue.priority.displayName)"
                + " | **Status:** \(issue.status.displayName)"
        )

        var dateLine = "**Reported:** \(IssueDate.string(from: issue.reported))"
        if !issue.reportedBy.isEmpty { dateLine += " · \(issue.reportedBy)" }
        if !issue.area.isEmpty { dateLine += " | **Area:** \(issue.area)" }
        parts.append(dateLine)

        if !issue.description.isEmpty {
            parts.append("---\n**Description**\n\n\(issue.description)")
        }
        if !issue.stepsToReproduce.isEmpty {
            let numbered = issue.stepsToReproduce.enumerated()
                .map { "\($0.offset + 1). \($0.element)" }
                .joined(separator: "\n")
            parts.append("**Steps to Reproduce**\n\n\(numbered)")
        }
        if !issue.notes.isEmpty {
            parts.append("**Notes / Investigation**\n\n\(issue.notes)")
        }
        if !issue.resolution.isEmpty {
            parts.append("**Resolution**\n\n\(issue.resolution)")
        }

        parts.append("---\n*Synced from local issue tracker · \(issue.displayNumber)*")
        return parts.joined(separator: "\n\n")
    }
}
