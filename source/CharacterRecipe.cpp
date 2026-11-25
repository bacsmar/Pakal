///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterRecipe.cpp
// Purpose: A character made of parts - its JSON form (see CharacterRecipe.h)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CharacterRecipe.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>

namespace Pakal
{
	namespace
	{
		std::string resolved(const std::string& file, const std::string& baseDir)
		{
			if (file.empty() || baseDir.empty()) return file;
			const std::filesystem::path path(file);
			if (path.is_absolute()) return file;
			return (std::filesystem::path(baseDir) / path).lexically_normal().generic_string();
		}
	}

	bool CharacterRecipe::from_json(const std::string& text, const std::string& baseDir, std::string& error)
	{
		const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
		if (json.is_discarded() || !json.is_object())
		{
			error = "not a JSON object";
			return false;
		}
		*this = CharacterRecipe();
		skeleton = resolved(json.value("skeleton", std::string()), baseDir);
		const nlohmann::json parts_ = json.value("parts", nlohmann::json::array());
		if (!parts_.is_array())
		{
			error = "\"parts\" is not a list";
			return false;
		}
		for (const auto& entry : parts_)
		{
			if (!entry.is_object() || !entry.contains("file"))
			{
				error = "a part without a \"file\"";
				return false;
			}
			parts.push_back({ entry.value("slot", std::string()), resolved(entry.value("file", std::string()), baseDir),
				entry.value("mesh", std::string()) });
		}
		if (parts.empty())
		{
			error = "no parts";
			return false;
		}
		const nlohmann::json shapes_ = json.value("shapes", nlohmann::json::object());
		if (shapes_.is_object())
		{
			for (auto it = shapes_.begin(); it != shapes_.end(); ++it)
			{
				if (it.value().is_number()) shapes[it.key()] = std::clamp(it.value().get<float>(), -1.0f, 2.0f);
			}
		}
		const nlohmann::json fold = json.value("foldBones", nlohmann::json::array());
		if (fold.is_array())
		{
			for (const auto& name : fold)
			{
				if (name.is_string()) foldBones.push_back(name.get<std::string>());
			}
		}
		return true;
	}

	std::string CharacterRecipe::to_json() const
	{
		nlohmann::json json;
		if (!skeleton.empty()) json["skeleton"] = skeleton;
		json["parts"] = nlohmann::json::array();
		for (const Part& p : parts)
		{
			nlohmann::json entry = { { "file", p.file } };
			if (!p.slot.empty()) entry["slot"] = p.slot;
			if (!p.mesh.empty()) entry["mesh"] = p.mesh;
			json["parts"].push_back(entry);
		}
		if (!shapes.empty()) json["shapes"] = shapes;
		if (!foldBones.empty()) json["foldBones"] = foldBones;
		return json.dump(1, '\t');
	}

	const CharacterRecipe::Part* CharacterRecipe::part(const std::string& slot) const
	{
		for (const Part& p : parts)
		{
			if (p.slot == slot) return &p;
		}
		return nullptr;
	}

	void CharacterRecipe::set_part(const std::string& slot, const std::string& file, const std::string& mesh)
	{
		for (Part& p : parts)
		{
			if (p.slot == slot)
			{
				p.file = file;
				p.mesh = mesh;
				return;
			}
		}
		parts.push_back({ slot, file, mesh });
	}
}
