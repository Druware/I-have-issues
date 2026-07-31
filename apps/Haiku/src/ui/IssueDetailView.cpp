/*
 * IssueDetailView.cpp
 */
#include "IssueDetailView.h"

#include <cstdio>

#include <Font.h>
#include <InterfaceDefs.h>

#include <issueskit/IssueDate.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

IssueDetailView::IssueDetailView(const char* name)
	:
	BTextView(name)
{
	MakeEditable(false);
	MakeSelectable(true);
	SetStylable(true);
	SetWordWrap(true);
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
	SetInsets(8.0f, 8.0f, 8.0f, 8.0f);
	Clear();
}


IssueDetailView::~IssueDetailView()
{
}


void
IssueDetailView::Clear()
{
	fText.clear();
	fBoldRanges.clear();
	_AppendHeading("No Issue Selected");
	_AppendPlain("Select an issue from the list, or add a new one.\n");
	_Apply();
}


void
IssueDetailView::SetIssue(const Issue& issue, const IssuesDocumentModel& model)
{
	fText.clear();
	fBoldRanges.clear();
	_Rebuild(issue, model);
	_Apply();
	ScrollToOffset(0);
}


// #pragma mark - Building

void
IssueDetailView::_AppendHeading(const char* text)
{
	if (!fText.empty())
		fText += "\n";
	int32 start = (int32)fText.size();
	fText += text;
	fBoldRanges.push_back(start);
	fBoldRanges.push_back((int32)fText.size());
	fText += "\n\n";
}


void
IssueDetailView::_AppendLabelled(const char* label, const std::string& value)
{
	int32 start = (int32)fText.size();
	fText += label;
	fText += ":";
	fBoldRanges.push_back(start);
	fBoldRanges.push_back((int32)fText.size());
	fText += " ";
	fText += value;
	fText += "\n";
}


void
IssueDetailView::_AppendPlain(const std::string& text)
{
	fText += text;
}


void
IssueDetailView::_Rebuild(const Issue& issue, const IssuesDocumentModel& model)
{
	// Header: "#NNN - Title", then the metadata rows.
	std::string title = issue.title.empty() ? std::string("Untitled")
		: issue.title;
	_AppendHeading((issue.DisplayNumber() + "  " + title).c_str());

	_AppendLabelled("Type", IssueTypeDisplayName(issue.type));
	_AppendLabelled("Priority", IssuePriorityDisplayName(issue.priority));
	_AppendLabelled("Status", IssueStatusDisplayName(issue.status));
	if (issue.resolutionKind.has_value()) {
		_AppendLabelled("Resolution",
			ResolutionKindDisplayName(*issue.resolutionKind));
	}
	_AppendLabelled("Reported", IssueDate::ToString(issue.reported));
	if (!issue.reportedBy.empty())
		_AppendLabelled("Reported by", issue.reportedBy);
	if (!issue.area.empty())
		_AppendLabelled("Area", issue.area);
	if (!issue.labels.empty())
		_AppendLabelled("Labels", Join(issue.labels, ", "));
	if (!issue.assignees.empty())
		_AppendLabelled("Assignees", Join(issue.assignees, ", "));
	if (issue.milestone.has_value() && !issue.milestone->empty())
		_AppendLabelled("Milestone", *issue.milestone);
	if (issue.estimate.has_value())
		_AppendLabelled("Estimate", FormatDouble(*issue.estimate));

	// Free-text sections, each omitted entirely when empty.
	if (!issue.description.empty()) {
		_AppendHeading("Description");
		_AppendPlain(issue.description + "\n");
	}

	if (!issue.stepsToReproduce.empty()) {
		_AppendHeading("Steps to Reproduce");
		for (size_t i = 0; i < issue.stepsToReproduce.size(); i++) {
			char prefix[32];
			snprintf(prefix, sizeof(prefix), "%lu. ", (unsigned long)(i + 1));
			_AppendPlain(std::string(prefix) + issue.stepsToReproduce[i] + "\n");
		}
	}

	if (!issue.environment.empty()) {
		_AppendHeading("Environment");
		_AppendPlain(issue.environment + "\n");
	}

	if (!issue.notes.empty()) {
		_AppendHeading("Notes / Investigation");
		_AppendPlain(issue.notes + "\n");
	}

	if (!issue.resolution.empty()) {
		_AppendHeading("Resolution");
		_AppendPlain(issue.resolution + "\n");
	}

	if (!issue.comments.empty()) {
		_AppendHeading("Comments");
		for (size_t i = 0; i < issue.comments.size(); i++) {
			const Comment& comment = issue.comments[i];
			int32 start = (int32)fText.size();
			fText += comment.author.empty() ? "Unknown" : comment.author;
			fBoldRanges.push_back(start);
			fBoldRanges.push_back((int32)fText.size());
			fText += " (" + IssueDate::ToString(comment.createdAt) + ")\n";
			fText += comment.body + "\n\n";
		}
	}

	if (!issue.relations.empty()) {
		_AppendHeading("Related Issues");
		for (size_t i = 0; i < issue.relations.size(); i++) {
			const Relation& relation = issue.relations[i];
			// A relation pointing at a deleted issue is shown literally as
			// "Missing issue", never repaired and never reported as an error --
			// the same silent-dangling behaviour the Apple app has.
			const Issue* target = model.IssueWithID(relation.issueID);
			std::string description = target != NULL
				? target->DisplayNumber() + " " + (target->title.empty()
					? std::string("Untitled") : target->title)
				: std::string("Missing issue");
			_AppendPlain(std::string(RelationKindDisplayName(relation.kind))
				+ ": " + description + "\n");
		}
	}

	if (!issue.remoteLinks.empty()) {
		_AppendHeading("Remote Links");
		for (size_t i = 0; i < issue.remoteLinks.size(); i++) {
			const RemoteLink& link = issue.remoteLinks[i];
			std::string line = link.provider.DisplayName() + " "
				+ link.identifier;
			if (link.url.has_value() && !link.url->empty())
				line += "  " + *link.url;
			_AppendPlain(line + "\n");
		}
	}
}


void
IssueDetailView::_Apply()
{
	SetText(fText.c_str());

	BFont font;
	GetFont(&font);
	BFont boldFont(font);
	boldFont.SetFace(B_BOLD_FACE);

	// fBoldRanges holds [start, end) pairs appended in document order.
	for (size_t i = 0; i + 1 < fBoldRanges.size(); i += 2) {
		SetFontAndColor(fBoldRanges[i], fBoldRanges[i + 1], &boldFont,
			B_FONT_FAMILY_AND_STYLE);
	}
}

} // namespace ihaveissues
