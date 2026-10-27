#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>

namespace Pakal
{
	// Simple JSON value type - no external dependencies
	class JsonValue
	{
	public:
		enum class Type { Null, Bool, Number, String, Object, Array };

		JsonValue() : m_type(Type::Null) {}
		JsonValue(bool value) : m_type(Type::Bool), m_bool(value) {}
		JsonValue(double value) : m_type(Type::Number), m_number(value) {}
		JsonValue(int value) : m_type(Type::Number), m_number(static_cast<double>(value)) {}
		JsonValue(float value) : m_type(Type::Number), m_number(static_cast<double>(value)) {}
		JsonValue(const std::string& value) : m_type(Type::String), m_string(value) {}
		JsonValue(const char* value) : m_type(Type::String), m_string(value) {}

		Type get_type() const { return m_type; }
		
		bool is_null() const { return m_type == Type::Null; }
		bool is_bool() const { return m_type == Type::Bool; }
		bool is_number() const { return m_type == Type::Number; }
		bool is_string() const { return m_type == Type::String; }
		bool is_object() const { return m_type == Type::Object; }
		bool is_array() const { return m_type == Type::Array; }

		bool as_bool(bool defaultValue = false) const;
		double as_number(double defaultValue = 0.0) const;
		int as_int(int defaultValue = 0) const;
		float as_float(float defaultValue = 0.0f) const;
		std::string as_string(const std::string& defaultValue = "") const;

		// Object accessors
		const JsonValue& operator[](const std::string& key) const;
		bool has(const std::string& key) const;
		std::vector<std::string> keys() const;

		// Array accessors
		const JsonValue& operator[](size_t index) const;
		size_t size() const;

		// Setters for object construction
		void set_object();
		void set_array();
		void insert(const std::string& key, const JsonValue& value);
		void push_back(const JsonValue& value);

	private:
		Type m_type;
		bool m_bool = false;
		double m_number = 0.0;
		std::string m_string;
		std::unordered_map<std::string, JsonValue> m_object;
		std::vector<JsonValue> m_array;

		friend class SimpleJsonParser;
	};

	// Simple JSON parser without external dependencies
	class SimpleJsonParser
	{
	public:
		// Parse JSON string, returns root object/array
		static JsonValue parse(const std::string& json);

		// Stringify JsonValue back to JSON
		static std::string stringify(const JsonValue& value, int indent = -1);

	private:
		struct ParseContext
		{
			const std::string& json;
			size_t pos = 0;

			ParseContext(const std::string& j) : json(j) {}
			char current() const { return pos < json.size() ? json[pos] : '\0'; }
			void advance() { pos++; }
			void skip_whitespace();
		};

		static JsonValue parse_value(ParseContext& ctx);
		static JsonValue parse_object(ParseContext& ctx);
		static JsonValue parse_array(ParseContext& ctx);
		static JsonValue parse_string(ParseContext& ctx);
		static JsonValue parse_number(ParseContext& ctx);
		static JsonValue parse_bool(ParseContext& ctx);
		static JsonValue parse_null(ParseContext& ctx);

		static std::string stringify_value(const JsonValue& value, int indent, int currentIndent);
	};
}
