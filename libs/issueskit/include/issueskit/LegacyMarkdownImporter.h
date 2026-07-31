/*
 * LegacyMarkdownImporter.h -- one-way importer for the legacy markdown format.
 *
 * Markdown was once the source of truth; it is now an export target, and this is
 * the migration path for files written before the .issues JSON format existed.
 * It also reads back everything IssuesMarkdownSerializer emits.
 *
 * Two layouts are accepted:
 *   - Standard: has a "## Open" or "## Known Issues" section; entries are
 *     "### #NNN - Title" and an entry with no number is SKIPPED.
 *   - Free-form: uses category headings ("## Bugs", "## Enhancements") which set
 *     the type of the entries beneath them; entries need no number (they
 *     auto-increment) and "~~Title~~ ✓ Fixed" marks them resolved.
 *
 * Importing is deliberately tolerant: missing metadata falls back to the enum
 * defaults, a malformed entry never aborts the whole import, and body content
 * under an unrecognized **Header** is appended to notes rather than dropped.
 * Every imported issue gets a fresh uuid and createdAt/updatedAt of the import
 * time, because markdown records neither.
 */
#ifndef ISSUESKIT_LEGACY_MARKDOWN_IMPORTER_H
#define ISSUESKIT_LEGACY_MARKDOWN_IMPORTER_H

#include <string>
#include <vector>

#include <issueskit/IssueModel.h>
#include <issueskit/IssuesError.h>

namespace issueskit {

class LegacyMarkdownImporter {
public:
	/*!	Imports a legacy markdown document.

		\return false with IssuesError::MissingOpenSection when the document has
			no level-2 heading at all and so matches neither layout.
	*/
	static	bool				Import(const std::string& markdown,
									IssuesDocumentModel& outModel,
									IssuesError& outError);

private:
			struct Heading {
				bool		hasNumber;
				int			number;
				std::string	title;
				bool		isResolved;
			};

			enum SectionKind {
				kSectionNone,
				kSectionDescription,
				kSectionSteps,
				kSectionEnvironment,
				kSectionNotes,
				kSectionResolution,
				kSectionComments,
				kSectionUnknown
			};

	static	bool				_IsLevel2Heading(const std::string& line);
	static	bool				_IsLevel2Heading(const std::string& line,
									const char* name);
	static	bool				_IsHorizontalRule(const std::string& line);
	static	bool				_ParseHeading(const std::string& line,
									Heading& outHeading);
	static	bool				_ParseMetadata(const std::string& line,
									std::string& outKey,
									std::string& outValue);
	static	bool				_ParseSectionHeader(const std::string& line,
									std::string& outHeader);
	static	SectionKind			_Classify(const std::string& header);
	static	IssueType			_SectionType(const std::string& line);

	static	std::vector<Issue>	_CollectIssues(
									const std::vector<std::string>& lines,
									size_t start, bool freeForm, Timestamp now);
	static	Issue				_MakeIssue(int number,
									const std::string& title,
									const std::vector<std::string>& body,
									IssueType defaultType, Timestamp now);
	static	void				_ApplyMetadata(const std::string& key,
									const std::string& value, Issue& issue);

	static	std::vector<std::string> _TrimBlankEdges(
									const std::vector<std::string>& lines);
	static	std::string			_JoinBody(
									const std::vector<std::string>& lines);
	static	std::vector<std::string> _ParseSteps(
									const std::vector<std::string>& lines);
	static	std::vector<Comment> _ParseComments(
									const std::vector<std::string>& lines);
	static	bool				_ParseCommentBullet(const std::string& line,
									Comment& outComment);
};

} // namespace issueskit

#endif // ISSUESKIT_LEGACY_MARKDOWN_IMPORTER_H
