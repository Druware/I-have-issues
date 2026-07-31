import IssuesKit
import SwiftUI

/// A scrollable, card-style presentation of a single issue.
///
/// The document model comes along so ``Issue/relations`` can be resolved to the issues they
/// point at; nothing here mutates it.
struct IssueDetailView: View {
    let issue: Issue
    let model: IssuesDocumentModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                headerCard
                bodySections
                commentsSection
                relationsSection
                remoteLinksSection
            }
            .padding()
            .frame(maxWidth: 720, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .navigationTitle(issue.displayNumber)
        #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
        #endif
    }

    // MARK: - Header

    private var headerCard: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 16) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(issue.displayNumber)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                    Text(issue.title.isEmpty ? "Untitled" : issue.title)
                        .font(.title2)
                        .fontWeight(.semibold)
                }

                Divider()

                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 10) {
                    metadataRow(
                        label: "Type",
                        value: issue.type.displayName,
                        symbol: issue.type.symbolName,
                        tint: issue.type.tint
                    )
                    metadataRow(
                        label: "Priority",
                        value: issue.priority.displayName,
                        symbol: issue.priority.symbolName,
                        tint: issue.priority.tint
                    )
                    metadataRow(
                        label: "Status",
                        value: issue.status.displayName,
                        symbol: issue.status.symbolName,
                        tint: issue.status.tint
                    )
                    if let resolutionKind = issue.resolutionKind {
                        metadataRow(
                            label: "Resolution",
                            value: resolutionKind.displayName,
                            symbol: "checkmark.seal",
                            tint: .green
                        )
                    }
                    metadataRow(
                        label: "Reported",
                        value: IssueDate.string(from: issue.reported),
                        symbol: "calendar",
                        tint: .secondary
                    )
                    if !issue.reportedBy.isEmpty {
                        metadataRow(label: "Reported by", value: issue.reportedBy, symbol: "person", tint: .secondary)
                    }
                    if !issue.area.isEmpty {
                        metadataRow(label: "Area", value: issue.area, symbol: "square.grid.2x2", tint: .secondary)
                    }
                    if !issue.labels.isEmpty {
                        metadataRow(
                            label: "Labels",
                            value: issue.labels.joined(separator: ", "),
                            symbol: "tag",
                            tint: .secondary
                        )
                    }
                    if !issue.assignees.isEmpty {
                        metadataRow(
                            label: "Assignees",
                            value: issue.assignees.joined(separator: ", "),
                            symbol: "person.2",
                            tint: .secondary
                        )
                    }
                    if let milestone = issue.milestone, !milestone.isEmpty {
                        metadataRow(label: "Milestone", value: milestone, symbol: "flag", tint: .secondary)
                    }
                    if let estimate = issue.estimate {
                        metadataRow(
                            label: "Estimate",
                            value: estimate.formatted(.number),
                            symbol: "gauge.with.needle",
                            tint: .secondary
                        )
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(4)
        }
    }

    private func metadataRow(label: String, value: String, symbol: String, tint: Color) -> some View {
        GridRow {
            Text(label)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .gridColumnAlignment(.leading)
            Label {
                Text(value)
            } icon: {
                Image(systemName: symbol).foregroundStyle(tint)
            }
            .font(.body)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(label): \(value)")
    }

    // MARK: - Body sections

    @ViewBuilder
    private var bodySections: some View {
        section(title: "Description", text: issue.description)
        stepsSection
        section(title: "Environment", text: issue.environment)
        section(title: "Notes / Investigation", text: issue.notes)
        section(title: "Resolution", text: issue.resolution)
    }

    @ViewBuilder
    private func section(title: String, text: String) -> some View {
        if !text.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                Text(title)
                    .font(.headline)
                Text(attributedMarkdown(text))
                    .font(.body)
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }

    private func attributedMarkdown(_ string: String) -> AttributedString {
        let options = AttributedString.MarkdownParsingOptions(
            interpretedSyntax: .inlineOnlyPreservingWhitespace
        )
        return (try? AttributedString(markdown: string, options: options)) ?? AttributedString(string)
    }

    @ViewBuilder
    private var stepsSection: some View {
        if !issue.stepsToReproduce.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                Text("Steps to Reproduce")
                    .font(.headline)
                VStack(alignment: .leading, spacing: 4) {
                    ForEach(Array(issue.stepsToReproduce.enumerated()), id: \.offset) { index, step in
                        HStack(alignment: .firstTextBaseline, spacing: 8) {
                            Text("\(index + 1).")
                                .font(.body.monospacedDigit())
                                .foregroundStyle(.secondary)
                            Text(step)
                                .font(.body)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                }
            }
        }
    }

    // MARK: - Comments

    @ViewBuilder
    private var commentsSection: some View {
        if !issue.comments.isEmpty {
            VStack(alignment: .leading, spacing: 10) {
                Text("Comments")
                    .font(.headline)
                ForEach(issue.comments) { comment in
                    VStack(alignment: .leading, spacing: 4) {
                        HStack(spacing: 6) {
                            Image(systemName: "person.crop.circle")
                                .foregroundStyle(.secondary)
                            Text(comment.author.isEmpty ? "Unknown" : comment.author)
                                .font(.subheadline)
                                .fontWeight(.semibold)
                            Text(IssueDate.string(from: comment.createdAt))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Text(attributedMarkdown(comment.body))
                            .font(.body)
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .accessibilityElement(children: .combine)
                }
            }
        }
    }

    // MARK: - Relations

    @ViewBuilder
    private var relationsSection: some View {
        if !issue.relations.isEmpty {
            VStack(alignment: .leading, spacing: 8) {
                Text("Related Issues")
                    .font(.headline)
                ForEach(Array(issue.relations.enumerated()), id: \.offset) { _, relation in
                    HStack(alignment: .firstTextBaseline, spacing: 8) {
                        Image(systemName: "link")
                            .foregroundStyle(.secondary)
                        Text(relation.kind.displayName)
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                        Text(relatedIssueText(for: relation))
                            .font(.body)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(relation.kind.displayName): \(relatedIssueText(for: relation))")
                }
            }
        }
    }

    private func relatedIssueText(for relation: Relation) -> String {
        guard let related = model.issue(withID: relation.issueID) else {
            return "Missing issue"
        }
        return "\(related.displayNumber) \(related.title.isEmpty ? "Untitled" : related.title)"
    }

    // MARK: - Remote links

    @ViewBuilder
    private var remoteLinksSection: some View {
        if !issue.remoteLinks.isEmpty {
            VStack(alignment: .leading, spacing: 8) {
                Text("Remote Links")
                    .font(.headline)
                ForEach(Array(issue.remoteLinks.enumerated()), id: \.offset) { _, link in
                    HStack(alignment: .firstTextBaseline, spacing: 8) {
                        Image(systemName: "arrow.up.forward.square")
                            .foregroundStyle(.secondary)
                        if let url = link.url {
                            Link("\(link.provider.displayName) \(link.identifier)", destination: url)
                                .font(.body)
                        } else {
                            Text("\(link.provider.displayName) \(link.identifier)")
                                .font(.body)
                        }
                        Spacer(minLength: 0)
                    }
                }
            }
        }
    }
}
