/*
 * IssuesError.h -- the error type the core reports.
 *
 * Mirrors Swift's IssuesError, including the exact user-facing wording, so every
 * port shows the same message for the same file. The two file-I/O cases at the
 * end have no Apple counterpart: SwiftUI's DocumentGroup reports read and write
 * failures itself, whereas the Haiku, GNOME and KDE ports open and save by hand.
 */
#ifndef ISSUESKIT_ISSUES_ERROR_H
#define ISSUESKIT_ISSUES_ERROR_H

#include <string>

namespace issueskit {

class IssuesError {
public:
			enum Code {
				kNone = 0,
				kMissingSchemaVersion,
				kUnsupportedSchemaVersion,
				kDecodingFailed,
				kEncodingFailed,
				kMissingOpenSection,
				kFileReadFailed,
				kFileWriteFailed
			};

								IssuesError();

	static	IssuesError			MissingSchemaVersion();
	static	IssuesError			UnsupportedSchemaVersion(int found,
									int supported);
	static	IssuesError			DecodingFailed(const std::string& detail);
	static	IssuesError			EncodingFailed(const std::string& detail);
	static	IssuesError			MissingOpenSection();
	static	IssuesError			FileReadFailed(const std::string& detail);
	static	IssuesError			FileWriteFailed(const std::string& detail);

			Code				GetCode() const { return fCode; }
			bool				IsError() const { return fCode != kNone; }

			//! The message to show the user.
			std::string			Message() const;

private:
			Code				fCode;
			std::string			fDetail;
			int					fFound;
			int					fSupported;
};

} // namespace issueskit

#endif // ISSUESKIT_ISSUES_ERROR_H
