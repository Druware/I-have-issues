import IssuesKit
import SwiftUI

/// A single sidebar row: type icon, id and title, and a priority badge.
struct IssueRowView: View {
    let issue: Issue

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: issue.type.symbolName)
                .foregroundStyle(issue.type.tint)
                .font(.title3)
                .accessibilityLabel(issue.type.displayName)

            VStack(alignment: .leading, spacing: 2) {
                Text(issue.title.isEmpty ? "Untitled" : issue.title)
                    .font(.headline)
                    .lineLimit(1)
                Text(issue.displayNumber)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Spacer(minLength: 8)

            Image(systemName: issue.priority.symbolName)
                .foregroundStyle(issue.priority.tint)
                .font(.body)
                .accessibilityLabel("Priority \(issue.priority.displayName)")
        }
        .padding(.vertical, 2)
    }
}
