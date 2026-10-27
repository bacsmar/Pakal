#include "SimpleJsonParser.h"
#include <cctype>
#include <cmath>
#include <sstream>

namespace Pakal
{
	// ===== JsonValue Implementation =====

	bool JsonValue::as_bool(bool defaultValue) const
	{
		return m_type == Type::Bool ? m_bool : defaultValue;
	}

	double JsonValue::as_number(double defaultValue) const
	{
		return m_type == Type::Number ? m_number : defaultValue;
	}

	int JsonValue::as_int(int defaultValue) const
	{
		return m_type == Type::Number ? static_cast<int>(m_number) : defaultValue;
	}

	float JsonValue::as_float(float defaultValue) const
	{
		return m_type == Type::Number ? static_cast<float>(m_number) : defaultValue;
	}

	std::string JsonValue::as_string(const std::string& defaultValue) const
	{
		return m_type == Type::String ? m_string : defaultValue;
	}

	const JsonValue& JsonValue::operator[](const std::string& key) const
	{
		if (m_type != Type::Object) {
			static const JsonValue nullValue;
			return nullValue;
		}
		auto it = m_object.find(key);
		if (it != m_object.end()) {
			return it->second;
		}
		static const JsonValue nullValue;
		return nullValue;
	}

	bool JsonValue::has(const std::string& key) const
	{
		if (m_type != Type::Object) return false;
		return m_object.find(key) != m_object.end();
	}

	std::vector<std::string> JsonValue::keys() const
	{
		std::vector<std::string> result;
		if (m_type == Type::Object) {
			for (const auto& pair : m_object) {
				result.push_back(pair.first);
			}
		}
		return result;
	}

	const JsonValue& JsonValue::operator[](size_t index) const
	{
		if (m_type != Type::Array || index >= m_array.size()) {
			static const JsonValue nullValue;
			return nullValue;
		}
		return m_array[index];
	}

	size_t JsonValue::size() const
	{
		if (m_type == Type::Object) return m_object.size();
		if (m_type == Type::Array) return m_array.size();
		return 0;
	}

	void JsonValue::set_object()
	{
		m_type = Type::Object;
		m_object.clear();
		m_array.clear();
	}

	void JsonValue::set_array()
	{
		m_type = Type::Array;
		m_object.clear();
		m_array.clear();
	}

	void JsonValue::insert(const std::string& key, const JsonValue& value)
	{
		if (m_type != Type::Object) set_object();
		m_object[key] = value;
	}

	void JsonValue::push_back(const JsonValue& value)
	{
		if (m_type != Type::Array) set_array();
		m_array.push_back(value);
	}

	// ===== SimpleJsonParser Implementation =====

	void SimpleJsonParser::ParseContext::skip_whitespace()
	{
		while (pos < json.size() && std::isspace(json[pos])) {
			pos++;
		}
	}

	JsonValue SimpleJsonParser::parse(const std::string& json)
	{
		ParseContext ctx(json);
		ctx.skip_whitespace();
		return parse_value(ctx);
	}

	JsonValue SimpleJsonParser::parse_value(ParseContext& ctx)
	{
		ctx.skip_whitespace();

		switch (ctx.current()) {
			case '{': return parse_object(ctx);
			case '[': return parse_array(ctx);
			case '"': return parse_string(ctx);
			case 't': return parse_bool(ctx);
			case 'f': return parse_bool(ctx);
			case 'n': return parse_null(ctx);
			case '-': 
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return parse_number(ctx);
			default:
				return JsonValue(); // null
		}
	}

	JsonValue SimpleJsonParser::parse_object(ParseContext& ctx)
	{
		JsonValue obj;
		obj.set_object();

		ctx.advance(); // skip '{'
		ctx.skip_whitespace();

		if (ctx.current() == '}') {
			ctx.advance();
			return obj;
		}

		while (true) {
			ctx.skip_whitespace();

			// Parse key
			if (ctx.current() != '"') {
				return JsonValue(); // error
			}
			std::string key = parse_string(ctx).as_string();

			ctx.skip_whitespace();
			if (ctx.current() != ':') {
				return JsonValue(); // error
			}
			ctx.advance(); // skip ':'

			// Parse value
			JsonValue value = parse_value(ctx);
			obj.insert(key, value);

			ctx.skip_whitespace();
			if (ctx.current() == '}') {
				ctx.advance();
				break;
			}
			if (ctx.current() != ',') {
				return JsonValue(); // error
			}
			ctx.advance(); // skip ','
		}

		return obj;
	}

	JsonValue SimpleJsonParser::parse_array(ParseContext& ctx)
	{
		JsonValue arr;
		arr.set_array();

		ctx.advance(); // skip '['
		ctx.skip_whitespace();

		if (ctx.current() == ']') {
			ctx.advance();
			return arr;
		}

		while (true) {
			JsonValue value = parse_value(ctx);
			arr.push_back(value);

			ctx.skip_whitespace();
			if (ctx.current() == ']') {
				ctx.advance();
				break;
			}
			if (ctx.current() != ',') {
				return JsonValue(); // error
			}
			ctx.advance(); // skip ','
		}

		return arr;
	}

	JsonValue SimpleJsonParser::parse_string(ParseContext& ctx)
	{
		std::string str;
		ctx.advance(); // skip opening '"'

		while (ctx.current() != '"' && ctx.current() != '\0') {
			if (ctx.current() == '\\') {
				ctx.advance();
				switch (ctx.current()) {
					case '"': str += '"'; break;
					case '\\': str += '\\'; break;
					case '/': str += '/'; break;
					case 'b': str += '\b'; break;
					case 'f': str += '\f'; break;
					case 'n': str += '\n'; break;
					case 'r': str += '\r'; break;
					case 't': str += '\t'; break;
					default: str += ctx.current();
				}
			} else {
				str += ctx.current();
			}
			ctx.advance();
		}

		if (ctx.current() == '"') {
			ctx.advance(); // skip closing '"'
		}

		return JsonValue(str);
	}

	JsonValue SimpleJsonParser::parse_number(ParseContext& ctx)
	{
		std::string numStr;

		if (ctx.current() == '-') {
			numStr += '-';
			ctx.advance();
		}

		while (std::isdigit(ctx.current())) {
			numStr += ctx.current();
			ctx.advance();
		}

		if (ctx.current() == '.') {
			numStr += '.';
			ctx.advance();
			while (std::isdigit(ctx.current())) {
				numStr += ctx.current();
				ctx.advance();
			}
		}

		if (ctx.current() == 'e' || ctx.current() == 'E') {
			numStr += ctx.current();
			ctx.advance();
			if (ctx.current() == '+' || ctx.current() == '-') {
				numStr += ctx.current();
				ctx.advance();
			}
			while (std::isdigit(ctx.current())) {
				numStr += ctx.current();
				ctx.advance();
			}
		}

		return JsonValue(std::stod(numStr));
	}

	JsonValue SimpleJsonParser::parse_bool(ParseContext& ctx)
	{
		if (ctx.current() == 't') {
			if (ctx.json.substr(ctx.pos, 4) == "true") {
				ctx.pos += 4;
				return JsonValue(true);
			}
		} else if (ctx.current() == 'f') {
			if (ctx.json.substr(ctx.pos, 5) == "false") {
				ctx.pos += 5;
				return JsonValue(false);
			}
		}
		return JsonValue(); // error -> null
	}

	JsonValue SimpleJsonParser::parse_null(ParseContext& ctx)
	{
		if (ctx.json.substr(ctx.pos, 4) == "null") {
			ctx.pos += 4;
			return JsonValue();
		}
		return JsonValue();
	}

	std::string SimpleJsonParser::stringify(const JsonValue& value, int indent)
	{
		return stringify_value(value, indent, 0);
	}

	std::string SimpleJsonParser::stringify_value(const JsonValue& value, int indent, int currentIndent)
	{
		switch (value.get_type()) {
			case JsonValue::Type::Null:
				return "null";

			case JsonValue::Type::Bool:
				return value.as_bool() ? "true" : "false";

			case JsonValue::Type::Number: {
				double num = value.as_number();
				if (num == static_cast<int>(num)) {
					return std::to_string(static_cast<int>(num));
				}
				std::ostringstream oss;
				oss << num;
				return oss.str();
			}

			case JsonValue::Type::String: {
				std::string str = "\"";
				for (char c : value.as_string()) {
					switch (c) {
						case '"': str += "\\\""; break;
						case '\\': str += "\\\\"; break;
						case '\b': str += "\\b"; break;
						case '\f': str += "\\f"; break;
						case '\n': str += "\\n"; break;
						case '\r': str += "\\r"; break;
						case '\t': str += "\\t"; break;
						default: str += c;
					}
				}
				str += "\"";
				return str;
			}

			case JsonValue::Type::Array: {
				std::string result = "[";
				if (indent >= 0 && value.size() > 0) {
					result += "\n";
				}
				for (size_t i = 0; i < value.size(); i++) {
					if (indent >= 0) {
						result += std::string((currentIndent + indent) * 2, ' ');
					}
					result += stringify_value(value[i], indent, currentIndent + 1);
					if (i < value.size() - 1) result += ",";
					if (indent >= 0) result += "\n";
				}
				if (indent >= 0 && value.size() > 0) {
					result += std::string(currentIndent * 2, ' ');
				}
				result += "]";
				return result;
			}

			case JsonValue::Type::Object: {
				std::string result = "{";
				if (indent >= 0 && value.size() > 0) {
					result += "\n";
				}
				auto objKeys = value.keys();
				for (size_t i = 0; i < objKeys.size(); i++) {
					if (indent >= 0) {
						result += std::string((currentIndent + indent) * 2, ' ');
					}
					result += "\"" + objKeys[i] + "\":";
					if (indent >= 0) result += " ";
					result += stringify_value(value[objKeys[i]], indent, currentIndent + 1);
					if (i < objKeys.size() - 1) result += ",";
					if (indent >= 0) result += "\n";
				}
				if (indent >= 0 && value.size() > 0) {
					result += std::string(currentIndent * 2, ' ');
				}
				result += "}";
				return result;
			}

			default:
				return "null";
		}
	}
}
