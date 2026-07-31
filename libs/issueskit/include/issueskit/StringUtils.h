/*
 * StringUtils.h -- small std::string helpers shared by the issueskit core.
 *
 * Part of the portable core: no platform toolkit, so the format code builds and
 * is testable anywhere.
 */
#ifndef ISSUESKIT_STRING_UTILS_H
#define ISSUESKIT_STRING_UTILS_H

#include <string>
#include <vector>

namespace issueskit {

/*!	Trims spaces and tabs from both ends.

	This mirrors Swift's `trimmingCharacters(in: .whitespaces)`, which does NOT
	include carriage returns or newlines. Files with CRLF line endings therefore
	keep their trailing '\r' -- exactly as they do in the Apple app.
*/
std::string Trim(const std::string& text);

//! Splits on '\n', keeping empty components (Swift's components(separatedBy:)).
std::vector<std::string> SplitLines(const std::string& text);

//! Splits on a single character, keeping empty components.
std::vector<std::string> Split(const std::string& text, char separator);

/*!	Splits on \a separator, trims each entry and drops the empty ones.

	Matches the Apple app's `splitEntries(_:separator:)` used for the
	comma-separated Labels/Assignees fields and the newline-separated steps.
*/
std::vector<std::string> SplitTrimNonEmpty(const std::string& text,
	char separator);

std::string Join(const std::vector<std::string>& parts,
	const std::string& separator);

bool StartsWith(const std::string& text, const std::string& prefix);
bool HasPrefixAt(const std::string& text, size_t offset,
	const std::string& prefix);

//! ASCII-only lowercase. Enum display names and section headers are all ASCII.
std::string ToLower(const std::string& text);

//! Formats an issue number as three zero-padded digits ("%03d").
std::string FormatIssueNumber(int number);

/*!	Renders a double the way the Apple app does.

	Whole values lose the decimal point ("3", not "3.0"); everything else uses
	the shortest representation that round-trips through strtod.
*/
std::string FormatDouble(double value);

//! Parses a decimal number; returns false when the whole string is not one.
bool ParseDouble(const std::string& text, double& outValue);

//! Parses a decimal integer; returns false when the whole string is not one.
bool ParseInt(const std::string& text, int& outValue);

} // namespace issueskit

#endif // ISSUESKIT_STRING_UTILS_H
