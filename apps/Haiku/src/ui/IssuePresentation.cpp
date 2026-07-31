/*
 * IssuePresentation.cpp
 */
#include "IssuePresentation.h"

using namespace issueskit;

namespace ihaveissues {

namespace {

rgb_color
MakeColor(uint8 red, uint8 green, uint8 blue)
{
	rgb_color color;
	color.red = red;
	color.green = green;
	color.blue = blue;
	color.alpha = 255;
	return color;
}

} // unnamed namespace


const char*
IssueTypeBadge(IssueType type)
{
	switch (type) {
		case kIssueTypeBug:			return "B";
		case kIssueTypeFeature:		return "F";
		case kIssueTypeTask:		return "T";
		case kIssueTypeQuestion:	return "?";
	}
	return "T";
}


rgb_color
IssueTypeTint(IssueType type)
{
	switch (type) {
		case kIssueTypeBug:			return MakeColor(214, 80, 118);		// pink
		case kIssueTypeFeature:		return MakeColor(138, 90, 200);		// purple
		case kIssueTypeTask:		return MakeColor(48, 110, 200);		// blue
		case kIssueTypeQuestion:	return MakeColor(40, 150, 150);		// teal
	}
	return MakeColor(48, 110, 200);
}


const char*
IssuePriorityBadge(IssuePriority priority)
{
	switch (priority) {
		case kIssuePriorityLow:		return "\xE2\x86\x93";	// U+2193 down arrow
		case kIssuePriorityMedium:	return "=";
		case kIssuePriorityHigh:	return "!";
		case kIssuePriorityCritical: return "!!";
	}
	return "=";
}


rgb_color
IssuePriorityTint(IssuePriority priority)
{
	switch (priority) {
		case kIssuePriorityLow:		return MakeColor(140, 140, 140);	// grey
		case kIssuePriorityMedium:	return MakeColor(200, 165, 40);		// yellow
		case kIssuePriorityHigh:	return MakeColor(225, 130, 40);		// orange
		case kIssuePriorityCritical: return MakeColor(210, 55, 55);		// red
	}
	return MakeColor(200, 165, 40);
}

} // namespace ihaveissues
