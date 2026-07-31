/*
 * IssuesMarkdown.h -- markdown export.
 *
 * Markdown is an EXPORT format, not the source of truth: the .issues JSON is.
 * Fields the template has no slot for (uuid, createdAt, updatedAt, closedAt,
 * resolutionKind, relations, remoteLinks) are dropped, deliberately. The legacy
 * importer can read back everything this emits.
 */
#ifndef ISSUESKIT_ISSUES_MARKDOWN_H
#define ISSUESKIT_ISSUES_MARKDOWN_H

#include <string>

#include <issueskit/IssueModel.h>

namespace issueskit {

class IssuesMarkdownSerializer {
public:
	//! Renders a model as a complete markdown document.
	static	std::string			Export(const IssuesDocumentModel& model);

private:
	static	std::string			_SectionBody(const std::vector<Issue>& issues,
									const char* emptyText);
	static	std::string			_ExportIssue(const Issue& issue);
	static	void				_AppendMetadata(
									std::vector<std::string>& lines,
									const char* key, const std::string& value);
	static	void				_AppendTextSection(
									std::vector<std::string>& lines,
									const char* header,
									const std::string& content);
	static	void				_AppendSteps(std::vector<std::string>& lines,
									const std::vector<std::string>& steps);
	static	void				_AppendComments(std::vector<std::string>& lines,
									const std::vector<Comment>& comments);
};

} // namespace issueskit

#endif // ISSUESKIT_ISSUES_MARKDOWN_H
