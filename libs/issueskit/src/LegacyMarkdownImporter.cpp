/*
 * LegacyMarkdownImporter.cpp
 */
#include <issueskit/LegacyMarkdownImporter.h>

#include <cstdio>

#include <issueskit/StringUtils.h>

namespace issueskit {

namespace {

const char* kEmDash = "\xE2\x80\x94";		// U+2014
const char* kEnDash = "\xE2\x80\x93";		// U+2013
const char* kCheckMark = "\xE2\x9C\x93";	// U+2713

//! ASCII letter test. See the note in _ParseHeading about non-ASCII prefixes.
bool
IsAsciiLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


bool
IsAsciiDigit(char c)
{
	return c >= '0' && c <= '9';
}


/*!	Advances \a offset past one title separator, or returns false.

	The separators are space, tab, hyphen, en dash and em dash -- the same set
	the Apple parser uses, spelled out in UTF-8 bytes because this code walks a
	std::string rather than a Swift Character sequence.
*/
bool
SkipOneSeparator(const std::string& text, size_t& offset)
{
	if (offset >= text.size())
		return false;
	char c = text[offset];
	if (c == ' ' || c == '\t' || c == '-') {
		offset++;
		return true;
	}
	if (HasPrefixAt(text, offset, kEmDash) || HasPrefixAt(text, offset,
			kEnDash)) {
		offset += 3;
		return true;
	}
	return false;
}

} // unnamed namespace


bool
LegacyMarkdownImporter::Import(const std::string& markdown,
	IssuesDocumentModel& outModel, IssuesError& outError)
{
	std::vector<std::string> lines = SplitLines(markdown);
	Timestamp now = IssueDate::Now();

	for (size_t i = 0; i < lines.size(); i++) {
		if (!_IsLevel2Heading(lines[i], "Open")
			&& !_IsLevel2Heading(lines[i], "Known Issues")) {
			continue;
		}

		std::vector<std::string> preambleLines(lines.begin(),
			lines.begin() + (long)i);
		std::string preamble = Join(preambleLines, "\n");
		if (i > 0)
			preamble += "\n";

		IssuesDocumentModel model;
		model.exportSettings.preambleMarkdown = preamble;
		model.issues = _CollectIssues(lines, i + 1, false, now);
		outModel = model;
		return true;
	}

	// Fall back to free-form parsing when the file uses its own category
	// headings (## Bugs / ## Enhancements) rather than the canonical ## Open.
	bool hasAnyLevel2 = false;
	for (size_t i = 0; i < lines.size(); i++) {
		if (_IsLevel2Heading(lines[i])) {
			hasAnyLevel2 = true;
			break;
		}
	}
	if (!hasAnyLevel2) {
		outError = IssuesError::MissingOpenSection();
		return false;
	}

	IssuesDocumentModel model;
	model.exportSettings.preambleMarkdown = "";
	model.issues = _CollectIssues(lines, 0, true, now);
	outModel = model;
	return true;
}


// #pragma mark - Line classification

bool
LegacyMarkdownImporter::_IsLevel2Heading(const std::string& line)
{
	return StartsWith(Trim(line), "## ");
}


bool
LegacyMarkdownImporter::_IsLevel2Heading(const std::string& line,
	const char* name)
{
	return Trim(line) == (std::string("## ") + name);
}


bool
LegacyMarkdownImporter::_IsHorizontalRule(const std::string& line)
{
	std::string trimmed = Trim(line);
	if (trimmed.size() < 3)
		return false;
	for (size_t i = 0; i < trimmed.size(); i++) {
		if (trimmed[i] != '-')
			return false;
	}
	return true;
}


bool
LegacyMarkdownImporter::_ParseHeading(const std::string& line,
	Heading& outHeading)
{
	if (!StartsWith(line, "### "))
		return false;
	std::string body = line.substr(4);

	size_t cursor = 0;
	while (cursor < body.size() && body[cursor] == ' ')
		cursor++;
	if (cursor < body.size() && body[cursor] == '#')
		cursor++;

	// Skip an optional letter prefix such as "MLM-" before the issue number.
	//
	// Deviation from Apple: Swift's Character.isLetter is Unicode-aware, this
	// test is ASCII-only. A legacy file with a non-ASCII letter prefix would
	// parse differently -- no such file exists in the repository's fixtures.
	size_t prefixStart = cursor;
	while (cursor < body.size() && IsAsciiLetter(body[cursor]))
		cursor++;
	if (cursor < body.size() && body[cursor] == '-')
		cursor++;
	else
		cursor = prefixStart;

	size_t digitsStart = cursor;
	while (cursor < body.size() && IsAsciiDigit(body[cursor]))
		cursor++;

	outHeading.hasNumber = false;
	outHeading.number = 0;
	if (cursor > digitsStart) {
		int parsed = 0;
		if (ParseInt(body.substr(digitsStart, cursor - digitsStart), parsed)) {
			outHeading.hasNumber = true;
			outHeading.number = parsed;
		}
	}
	if (!outHeading.hasNumber) {
		// No numeric identifier: the title starts at the beginning.
		cursor = 0;
	}

	size_t titleStart = cursor;
	while (SkipOneSeparator(body, titleStart))
		;
	std::string rawTitle = Trim(body.substr(titleStart));

	// Detect ~~strikethrough~~ titles used by free-form resolved entries.
	outHeading.isResolved = false;
	if (StartsWith(rawTitle, "~~")) {
		size_t close = rawTitle.find("~~", 2);
		if (close != std::string::npos) {
			std::string inner = rawTitle.substr(2, close - 2);
			std::string suffix = Trim(rawTitle.substr(close + 2));
			std::string lowerSuffix = ToLower(suffix);
			outHeading.isResolved = StartsWith(suffix, kCheckMark)
				|| StartsWith(lowerSuffix, "fixed")
				|| StartsWith(lowerSuffix, "closed")
				|| suffix.empty();
			rawTitle = inner;
		}
	}

	outHeading.title = rawTitle;
	return true;
}


bool
LegacyMarkdownImporter::_ParseMetadata(const std::string& line,
	std::string& outKey, std::string& outValue)
{
	std::string trimmed = Trim(line);
	if (!StartsWith(trimmed, "- "))
		return false;
	std::string afterDash = trimmed.substr(2);
	if (!StartsWith(afterDash, "**"))
		return false;
	std::string afterStars = afterDash.substr(2);
	size_t close = afterStars.find(":**");
	if (close == std::string::npos)
		return false;
	outKey = afterStars.substr(0, close);
	outValue = Trim(afterStars.substr(close + 3));
	return true;
}


bool
LegacyMarkdownImporter::_ParseSectionHeader(const std::string& line,
	std::string& outHeader)
{
	std::string trimmed = Trim(line);
	if (!StartsWith(trimmed, "**"))
		return false;
	std::string afterStars = trimmed.substr(2);
	size_t close = afterStars.find("**");
	if (close == std::string::npos)
		return false;
	outHeader = afterStars.substr(0, close);
	return true;
}


LegacyMarkdownImporter::SectionKind
LegacyMarkdownImporter::_Classify(const std::string& header)
{
	std::string lower = ToLower(header);
	if (lower == "description")
		return kSectionDescription;
	if (lower == "steps to reproduce")
		return kSectionSteps;
	if (lower == "environment")
		return kSectionEnvironment;
	if (lower == "notes / investigation" || lower == "notes/investigation"
		|| lower == "notes") {
		return kSectionNotes;
	}
	if (lower == "resolution" || lower == "proposed fix")
		return kSectionResolution;
	if (lower == "comments")
		return kSectionComments;
	return kSectionUnknown;
}


IssueType
LegacyMarkdownImporter::_SectionType(const std::string& line)
{
	std::string lower = ToLower(Trim(line));
	if (StartsWith(lower, "## bug"))
		return kIssueTypeBug;
	if (StartsWith(lower, "## enhancement") || StartsWith(lower, "## feature"))
		return kIssueTypeFeature;
	if (StartsWith(lower, "## question"))
		return kIssueTypeQuestion;
	return kIssueTypeTask;
}


// #pragma mark - Issue collection

std::vector<Issue>
LegacyMarkdownImporter::_CollectIssues(const std::vector<std::string>& lines,
	size_t start, bool freeForm, Timestamp now)
{
	std::vector<Issue> issues;
	int nextAutoNumber = 1;
	IssueType currentSectionType = kIssueTypeTask;
	size_t index = start;

	while (index < lines.size()) {
		if (freeForm && _IsLevel2Heading(lines[index])) {
			currentSectionType = _SectionType(lines[index]);
			index++;
			continue;
		}

		Heading heading;
		if (!_ParseHeading(lines[index], heading)) {
			index++;
			continue;
		}
		// In standard layout an entry with no numeric id is skipped, and the
		// rest of the file still imports.
		if (!freeForm && !heading.hasNumber) {
			index++;
			continue;
		}

		int number = heading.hasNumber ? heading.number : nextAutoNumber;
		nextAutoNumber = (number > nextAutoNumber ? number : nextAutoNumber) + 1;
		index++;

		std::vector<std::string> body;
		while (index < lines.size()) {
			Heading lookahead;
			if (_ParseHeading(lines[index], lookahead)
				|| _IsLevel2Heading(lines[index])
				|| _IsHorizontalRule(lines[index])) {
				break;
			}
			body.push_back(lines[index]);
			index++;
		}

		Issue issue = _MakeIssue(number, heading.title, body,
			freeForm ? currentSectionType : kDefaultIssueType, now);
		if (freeForm && heading.isResolved)
			issue.status = kIssueStatusResolved;
		issues.push_back(issue);
	}
	return issues;
}


Issue
LegacyMarkdownImporter::_MakeIssue(int number, const std::string& title,
	const std::vector<std::string>& body, IssueType defaultType, Timestamp now)
{
	Issue issue;
	issue.number = number;
	issue.title = title;
	issue.type = defaultType;
	issue.reported = IssueDate::Today();
	issue.createdAt = now;
	issue.updatedAt = now;

	SectionKind current = kSectionNone;
	std::string currentHeader;
	std::vector<std::string> buffer;
	std::vector<std::string> extraParts;

	// Flushes `buffer` into whichever field `current` names. Content the
	// markdown format has no dedicated slot for lands in Notes rather than being
	// dropped, so an export/import/export cycle is stable.
	auto flush = [&]() {
		switch (current) {
			case kSectionNone:
			{
				std::string content = _JoinBody(buffer);
				if (!content.empty())
					extraParts.push_back(content);
				break;
			}
			case kSectionDescription:
				issue.description = _JoinBody(buffer);
				break;
			case kSectionSteps:
				issue.stepsToReproduce = _ParseSteps(buffer);
				break;
			case kSectionEnvironment:
				issue.environment = _JoinBody(buffer);
				break;
			case kSectionNotes:
				issue.notes = _JoinBody(buffer);
				break;
			case kSectionResolution:
				issue.resolution = _JoinBody(buffer);
				break;
			case kSectionComments:
				issue.comments = _ParseComments(buffer);
				break;
			case kSectionUnknown:
			{
				std::string content = _JoinBody(buffer);
				std::string marker = "**" + currentHeader + "**";
				extraParts.push_back(content.empty() ? marker
					: marker + "\n\n" + content);
				break;
			}
		}
		buffer.clear();
	};

	for (size_t i = 0; i < body.size(); i++) {
		std::string header;
		std::string key;
		std::string value;
		if (_ParseSectionHeader(body[i], header)) {
			flush();
			current = _Classify(header);
			currentHeader = header;
		} else if (current == kSectionNone
			&& _ParseMetadata(body[i], key, value)) {
			_ApplyMetadata(key, value, issue);
		} else {
			buffer.push_back(body[i]);
		}
	}
	flush();

	std::vector<std::string> extras;
	for (size_t i = 0; i < extraParts.size(); i++) {
		if (!extraParts[i].empty())
			extras.push_back(extraParts[i]);
	}
	if (!extras.empty()) {
		std::string joined = Join(extras, "\n\n");
		issue.notes = issue.notes.empty() ? joined
			: issue.notes + "\n\n" + joined;
	}
	return issue;
}


void
LegacyMarkdownImporter::_ApplyMetadata(const std::string& key,
	const std::string& value, Issue& issue)
{
	std::string lower = ToLower(key);

	if (lower == "type") {
		IssueType type;
		issue.type = IssueTypeFromDisplayName(value, type) ? type
			: kDefaultIssueType;
	} else if (lower == "priority" || lower == "severity") {
		// "Severity" is how the free-form MLM sample spells Priority.
		IssuePriority priority;
		issue.priority = IssuePriorityFromDisplayName(value, priority)
			? priority : kDefaultIssuePriority;
	} else if (lower == "status") {
		IssueStatus status;
		issue.status = IssueStatusFromDisplayName(value, status) ? status
			: kDefaultIssueStatus;
	} else if (lower == "reported") {
		// An unparsable date is silently ignored, keeping the default.
		Timestamp date = 0;
		if (IssueDate::Parse(value, date))
			issue.reported = date;
	} else if (lower == "reported by") {
		issue.reportedBy = value;
	} else if (lower == "area" || lower == "component") {
		issue.area = value;
	} else if (lower == "labels") {
		issue.labels = SplitTrimNonEmpty(value, ',');
	} else if (lower == "assignees") {
		issue.assignees = SplitTrimNonEmpty(value, ',');
	} else if (lower == "milestone") {
		if (value.empty())
			issue.milestone.reset();
		else
			issue.milestone = value;
	} else if (lower == "estimate") {
		double estimate = 0.0;
		if (ParseDouble(value, estimate))
			issue.estimate = estimate;
	} else if (lower == "github") {
		RemoteLink link;
		link.provider = RemoteProvider::GitHub();
		link.identifier = value;
		issue.remoteLinks.push_back(link);
	}
	// Anything else is ignored, exactly as in the Apple importer.
}


// #pragma mark - Value parsing

std::vector<std::string>
LegacyMarkdownImporter::_TrimBlankEdges(const std::vector<std::string>& lines)
{
	size_t first = 0;
	size_t last = lines.size();
	while (first < last && Trim(lines[first]).empty())
		first++;
	while (last > first && Trim(lines[last - 1]).empty())
		last--;
	return std::vector<std::string>(lines.begin() + (long)first,
		lines.begin() + (long)last);
}


std::string
LegacyMarkdownImporter::_JoinBody(const std::vector<std::string>& lines)
{
	return Join(_TrimBlankEdges(lines), "\n");
}


std::vector<std::string>
LegacyMarkdownImporter::_ParseSteps(const std::vector<std::string>& lines)
{
	std::vector<std::string> steps;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string trimmed = Trim(lines[i]);
		if (trimmed.empty())
			continue;

		size_t dot = trimmed.find('.');
		bool numbered = dot != std::string::npos && dot > 0;
		if (numbered) {
			for (size_t j = 0; j < dot; j++) {
				if (!IsAsciiDigit(trimmed[j])) {
					numbered = false;
					break;
				}
			}
		}

		if (numbered) {
			std::string text = Trim(trimmed.substr(dot + 1));
			if (!text.empty())
				steps.push_back(text);
		} else if (!steps.empty()) {
			// A non-numbered line continues the previous step.
			steps[steps.size() - 1] += " " + trimmed;
		}
	}
	return steps;
}


std::vector<Comment>
LegacyMarkdownImporter::_ParseComments(const std::vector<std::string>& lines)
{
	std::vector<Comment> comments;
	std::vector<std::string> trimmed = _TrimBlankEdges(lines);
	for (size_t i = 0; i < trimmed.size(); i++) {
		Comment comment;
		if (_ParseCommentBullet(trimmed[i], comment)) {
			comments.push_back(comment);
		} else if (!comments.empty()) {
			std::string continuation = StartsWith(trimmed[i], "  ")
				? trimmed[i].substr(2) : trimmed[i];
			comments[comments.size() - 1].body += "\n" + continuation;
		}
	}
	return comments;
}


bool
LegacyMarkdownImporter::_ParseCommentBullet(const std::string& line,
	Comment& outComment)
{
	if (!StartsWith(line, "- **"))
		return false;
	std::string afterBullet = line.substr(4);
	size_t closeStars = afterBullet.find("**");
	if (closeStars == std::string::npos)
		return false;

	std::string author = afterBullet.substr(0, closeStars);
	std::string remainder = Trim(afterBullet.substr(closeStars + 2));

	Timestamp createdAt = IssueDate::Today();
	if (StartsWith(remainder, "(")) {
		size_t closeParen = remainder.find(')');
		if (closeParen != std::string::npos) {
			std::string text = remainder.substr(1, closeParen - 1);
			Timestamp parsed = 0;
			if (IssueDate::Parse(text, parsed))
				createdAt = parsed;
			remainder = remainder.substr(closeParen + 1);
		}
	}
	if (StartsWith(remainder, ":"))
		remainder = remainder.substr(1);

	outComment.author = author;
	outComment.createdAt = createdAt;
	outComment.body = Trim(remainder);
	return true;
}

} // namespace issueskit
