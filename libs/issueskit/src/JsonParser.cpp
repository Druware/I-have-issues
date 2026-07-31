/*
 * JsonParser.cpp
 */
#include <issueskit/JsonParser.h>

#include <cstdio>
#include <cstdlib>

namespace issueskit {

namespace {

//! Guards against a hand-edited file with pathological nesting.
const int kMaxDepth = 128;

} // unnamed namespace


JsonParser::JsonParser(const std::string& text)
	:
	fText(text),
	fOffset(0)
{
}


bool
JsonParser::Parse(const std::string& text, JsonValue& outValue,
	std::string& outError)
{
	JsonParser parser(text);
	parser._SkipWhitespace();
	if (!parser._ParseValue(outValue, 0)) {
		outError = parser.fError;
		return false;
	}
	parser._SkipWhitespace();
	if (parser.fOffset != text.size()) {
		outError = "Trailing content after the JSON value.";
		return false;
	}
	return true;
}


bool
JsonParser::_Fail(const char* message)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s (at byte %lu)", message,
		(unsigned long)fOffset);
	fError = buffer;
	return false;
}


void
JsonParser::_SkipWhitespace()
{
	while (fOffset < fText.size()) {
		char c = fText[fOffset];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			fOffset++;
		else
			break;
	}
}


bool
JsonParser::_ParseLiteral(const char* literal)
{
	size_t length = 0;
	while (literal[length] != '\0')
		length++;
	if (fText.compare(fOffset, length, literal) != 0)
		return _Fail("Unexpected token.");
	fOffset += length;
	return true;
}


bool
JsonParser::_ParseValue(JsonValue& outValue, int depth)
{
	if (depth > kMaxDepth)
		return _Fail("JSON nesting is too deep.");
	if (fOffset >= fText.size())
		return _Fail("Unexpected end of document.");

	switch (fText[fOffset]) {
		case '{':
			return _ParseObject(outValue, depth);
		case '[':
			return _ParseArray(outValue, depth);
		case '"':
		{
			std::string text;
			if (!_ParseString(text))
				return false;
			outValue = JsonValue::String(text);
			return true;
		}
		case 't':
			if (!_ParseLiteral("true"))
				return false;
			outValue = JsonValue::Bool(true);
			return true;
		case 'f':
			if (!_ParseLiteral("false"))
				return false;
			outValue = JsonValue::Bool(false);
			return true;
		case 'n':
			if (!_ParseLiteral("null"))
				return false;
			outValue = JsonValue::Null();
			return true;
		default:
			return _ParseNumber(outValue);
	}
}


bool
JsonParser::_ParseObject(JsonValue& outValue, int depth)
{
	fOffset++; // '{'
	outValue = JsonValue::Object();
	_SkipWhitespace();
	if (fOffset < fText.size() && fText[fOffset] == '}') {
		fOffset++;
		return true;
	}

	while (true) {
		_SkipWhitespace();
		if (fOffset >= fText.size() || fText[fOffset] != '"')
			return _Fail("Expected an object key.");
		std::string key;
		if (!_ParseString(key))
			return false;
		_SkipWhitespace();
		if (fOffset >= fText.size() || fText[fOffset] != ':')
			return _Fail("Expected ':' after an object key.");
		fOffset++;
		_SkipWhitespace();
		JsonValue value;
		if (!_ParseValue(value, depth + 1))
			return false;
		// A duplicate key keeps the last occurrence, as JSONSerialization does.
		outValue.Set(key, value);

		_SkipWhitespace();
		if (fOffset >= fText.size())
			return _Fail("Unterminated object.");
		if (fText[fOffset] == ',') {
			fOffset++;
			continue;
		}
		if (fText[fOffset] == '}') {
			fOffset++;
			return true;
		}
		return _Fail("Expected ',' or '}' in an object.");
	}
}


bool
JsonParser::_ParseArray(JsonValue& outValue, int depth)
{
	fOffset++; // '['
	outValue = JsonValue::Array();
	_SkipWhitespace();
	if (fOffset < fText.size() && fText[fOffset] == ']') {
		fOffset++;
		return true;
	}

	while (true) {
		_SkipWhitespace();
		JsonValue value;
		if (!_ParseValue(value, depth + 1))
			return false;
		outValue.Append(value);

		_SkipWhitespace();
		if (fOffset >= fText.size())
			return _Fail("Unterminated array.");
		if (fText[fOffset] == ',') {
			fOffset++;
			continue;
		}
		if (fText[fOffset] == ']') {
			fOffset++;
			return true;
		}
		return _Fail("Expected ',' or ']' in an array.");
	}
}


bool
JsonParser::_ParseHex4(uint32_t& outValue)
{
	if (fOffset + 4 > fText.size())
		return _Fail("Truncated \\u escape.");
	uint32_t value = 0;
	for (int i = 0; i < 4; i++) {
		char c = fText[fOffset + i];
		int digit;
		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			return _Fail("Invalid \\u escape.");
		value = (value << 4) | (uint32_t)digit;
	}
	fOffset += 4;
	outValue = value;
	return true;
}


bool
JsonParser::_AppendUtf8(std::string& output, uint32_t codePoint)
{
	if (codePoint < 0x80) {
		output += (char)codePoint;
	} else if (codePoint < 0x800) {
		output += (char)(0xc0 | (codePoint >> 6));
		output += (char)(0x80 | (codePoint & 0x3f));
	} else if (codePoint < 0x10000) {
		output += (char)(0xe0 | (codePoint >> 12));
		output += (char)(0x80 | ((codePoint >> 6) & 0x3f));
		output += (char)(0x80 | (codePoint & 0x3f));
	} else if (codePoint <= 0x10ffff) {
		output += (char)(0xf0 | (codePoint >> 18));
		output += (char)(0x80 | ((codePoint >> 12) & 0x3f));
		output += (char)(0x80 | ((codePoint >> 6) & 0x3f));
		output += (char)(0x80 | (codePoint & 0x3f));
	} else {
		return _Fail("Invalid Unicode code point.");
	}
	return true;
}


bool
JsonParser::_ParseString(std::string& outValue)
{
	fOffset++; // opening quote
	outValue.clear();

	while (true) {
		if (fOffset >= fText.size())
			return _Fail("Unterminated string.");
		unsigned char c = (unsigned char)fText[fOffset];
		if (c == '"') {
			fOffset++;
			return true;
		}
		if (c != '\\') {
			if (c < 0x20)
				return _Fail("Unescaped control character in a string.");
			outValue += (char)c;
			fOffset++;
			continue;
		}

		fOffset++; // backslash
		if (fOffset >= fText.size())
			return _Fail("Unterminated escape sequence.");
		char escape = fText[fOffset++];
		switch (escape) {
			case '"': outValue += '"'; break;
			case '\\': outValue += '\\'; break;
			case '/': outValue += '/'; break;
			case 'b': outValue += '\b'; break;
			case 'f': outValue += '\f'; break;
			case 'n': outValue += '\n'; break;
			case 'r': outValue += '\r'; break;
			case 't': outValue += '\t'; break;
			case 'u':
			{
				uint32_t codePoint = 0;
				if (!_ParseHex4(codePoint))
					return false;
				if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
					// High surrogate: a matching low surrogate must follow.
					if (fOffset + 1 < fText.size() && fText[fOffset] == '\\'
						&& fText[fOffset + 1] == 'u') {
						fOffset += 2;
						uint32_t low = 0;
						if (!_ParseHex4(low))
							return false;
						if (low < 0xdc00 || low > 0xdfff)
							return _Fail("Invalid low surrogate.");
						codePoint = 0x10000
							+ ((codePoint - 0xd800) << 10) + (low - 0xdc00);
					} else {
						return _Fail("Lone high surrogate.");
					}
				} else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
					return _Fail("Lone low surrogate.");
				}
				if (!_AppendUtf8(outValue, codePoint))
					return false;
				break;
			}
			default:
				return _Fail("Unknown escape sequence.");
		}
	}
}


bool
JsonParser::_ParseNumber(JsonValue& outValue)
{
	size_t start = fOffset;
	bool isInteger = true;

	if (fOffset < fText.size() && fText[fOffset] == '-')
		fOffset++;
	size_t digitsStart = fOffset;
	while (fOffset < fText.size() && fText[fOffset] >= '0'
		&& fText[fOffset] <= '9') {
		fOffset++;
	}
	if (fOffset == digitsStart)
		return _Fail("Expected a number.");

	if (fOffset < fText.size() && fText[fOffset] == '.') {
		isInteger = false;
		fOffset++;
		size_t fractionStart = fOffset;
		while (fOffset < fText.size() && fText[fOffset] >= '0'
			&& fText[fOffset] <= '9') {
			fOffset++;
		}
		if (fOffset == fractionStart)
			return _Fail("Expected digits after the decimal point.");
	}

	if (fOffset < fText.size()
		&& (fText[fOffset] == 'e' || fText[fOffset] == 'E')) {
		isInteger = false;
		fOffset++;
		if (fOffset < fText.size()
			&& (fText[fOffset] == '+' || fText[fOffset] == '-')) {
			fOffset++;
		}
		size_t exponentStart = fOffset;
		while (fOffset < fText.size() && fText[fOffset] >= '0'
			&& fText[fOffset] <= '9') {
			fOffset++;
		}
		if (fOffset == exponentStart)
			return _Fail("Expected digits in the exponent.");
	}

	std::string literal = fText.substr(start, fOffset - start);
	if (isInteger) {
		outValue = JsonValue::Integer(strtoll(literal.c_str(), NULL, 10));
	} else {
		outValue = JsonValue::Number(strtod(literal.c_str(), NULL));
	}
	return true;
}

} // namespace issueskit
