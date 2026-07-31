/*
 * IssuePresentation.h -- UI-only decoration for the model enums.
 *
 * Kept out of libs/issueskit deliberately, exactly as IssuePresentation.swift is
 * kept out of IssuesKit: the format knows nothing about how it is drawn.
 *
 * Haiku has no SF Symbols, so the Apple app's symbol-per-case mapping becomes a
 * one- or two-letter badge plus a tint. The tints are the closest fixed
 * equivalents of Apple's semantic colours; Haiku's ui_color() palette has no
 * "yellow"/"orange"/"pink" roles to borrow, so these are literal RGB.
 */
#ifndef IHAVEISSUES_ISSUE_PRESENTATION_H
#define IHAVEISSUES_ISSUE_PRESENTATION_H

#include <GraphicsDefs.h>

#include <issueskit/IssueEnums.h>

namespace ihaveissues {

//! A single letter drawn inside the type badge: B, F, T or Q.
const char* IssueTypeBadge(issueskit::IssueType type);
rgb_color IssueTypeTint(issueskit::IssueType type);

//! An arrow or bar glyph for priority, low to critical.
const char* IssuePriorityBadge(issueskit::IssuePriority priority);
rgb_color IssuePriorityTint(issueskit::IssuePriority priority);

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_PRESENTATION_H
