/*
 * Uuid.cpp
 */
#include <issueskit/Uuid.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <chrono>
#include <mutex>
#include <random>

namespace issueskit {

namespace {

/*!	Draws random bytes from a refillable pool.

	/dev/urandom is the entropy source, read in 4 KiB blocks rather than once per
	UUID: decoding a document constructs one Issue per entry and each one wants a
	fresh uuid, so a per-call open() costs a syscall pair per issue. Measured on
	the reference build, per-call opening dominated decode time for large
	documents.

	The pool is shared between the window threads and the GitHub sync worker, so
	it is guarded by a mutex.

	If /dev/urandom is unavailable the fallback is a std::mt19937_64 seeded from
	the steady clock, the wall clock and an address. A UUID that is merely
	unlikely to collide is far better than a save that aborts.
*/
void
RandomBytes(unsigned char* buffer, size_t length)
{
	static const size_t kPoolSize = 4096;

	static std::mutex sMutex;
	static unsigned char sPool[kPoolSize];
	static size_t sAvailable = 0;

	std::lock_guard<std::mutex> lock(sMutex);

	size_t produced = 0;
	while (produced < length) {
		if (sAvailable == 0) {
			FILE* source = fopen("/dev/urandom", "rb");
			if (source != NULL) {
				sAvailable = fread(sPool, 1, kPoolSize, source);
				fclose(source);
			}
			if (sAvailable == 0) {
				// No entropy device: finish from the seeded generator.
				static std::mt19937_64 generator(
					(uint64_t)std::chrono::steady_clock::now()
							.time_since_epoch().count()
					^ (uint64_t)(uintptr_t)buffer ^ (uint64_t)::time(NULL));
				while (produced < length)
					buffer[produced++] = (unsigned char)(generator() & 0xff);
				return;
			}
		}

		size_t take = length - produced;
		if (take > sAvailable)
			take = sAvailable;
		memcpy(buffer + produced, sPool + sAvailable - take, take);
		sAvailable -= take;
		produced += take;
	}
}


int
HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

} // unnamed namespace


std::string
GenerateUuid()
{
	unsigned char bytes[16];
	RandomBytes(bytes, sizeof(bytes));

	// RFC 4122 version 4, variant 1.
	bytes[6] = (unsigned char)((bytes[6] & 0x0f) | 0x40);
	bytes[8] = (unsigned char)((bytes[8] & 0x3f) | 0x80);

	char text[37];
	snprintf(text, sizeof(text),
		"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
		bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
		bytes[13], bytes[14], bytes[15]);
	return std::string(text);
}


bool
IsValidUuid(const std::string& text)
{
	if (text.size() != 36)
		return false;
	static const int kHyphens[4] = { 8, 13, 18, 23 };
	for (int i = 0; i < 4; i++) {
		if (text[kHyphens[i]] != '-')
			return false;
	}
	for (size_t i = 0; i < text.size(); i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23)
			continue;
		if (HexValue(text[i]) < 0)
			return false;
	}
	return true;
}


std::string
NormalizeUuid(const std::string& text)
{
	if (!IsValidUuid(text))
		return std::string();
	std::string result = text;
	for (size_t i = 0; i < result.size(); i++) {
		char c = result[i];
		if (c >= 'a' && c <= 'f')
			result[i] = (char)(c - 'a' + 'A');
	}
	return result;
}

} // namespace issueskit
