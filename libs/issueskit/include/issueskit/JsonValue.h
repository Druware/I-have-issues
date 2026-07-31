/*
 * JsonValue.h -- a minimal JSON tree plus the byte-exact .issues writer.
 *
 * The .issues file is shared between every client -- Apple, Android, Haiku,
 * GNOME, KDE -- and is committed to git, so a differently formatted save would
 * rewrite the whole file and destroy its diffs. Foundation's JSONEncoder with
 * [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes] produces:
 *
 *   - two spaces of indent per nesting level;
 *   - " : " between a key and its value (a space BEFORE the colon as well);
 *   - keys sorted alphabetically at every nesting level;
 *   - '/' left unescaped;
 *   - an empty array or object rendered as the opening bracket, a completely
 *     empty line, then the closing bracket at the PARENT's indent, e.g.
 *
 *         "relations" : [
 *
 *         ],
 *
 *   - a trailing newline at the end of the file.
 *
 * Write() below reproduces exactly that. Haiku's own BJson is deliberately not
 * used: its output format is not controllable.
 */
#ifndef ISSUESKIT_JSON_VALUE_H
#define ISSUESKIT_JSON_VALUE_H

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace issueskit {

class JsonValue {
public:
			enum Type {
				kNull,
				kBool,
				kNumber,
				kString,
				kArray,
				kObject
			};

								JsonValue();
	static	JsonValue			Null();
	static	JsonValue			Bool(bool value);
	static	JsonValue			Integer(int64_t value);
	static	JsonValue			Number(double value);
	static	JsonValue			String(const std::string& value);
	static	JsonValue			Array();
	static	JsonValue			Object();

			Type				GetType() const { return fType; }
			bool				IsNull() const { return fType == kNull; }
			bool				IsBool() const { return fType == kBool; }
			bool				IsNumber() const { return fType == kNumber; }
			bool				IsString() const { return fType == kString; }
			bool				IsArray() const { return fType == kArray; }
			bool				IsObject() const { return fType == kObject; }

			bool				BoolValue() const { return fBool; }
			double				DoubleValue() const { return fNumber; }
			int64_t				IntegerValue() const;
			bool				IsIntegerLiteral() const { return fIsInteger; }
			const std::string&	StringValue() const { return fString; }

	// Object access. Keys are held in a std::map, so they are already stored in
	// the byte order the writer must emit them in.
			void				Set(const std::string& key,
									const JsonValue& value);
			const JsonValue*	Find(const std::string& key) const;
			bool				HasKey(const std::string& key) const;
			const std::map<std::string, JsonValue>& Members() const
									{ return fObject; }

	// Array access.
			void				Append(const JsonValue& value);
			size_t				CountItems() const { return fArray.size(); }
			const JsonValue&	ItemAt(size_t index) const
									{ return fArray[index]; }
			const std::vector<JsonValue>& Items() const { return fArray; }

	/*!	Renders the value as a complete .issues document body.

		The result carries no trailing newline; IssuesJsonCoder appends it.
	*/
			std::string			Write() const;

private:
			void				_Write(std::string& output, int indent) const;
	static	void				_WriteString(std::string& output,
									const std::string& value);
	static	void				_WriteIndent(std::string& output, int indent);

			Type				fType;
			bool				fBool;
			double				fNumber;
			bool				fIsInteger;
			int64_t				fInteger;
			std::string			fString;
			std::vector<JsonValue> fArray;
			std::map<std::string, JsonValue> fObject;
};

} // namespace issueskit

#endif // ISSUESKIT_JSON_VALUE_H
