#include "BuildIdentity.h"
#include "ResourceManager.h"
#include "LogMgr.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void* module, wchar_t* name, unsigned long size);
#elif defined(__linux__) && !defined(__ANDROID__)
#include <unistd.h>
#endif

namespace Pakal
{
	namespace
	{
		std::string text_of(const nlohmann::json& json, const char* key)
		{
			const auto it = json.find(key);
			if (it == json.end()) return {};
			if (it->is_string()) return it->get<std::string>();
			if (it->is_number_integer()) return std::to_string(it->get<long long>());
			return {};
		}

		int int_of(const nlohmann::json& json, const char* key)
		{
			const auto it = json.find(key);
			return it != json.end() && it->is_number_integer() ? it->get<int>() : 0;
		}

		// The folder of the running program (a desktop build writes its build.json there); empty when unknown
		std::filesystem::path executable_dir()
		{
#if defined(_WIN32)
			wchar_t name[1024];
			const unsigned long length = GetModuleFileNameW(nullptr, name, 1024);
			if (length == 0 || length >= 1024) return {};
			return std::filesystem::path(std::wstring(name, length)).parent_path();
#elif defined(__linux__) && !defined(__ANDROID__)
			char name[1024];
			const ssize_t length = readlink("/proc/self/exe", name, sizeof(name) - 1);
			if (length <= 0) return {};
			return std::filesystem::path(std::string(name, size_t(length))).parent_path();
#else
			return {};
#endif
		}
	}

	std::string BuildIdentity::label() const
	{
		if (channel.empty()) return {};
		std::string text;
		if (!edition.empty()) text += edition + " ";
		text += version.empty() ? channel : "v" + version + "-" + channel;
		if (build > 0 || !commit.empty())
		{
			text += "  " + std::to_string(build);
			if (!commit.empty()) text += "." + commit;
		}
		if (dirty) text += ".dirty";
		return text;
	}

	std::string BuildIdentity::describe() const
	{
		std::ostringstream text;
		text << "[Build] " << (channel.empty() ? std::string("production") : label());
		if (!branch.empty()) text << " (branch " << branch << ")";
		if (code > 0) text << ", version code " << code;
		if (!known) text << ", no build.json";
		return text.str();
	}

	BuildIdentity BuildIdentity::parse(const std::string& json)
	{
		BuildIdentity identity;
		identity.channel = "dev";
		const nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
		if (!root.is_object()) return identity;
		identity.known = true;
		identity.edition = text_of(root, "edition");
		identity.version = text_of(root, "version");
		// a build.json without a channel was written before channels existed: a developer's copy
		identity.channel = root.contains("channel") ? text_of(root, "channel") : "dev";
		identity.commit = text_of(root, "commit");
		identity.branch = text_of(root, "branch");
		identity.build = int_of(root, "build");
		identity.code = int_of(root, "code");
		const auto dirty = root.find("dirty");
		identity.dirty = dirty != root.end() && dirty->is_boolean() && dirty->get<bool>();
		identity.label_flag_url = text_of(root, "label_flag_url");
		return identity;
	}

	BuildIdentity BuildIdentity::load()
	{
		bool found = false;
		std::string text = ResourceMgr.read_text("Assets/build.json", &found);
		if (!found)
		{
			const std::filesystem::path dir = executable_dir();
			std::ifstream file(dir.empty() ? std::filesystem::path("build.json") : dir / "build.json", std::ios::binary);
			if (file)
			{
				std::ostringstream content;
				content << file.rdbuf();
				text = content.str();
			}
		}
		return parse(text);
	}

	bool label_hidden_by_flag(const std::string& answer, int code)
	{
		const nlohmann::json root = nlohmann::json::parse(answer, nullptr, false);
		if (!root.is_object() || code <= 0) return false;
		const auto codes = root.find("hide_label");
		if (codes == root.end() || !codes->is_array()) return false;
		for (const auto& value : *codes)
		{
			if (value.is_number_integer() && value.get<int>() == code) return true;
		}
		return false;
	}
}
