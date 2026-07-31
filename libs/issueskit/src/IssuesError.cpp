/*
 * IssuesError.cpp
 */
#include <issueskit/IssuesError.h>

#include <cstdio>

namespace issueskit {

IssuesError::IssuesError()
	:
	fCode(kNone),
	fFound(0),
	fSupported(0)
{
}


IssuesError
IssuesError::MissingSchemaVersion()
{
	IssuesError error;
	error.fCode = kMissingSchemaVersion;
	return error;
}


IssuesError
IssuesError::UnsupportedSchemaVersion(int found, int supported)
{
	IssuesError error;
	error.fCode = kUnsupportedSchemaVersion;
	error.fFound = found;
	error.fSupported = supported;
	return error;
}


IssuesError
IssuesError::DecodingFailed(const std::string& detail)
{
	IssuesError error;
	error.fCode = kDecodingFailed;
	error.fDetail = detail;
	return error;
}


IssuesError
IssuesError::EncodingFailed(const std::string& detail)
{
	IssuesError error;
	error.fCode = kEncodingFailed;
	error.fDetail = detail;
	return error;
}


IssuesError
IssuesError::MissingOpenSection()
{
	IssuesError error;
	error.fCode = kMissingOpenSection;
	return error;
}


IssuesError
IssuesError::FileReadFailed(const std::string& detail)
{
	IssuesError error;
	error.fCode = kFileReadFailed;
	error.fDetail = detail;
	return error;
}


IssuesError
IssuesError::FileWriteFailed(const std::string& detail)
{
	IssuesError error;
	error.fCode = kFileWriteFailed;
	error.fDetail = detail;
	return error;
}


std::string
IssuesError::Message() const
{
	switch (fCode) {
		case kNone:
			return std::string();

		case kMissingSchemaVersion:
			return "The file does not declare a \"schemaVersion\" and is not a "
				"valid issues document.";

		case kUnsupportedSchemaVersion:
		{
			char buffer[256];
			snprintf(buffer, sizeof(buffer),
				"This document uses issues format version %d, but this version "
				"of the app only reads up to version %d. Update the app to open "
				"it.", fFound, fSupported);
			return std::string(buffer);
		}

		case kDecodingFailed:
			return "The issues document could not be read: " + fDetail;

		case kEncodingFailed:
			return "The issues document could not be written: " + fDetail;

		case kMissingOpenSection:
			return "The markdown file does not contain an \"## Open\" section "
				"and cannot be imported.";

		case kFileReadFailed:
			return "The file could not be read: " + fDetail;

		case kFileWriteFailed:
			return "The file could not be written: " + fDetail;
	}
	return std::string();
}

} // namespace issueskit
