/*
 * Uuid.h -- UUID generation and validation for the .issues format.
 *
 * The format stores UUIDs as Foundation writes them: 36 uppercase characters,
 * 8-4-4-4-12, hyphen separated. Anything else is rejected on decode, because
 * Swift's `UUID` decoding throws on a malformed string and this app must not be
 * more permissive than the file it shares.
 */
#ifndef ISSUESKIT_UUID_H
#define ISSUESKIT_UUID_H

#include <string>

namespace issueskit {

//! A fresh random (version 4) UUID, uppercase.
std::string GenerateUuid();

//! Whether \a text is a well-formed 8-4-4-4-12 hexadecimal UUID.
bool IsValidUuid(const std::string& text);

/*!	Uppercases a well-formed UUID.

	Returns an empty string when \a text is not a UUID, so callers can treat the
	empty result as "reject this document".
*/
std::string NormalizeUuid(const std::string& text);

} // namespace issueskit

#endif // ISSUESKIT_UUID_H
