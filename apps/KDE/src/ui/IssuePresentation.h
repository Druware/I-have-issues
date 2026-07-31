/*
 * IssuePresentation.h -- UI-only decoration for the model enums.
 *
 * Kept out of libs/issueskit deliberately, exactly as IssuePresentation.swift is
 * kept out of IssuesKit and IssuePresentation.h out of the Haiku core: the
 * format knows nothing about how it is drawn.
 *
 * Colours come from KColorScheme rather than literal RGB, which is the KDE
 * answer to the rationale in the Apple source ("system semantic colors ... so
 * they adapt to light/dark and contrast"). The Haiku port had to use literal RGB
 * only because ui_color() has no such roles.
 */
#ifndef IHAVEISSUES_ISSUE_PRESENTATION_H
#define IHAVEISSUES_ISSUE_PRESENTATION_H

#include <QColor>
#include <QIcon>
#include <QString>

#include <issueskit/IssueEnums.h>

namespace ihaveissues
{

/*! A themed icon for the issue type.
 *
 *  Resolves the first icon name the current theme actually has; when the theme
 *  has none of them, paints a tinted disc carrying the type's letter, so a
 *  minimal icon theme degrades to something legible rather than to nothing.
 */
QIcon issueTypeIcon(issueskit::IssueType type);

//! B, F, T or ? -- the fallback glyph, also used in tooltips.
QString issueTypeBadgeLetter(issueskit::IssueType type);

//! Semantic tint for the type, from KColorScheme.
QColor issueTypeTint(issueskit::IssueType type);

//! Semantic tint for the priority: a grey-to-red ramp, from KColorScheme.
QColor issuePriorityTint(issueskit::IssuePriority priority);

//! A themed icon for the status, used in the detail pane's metadata rows.
QIcon issueStatusIcon(issueskit::IssueStatus status);

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_PRESENTATION_H
