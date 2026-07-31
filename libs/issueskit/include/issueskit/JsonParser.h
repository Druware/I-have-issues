/*
 * JsonParser.h -- a hand-rolled RFC 8259 reader.
 *
 * Hand-rolled so the core depends on no platform JSON parser (each would impose
 * its own object model, and Haiku's BJson would drag in libbe) and so the number
 * handling can distinguish an integer literal from a fractional one -- the
 * writer needs that to round-trip `"number" : 1` and `"estimate" : 3.5`.
 */
#ifndef ISSUESKIT_JSON_PARSER_H
#define ISSUESKIT_JSON_PARSER_H

#include <string>

#include <issueskit/JsonValue.h>

namespace issueskit {

class JsonParser {
public:
	/*!	Parses a complete UTF-8 JSON document.

		\param text The document.
		\param outValue Receives the parsed tree on success.
		\param outError Receives a human-readable message on failure.
		\return true on success.
	*/
	static	bool				Parse(const std::string& text,
									JsonValue& outValue,
									std::string& outError);

private:
								JsonParser(const std::string& text);

			bool				_ParseValue(JsonValue& outValue, int depth);
			bool				_ParseObject(JsonValue& outValue, int depth);
			bool				_ParseArray(JsonValue& outValue, int depth);
			bool				_ParseString(std::string& outValue);
			bool				_ParseNumber(JsonValue& outValue);
			bool				_ParseLiteral(const char* literal);
			void				_SkipWhitespace();
			bool				_Fail(const char* message);
			bool				_AppendUtf8(std::string& output,
									uint32_t codePoint);
			bool				_ParseHex4(uint32_t& outValue);

			const std::string&	fText;
			size_t				fOffset;
			std::string			fError;
};

} // namespace issueskit

#endif // ISSUESKIT_JSON_PARSER_H
