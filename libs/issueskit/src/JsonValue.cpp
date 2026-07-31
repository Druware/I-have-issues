/*
 * JsonValue.cpp
 */
#include <issueskit/JsonValue.h>

#include <cstdio>

#include <issueskit/StringUtils.h>

namespace issueskit {

JsonValue::JsonValue()
	:
	fType(kNull),
	fBool(false),
	fNumber(0.0),
	fIsInteger(false),
	fInteger(0)
{
}


JsonValue
JsonValue::Null()
{
	return JsonValue();
}


JsonValue
JsonValue::Bool(bool value)
{
	JsonValue result;
	result.fType = kBool;
	result.fBool = value;
	return result;
}


JsonValue
JsonValue::Integer(int64_t value)
{
	JsonValue result;
	result.fType = kNumber;
	result.fIsInteger = true;
	result.fInteger = value;
	result.fNumber = (double)value;
	return result;
}


JsonValue
JsonValue::Number(double value)
{
	JsonValue result;
	result.fType = kNumber;
	result.fIsInteger = false;
	result.fNumber = value;
	result.fInteger = (int64_t)value;
	return result;
}


JsonValue
JsonValue::String(const std::string& value)
{
	JsonValue result;
	result.fType = kString;
	result.fString = value;
	return result;
}


JsonValue
JsonValue::Array()
{
	JsonValue result;
	result.fType = kArray;
	return result;
}


JsonValue
JsonValue::Object()
{
	JsonValue result;
	result.fType = kObject;
	return result;
}


int64_t
JsonValue::IntegerValue() const
{
	return fIsInteger ? fInteger : (int64_t)fNumber;
}


void
JsonValue::Set(const std::string& key, const JsonValue& value)
{
	fType = kObject;
	fObject[key] = value;
}


const JsonValue*
JsonValue::Find(const std::string& key) const
{
	std::map<std::string, JsonValue>::const_iterator it = fObject.find(key);
	if (it == fObject.end())
		return NULL;
	return &it->second;
}


bool
JsonValue::HasKey(const std::string& key) const
{
	return fObject.find(key) != fObject.end();
}


void
JsonValue::Append(const JsonValue& value)
{
	fType = kArray;
	fArray.push_back(value);
}


std::string
JsonValue::Write() const
{
	std::string output;
	_Write(output, 0);
	return output;
}


void
JsonValue::_WriteIndent(std::string& output, int indent)
{
	output.append((size_t)indent, ' ');
}


void
JsonValue::_WriteString(std::string& output, const std::string& value)
{
	output += '"';
	for (size_t i = 0; i < value.size(); i++) {
		unsigned char c = (unsigned char)value[i];
		switch (c) {
			case '"':
				output += "\\\"";
				break;
			case '\\':
				output += "\\\\";
				break;
			case '\b':
				output += "\\b";
				break;
			case '\f':
				output += "\\f";
				break;
			case '\n':
				output += "\\n";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\t':
				output += "\\t";
				break;
			default:
				if (c < 0x20) {
					char escape[8];
					snprintf(escape, sizeof(escape), "\\u%04x", c);
					output += escape;
				} else {
					// '/' is deliberately NOT escaped, and bytes >= 0x20 are
					// emitted verbatim so UTF-8 text (em dashes, accents) stays
					// readable in the file and in git diffs.
					output += (char)c;
				}
				break;
		}
	}
	output += '"';
}


void
JsonValue::_Write(std::string& output, int indent) const
{
	switch (fType) {
		case kNull:
			output += "null";
			break;

		case kBool:
			output += fBool ? "true" : "false";
			break;

		case kNumber:
			if (fIsInteger) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "%lld", (long long)fInteger);
				output += buffer;
			} else {
				output += FormatDouble(fNumber);
			}
			break;

		case kString:
			_WriteString(output, fString);
			break;

		case kArray:
		{
			output += "[\n";
			for (size_t i = 0; i < fArray.size(); i++) {
				if (i > 0)
					output += ",\n";
				_WriteIndent(output, indent + 2);
				fArray[i]._Write(output, indent + 2);
			}
			// An empty array still emits one bare newline, so it renders as
			// "[", a blank line, then "]" -- exactly what Foundation writes.
			output += '\n';
			_WriteIndent(output, indent);
			output += ']';
			break;
		}

		case kObject:
		{
			output += "{\n";
			std::map<std::string, JsonValue>::const_iterator it;
			bool first = true;
			for (it = fObject.begin(); it != fObject.end(); ++it) {
				if (!first)
					output += ",\n";
				first = false;
				_WriteIndent(output, indent + 2);
				_WriteString(output, it->first);
				output += " : ";
				it->second._Write(output, indent + 2);
			}
			output += '\n';
			_WriteIndent(output, indent);
			output += '}';
			break;
		}
	}
}

} // namespace issueskit
