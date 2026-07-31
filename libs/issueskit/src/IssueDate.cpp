/*
 * IssueDate.cpp
 */
#include <issueskit/IssueDate.h>

#include <cstdio>
#include <ctime>

#include <issueskit/StringUtils.h>

namespace issueskit {
namespace IssueDate {

namespace {

const int64_t kSecondsPerDay = 86400;


/*!	Days since 1970-01-01 for a proleptic Gregorian y/m/d.

	Howard Hinnant's days_from_civil. Valid for the full int64 range we care
	about and completely independent of the C library's time zone handling.
*/
int64_t
DaysFromCivil(int64_t year, unsigned month, unsigned day)
{
	year -= month <= 2 ? 1 : 0;
	const int64_t era = (year >= 0 ? year : year - 399) / 400;
	const unsigned yoe = (unsigned)(year - era * 400);			// [0, 399]
	const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5
		+ day - 1;												// [0, 365]
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;	// [0, 146096]
	return era * 146097 + (int64_t)doe - 719468;
}


//! The inverse of DaysFromCivil.
void
CivilFromDays(int64_t days, int64_t& outYear, unsigned& outMonth,
	unsigned& outDay)
{
	days += 719468;
	const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
	const unsigned doe = (unsigned)(days - era * 146097);		// [0, 146096]
	const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int64_t year = (int64_t)yoe + era * 400;
	const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);// [0, 365]
	const unsigned mp = (5 * doy + 2) / 153;					// [0, 11]
	const unsigned day = doy - (153 * mp + 2) / 5 + 1;			// [1, 31]
	const unsigned month = mp + (mp < 10 ? 3 : -9);				// [1, 12]

	outYear = year + (month <= 2 ? 1 : 0);
	outMonth = month;
	outDay = day;
}


//! Floor division, so pre-epoch timestamps land on the correct day.
int64_t
FloorDiv(int64_t value, int64_t divisor)
{
	int64_t quotient = value / divisor;
	if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
		quotient--;
	return quotient;
}


bool
ReadDigits(const std::string& text, size_t& offset, int count, int& outValue)
{
	if (offset + (size_t)count > text.size())
		return false;
	int value = 0;
	for (int i = 0; i < count; i++) {
		char c = text[offset + i];
		if (c < '0' || c > '9')
			return false;
		value = value * 10 + (c - '0');
	}
	offset += count;
	outValue = value;
	return true;
}


bool
IsValidCivil(int year, int month, int day)
{
	if (month < 1 || month > 12 || day < 1 || day > 31)
		return false;
	static const int kDaysInMonth[13]
		= { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int limit = kDaysInMonth[month];
	if (month == 2) {
		bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
		if (leap)
			limit = 29;
	}
	return day <= limit;
}

} // unnamed namespace


Timestamp
Now()
{
	return (Timestamp)::time(NULL);
}


Timestamp
StartOfDay(Timestamp value)
{
	return FloorDiv(value, kSecondsPerDay) * kSecondsPerDay;
}


Timestamp
Today()
{
	return StartOfDay(Now());
}


bool
Parse(const std::string& text, Timestamp& outValue)
{
	std::string trimmed = Trim(text);
	if (trimmed.size() != 10)
		return false;
	size_t offset = 0;
	int year = 0;
	int month = 0;
	int day = 0;
	if (!ReadDigits(trimmed, offset, 4, year))
		return false;
	if (trimmed[offset++] != '-')
		return false;
	if (!ReadDigits(trimmed, offset, 2, month))
		return false;
	if (trimmed[offset++] != '-')
		return false;
	if (!ReadDigits(trimmed, offset, 2, day))
		return false;
	if (!IsValidCivil(year, month, day))
		return false;

	outValue = DaysFromCivil(year, (unsigned)month, (unsigned)day)
		* kSecondsPerDay;
	return true;
}


std::string
ToString(Timestamp value)
{
	int64_t year = 0;
	unsigned month = 0;
	unsigned day = 0;
	CivilFromDays(FloorDiv(value, kSecondsPerDay), year, month, day);

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02u", (long long)year, month,
		day);
	return std::string(buffer);
}


bool
ParseISO8601(const std::string& text, Timestamp& outValue)
{
	std::string trimmed = Trim(text);
	size_t offset = 0;
	int year = 0;
	int month = 0;
	int day = 0;
	if (!ReadDigits(trimmed, offset, 4, year))
		return false;
	if (offset >= trimmed.size() || trimmed[offset++] != '-')
		return false;
	if (!ReadDigits(trimmed, offset, 2, month))
		return false;
	if (offset >= trimmed.size() || trimmed[offset++] != '-')
		return false;
	if (!ReadDigits(trimmed, offset, 2, day))
		return false;
	if (!IsValidCivil(year, month, day))
		return false;

	int hour = 0;
	int minute = 0;
	int second = 0;
	if (offset < trimmed.size()
		&& (trimmed[offset] == 'T' || trimmed[offset] == ' ')) {
		offset++;
		if (!ReadDigits(trimmed, offset, 2, hour))
			return false;
		if (offset >= trimmed.size() || trimmed[offset++] != ':')
			return false;
		if (!ReadDigits(trimmed, offset, 2, minute))
			return false;
		if (offset < trimmed.size() && trimmed[offset] == ':') {
			offset++;
			if (!ReadDigits(trimmed, offset, 2, second))
				return false;
		}
		// A fractional part is dropped: the format carries whole seconds.
		if (offset < trimmed.size() && trimmed[offset] == '.') {
			offset++;
			while (offset < trimmed.size() && trimmed[offset] >= '0'
				&& trimmed[offset] <= '9') {
				offset++;
			}
		}
	}
	if (hour > 23 || minute > 59 || second > 60)
		return false;

	int64_t offsetSeconds = 0;
	if (offset < trimmed.size()) {
		char sign = trimmed[offset];
		if (sign == 'Z' || sign == 'z') {
			offset++;
		} else if (sign == '+' || sign == '-') {
			offset++;
			int offsetHour = 0;
			int offsetMinute = 0;
			if (!ReadDigits(trimmed, offset, 2, offsetHour))
				return false;
			if (offset < trimmed.size() && trimmed[offset] == ':')
				offset++;
			if (offset < trimmed.size()
				&& !ReadDigits(trimmed, offset, 2, offsetMinute)) {
				return false;
			}
			offsetSeconds = (int64_t)offsetHour * 3600 + offsetMinute * 60;
			if (sign == '+')
				offsetSeconds = -offsetSeconds;
		} else {
			return false;
		}
	}
	if (offset != trimmed.size())
		return false;

	outValue = DaysFromCivil(year, (unsigned)month, (unsigned)day)
			* kSecondsPerDay
		+ (int64_t)hour * 3600 + (int64_t)minute * 60 + second + offsetSeconds;
	return true;
}


std::string
ToISO8601(Timestamp value)
{
	int64_t days = FloorDiv(value, kSecondsPerDay);
	int64_t secondsInDay = value - days * kSecondsPerDay;

	int64_t year = 0;
	unsigned month = 0;
	unsigned day = 0;
	CivilFromDays(days, year, month, day);

	char buffer[48];
	snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02uT%02d:%02d:%02dZ",
		(long long)year, month, day, (int)(secondsInDay / 3600),
		(int)((secondsInDay / 60) % 60), (int)(secondsInDay % 60));
	return std::string(buffer);
}

} // namespace IssueDate
} // namespace issueskit
