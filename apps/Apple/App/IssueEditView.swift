import IssuesKit
import SwiftUI

/// A modal editor for a single issue, presented as a sheet.
///
/// It edits a local draft and only writes back through `onSave`, so cancelling discards changes.
struct IssueEditView: View {
    @Environment(\.dismiss) private var dismiss

    @State private var draft: Issue
    @State private var stepsText: String
    @State private var labelsText: String
    @State private var assigneesText: String
    @State private var milestoneText: String
    @State private var estimateText: String
    let onSave: (Issue) -> Void

    init(issue: Issue, onSave: @escaping (Issue) -> Void) {
        _draft = State(initialValue: issue)
        _stepsText = State(initialValue: issue.stepsToReproduce.joined(separator: "\n"))
        _labelsText = State(initialValue: issue.labels.joined(separator: ", "))
        _assigneesText = State(initialValue: issue.assignees.joined(separator: ", "))
        _milestoneText = State(initialValue: issue.milestone ?? "")
        _estimateText = State(initialValue: issue.estimate.map(Self.estimateString) ?? "")
        self.onSave = onSave
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Summary") {
                    TextField("Title", text: $draft.title, axis: .vertical)

                    Picker("Type", selection: $draft.type) {
                        ForEach(IssueType.allCases, id: \.self) { Text($0.displayName).tag($0) }
                    }
                    Picker("Priority", selection: $draft.priority) {
                        ForEach(IssuePriority.allCases, id: \.self) { Text($0.displayName).tag($0) }
                    }
                    Picker("Status", selection: $draft.status) {
                        ForEach(IssueStatus.allCases, id: \.self) { Text($0.displayName).tag($0) }
                    }
                    // `ResolutionKind` has no default — an issue that is still open has none.
                    Picker("Resolution", selection: $draft.resolutionKind) {
                        Text("None").tag(ResolutionKind?.none)
                        ForEach(ResolutionKind.allCases, id: \.self) {
                            Text($0.displayName).tag(ResolutionKind?.some($0))
                        }
                    }
                }

                Section("Details") {
                    // The document stores a reported date as a calendar day at GMT midnight
                    // (see `IssueDate`), so the picker has to read and write it in GMT too —
                    // interpreting it in the local zone shifts the displayed day, and writing
                    // back a local midnight would save the wrong day east of GMT.
                    DatePicker("Reported", selection: $draft.reported, displayedComponents: .date)
                        .environment(\.timeZone, .gmt)
                    TextField("Reported by", text: $draft.reportedBy)
                    TextField("Area", text: $draft.area)
                    TextField("Milestone", text: $milestoneText)
                    TextField("Estimate", text: $estimateText)
                        #if os(iOS)
                        .keyboardType(.decimalPad)
                        #endif
                }

                Section {
                    TextField("Labels", text: $labelsText)
                    TextField("Assignees", text: $assigneesText)
                } header: {
                    Text("Labels & People")
                } footer: {
                    Text("Separate multiple entries with commas.")
                }

                Section("Description") {
                    TextEditor(text: $draft.description)
                        .frame(minHeight: 88)
                }

                Section("Steps to Reproduce") {
                    TextEditor(text: $stepsText)
                        .frame(minHeight: 88)
                    Text("One step per line.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Section("Environment") {
                    TextEditor(text: $draft.environment)
                        .frame(minHeight: 66)
                }

                Section("Notes / Investigation") {
                    TextEditor(text: $draft.notes)
                        .frame(minHeight: 88)
                }

                Section("Resolution") {
                    TextEditor(text: $draft.resolution)
                        .frame(minHeight: 88)
                }
            }
            .formStyle(.grouped)
            .navigationTitle(draft.displayNumber)
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
        .frame(minWidth: 420, minHeight: 560)
        #endif
    }

    private func save() {
        var result = draft
        result.stepsToReproduce = splitEntries(stepsText, separator: "\n")
        result.labels = splitEntries(labelsText, separator: ",")
        result.assignees = splitEntries(assigneesText, separator: ",")

        let milestone = milestoneText.trimmingCharacters(in: .whitespaces)
        result.milestone = milestone.isEmpty ? nil : milestone
        result.estimate = Double(estimateText.trimmingCharacters(in: .whitespaces))

        result.updatedAt = Date()
        onSave(result)
        dismiss()
    }

    /// Splits a free-text field into trimmed, non-empty entries.
    private func splitEntries(_ text: String, separator: Character) -> [String] {
        text.split(separator: separator, omittingEmptySubsequences: true)
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    /// Renders an estimate the way `Double(_: String)` reads it back, so editing round-trips
    /// regardless of locale — a localized `.formatted(.number)` string would not.
    private static func estimateString(_ value: Double) -> String {
        guard value.rounded() == value, let whole = Int(exactly: value.rounded()) else {
            return String(value)
        }
        return String(whole)
    }
}
