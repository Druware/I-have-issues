import IssuesKit
import SwiftUI

// UI-only presentation for the model enums. Kept in the app layer so IssuesKit stays UI-free.
// Colors are system semantic colors (not hard-coded hex) so they adapt to light/dark and contrast.

extension IssueType {
    var symbolName: String {
        switch self {
        case .bug: "ladybug.fill"
        case .feature: "sparkles"
        case .task: "checklist"
        case .question: "questionmark.circle.fill"
        }
    }

    var tint: Color {
        switch self {
        case .bug: .pink
        case .feature: .purple
        case .task: .blue
        case .question: .teal
        }
    }
}

extension IssuePriority {
    var symbolName: String {
        switch self {
        case .low: "arrow.down.circle.fill"
        case .medium: "equal.circle.fill"
        case .high: "exclamationmark.triangle.fill"
        case .critical: "exclamationmark.octagon.fill"
        }
    }

    var tint: Color {
        switch self {
        case .low: .secondary
        case .medium: .yellow
        case .high: .orange
        case .critical: .red
        }
    }
}

extension IssueStatus {
    var symbolName: String {
        switch self {
        case .open: "circle"
        case .inProgress: "clock"
        case .blocked: "hand.raised.fill"
        case .resolved: "checkmark.circle.fill"
        }
    }

    var tint: Color {
        switch self {
        case .open: .secondary
        case .inProgress: .blue
        case .blocked: .orange
        case .resolved: .green
        }
    }
}

extension Issue {
    /// The `#NNN` display form of the issue's human display number, zero-padded to three digits.
    ///
    /// This formats ``Issue/number``, never ``Issue/uuid`` — the UUID is sync identity, not
    /// something a person reads.
    var displayNumber: String {
        String(format: "#%03d", number)
    }
}
