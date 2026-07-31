/*
 * IssueDate.h -- fixed UTC date handling for the .issues format.
 *
 * Two representations live in the format:
 *   - full timestamps (createdAt, updatedAt, closedAt, comment createdAt,
 *     remoteLink lastSyncedAt/remoteUpdatedAt) written as ISO-8601 UTC with
 *     whole-second precision;
 *   - day-normalized dates (issue.reported, milestone.dueOn) which are the same
 *     ISO-8601 timestamp type pinned to midnight UTC and displayed YYYY-MM-DD.
 *
 * Every conversion here is fixed-UTC. It never consults the local time zone, so
 * a "reported" day does not shift for a user east or west of GMT. The civil
 * date maths is implemented inline (days_from_civil / civil_from_days) rather
 * than via timegm()/gmtime_r() so the core has no libc portability surface.
 */
#ifndef ISSUESKIT_ISSUE_DATE_H
#define ISSUESKIT_ISSUE_DATE_H

#include <stdint.h>

#include <string>

namespace issueskit {

//! Seconds since 1970-01-01T00:00:00Z. Negative values are pre-epoch.
typedef int64_t Timestamp;

namespace IssueDate {

//! The current wall-clock time, whole seconds.
Timestamp Now();

//! Today's date normalized to midnight UTC.
Timestamp Today();

//! Normalizes any timestamp to midnight UTC of the same UTC calendar day.
Timestamp StartOfDay(Timestamp value);

//! Parses "YYYY-MM-DD" (surrounding spaces tolerated) into midnight UTC.
bool Parse(const std::string& text, Timestamp& outValue);

//! Formats as "YYYY-MM-DD" in UTC.
std::string ToString(Timestamp value);

/*!	Parses an ISO-8601 timestamp.

	Accepts "YYYY-MM-DDTHH:MM:SS" with an optional fractional part (truncated,
	because the format carries whole seconds only) and an optional "Z" or
	"+HH:MM" / "-HH:MM" offset. A missing offset is read as UTC.
*/
bool ParseISO8601(const std::string& text, Timestamp& outValue);

/*!	Formats as "YYYY-MM-DDTHH:MM:SSZ".

	This is the exact shape Foundation's `.iso8601` encoding strategy writes, and
	the only shape this app is allowed to write into a .issues file.
*/
std::string ToISO8601(Timestamp value);

} // namespace IssueDate

} // namespace issueskit

#endif // ISSUESKIT_ISSUE_DATE_H
