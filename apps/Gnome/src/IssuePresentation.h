/*
 * IssuePresentation.h -- UI-only decoration for the model enums.
 *
 * Kept out of libs/issueskit deliberately, exactly as IssuePresentation.swift
 * is kept out of IssuesKit and IssuePresentation.h out of the Haiku core: the
 * format knows nothing about how it is drawn.
 *
 * Two mappings live here:
 *
 *   - an icon name per case. Every name below is from the freedesktop icon
 *     naming specification or has been in adwaita-icon-theme for many years,
 *     because a name the theme does not have renders as "image-missing" rather
 *     than failing to build. There is no GNOME equivalent of Apple's
 *     ladybug.fill, so the choices are approximations;
 *   - a style class per case, taken ONLY from the stock libadwaita/GTK
 *     palette (.error, .warning, .accent, .success, .dim-label). Those follow
 *     the user's theme and accent colour, which literal RGB -- what the Haiku
 *     port had to use -- does not.
 */
#ifndef IHAVEISSUES_ISSUE_PRESENTATION_H
#define IHAVEISSUES_ISSUE_PRESENTATION_H

#include <string>

#include <issueskit/IssueEnums.h>
#include <issueskit/IssueModel.h>

namespace ihaveissues {

//! The icon shown at the leading edge of a list row.
const char* IssueTypeIconName(issueskit::IssueType type);
//! A stock style class, or NULL to leave the icon in the default foreground.
const char* IssueTypeStyleClass(issueskit::IssueType type);

//! The icon shown at the trailing edge of a list row.
const char* IssuePriorityIconName(issueskit::IssuePriority priority);
const char* IssuePriorityStyleClass(issueskit::IssuePriority priority);

const char* IssueStatusIconName(issueskit::IssueStatus status);
const char* IssueStatusStyleClass(issueskit::IssueStatus status);

/*!	The row title as Pango markup: bold, and "Untitled" when blank.

	The title is user text, so it is escaped before the bold tags go round it.
*/
std::string IssueTitleMarkup(const issueskit::Issue& issue);

//! The row title as plain text: the issue title, or "Untitled" when blank.
std::string IssueTitlePlain(const issueskit::Issue& issue);

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_PRESENTATION_H
