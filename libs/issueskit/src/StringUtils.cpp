/*
 * StringUtils.cpp
 */
#include <issueskit/StringUtils.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace issueskit {

std::string
Trim(const std::string& text)
{
	size_t start = 0;
	while (start < text.size()
		&& (text[start] == ' ' || text[start] == '\t')) {
		start++;
	}
	size_t end = text.size();
	while (end > start
		&& (text[end - 1] == ' ' || text[end - 1] == '\t')) {
		end--;
	}
	return text.substr(start, end - start);
}


std::vector<std::string>
Split(const std::string& text, char separator)
{
	std::vector<std::string> parts;
	std::string current;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == separator) {
			parts.push_back(current);
			current.clear();
		} else {
			current += text[i];
		}
	}
	parts.push_back(current);
	return parts;
}


std::vector<std::string>
SplitLines(const std::string& text)
{
	return Split(text, '\n');
}


std::vector<std::string>
SplitTrimNonEmpty(const std::string& text, char separator)
{
	std::vector<std::string> result;
	std::vector<std::string> raw = Split(text, separator);
	for (size_t i = 0; i < raw.size(); i++) {
		std::string entry = Trim(raw[i]);
		if (!entry.empty())
			result.push_back(entry);
	}
	return result;
}


std::string
Join(const std::vector<std::string>& parts, const std::string& separator)
{
	std::string result;
	for (size_t i = 0; i < parts.size(); i++) {
		if (i > 0)
			result += separator;
		result += parts[i];
	}
	return result;
}


bool
StartsWith(const std::string& text, const std::string& prefix)
{
	return text.size() >= prefix.size()
		&& text.compare(0, prefix.size(), prefix) == 0;
}


bool
HasPrefixAt(const std::string& text, size_t offset, const std::string& prefix)
{
	return text.size() >= offset + prefix.size()
		&& text.compare(offset, prefix.size(), prefix) == 0;
}


std::string
ToLower(const std::string& text)
{
	std::string result = text;
	for (size_t i = 0; i < result.size(); i++) {
		char c = result[i];
		if (c >= 'A' && c <= 'Z')
			result[i] = (char)(c - 'A' + 'a');
	}
	return result;
}


std::string
FormatIssueNumber(int number)
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%03d", number);
	return std::string(buffer);
}


std::string
FormatDouble(double value)
{
	// A whole number loses its decimal point so "3" survives an export/import
	// cycle, matching IssuesMarkdownSerializer.format(estimate:) and the
	// `"estimate" : 5` seen in the real .issues samples.
	if (value == (double)(long long)value && value < 1e15 && value > -1e15) {
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
		return std::string(buffer);
	}

	// Shortest representation that reads back identically.
	for (int precision = 15; precision <= 17; precision++) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
		if (strtod(buffer, NULL) == value)
			return std::string(buffer);
	}
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.17g", value);
	return std::string(buffer);
}


bool
ParseDouble(const std::string& text, double& outValue)
{
	if (text.empty())
		return false;
	const char* start = text.c_str();
	char* end = NULL;
	double parsed = strtod(start, &end);
	if (end == start || *end != '\0')
		return false;
	outValue = parsed;
	return true;
}


bool
ParseInt(const std::string& text, int& outValue)
{
	if (text.empty())
		return false;
	const char* start = text.c_str();
	char* end = NULL;
	long parsed = strtol(start, &end, 10);
	if (end == start || *end != '\0')
		return false;
	outValue = (int)parsed;
	return true;
}

} // namespace issueskit
