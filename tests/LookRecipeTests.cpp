// LookRecipeTests: a character's look made of parts (CharacterViewComponent::Look's "recipe" and "recipes",
// docs/MODULAR_CHARACTERS.md). The look reads a recipe inline or from a recipe file, keeps a list of them as variants,
// leaves out one that does not read, and keeps the defaults' recipes when the look names none. No GPU: what is drawn
// from them is LevelRuntime::character_model's, and the assembly is CharacterAssemblyTests'.

#include "topdown/view/CharacterViewComponent.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using Pakal::TopDown::CharacterViewComponent;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* description)
	{
		std::printf("%s %s\n", condition ? "[ OK ]" : "[FAIL]", description);
		if (!condition) ++g_failures;
	}
}

int main()
{
	namespace fs = std::filesystem;
	const fs::path dir = fs::temp_directory_path() / "pakal_look_recipe_tests";
	fs::create_directories(dir / "crowd");
	const std::string recipeFile = (dir / "crowd" / "onlooker.json").generic_string();
	{
		std::ofstream out(recipeFile);
		out << R"({ "skeleton": "rig.glb", "parts": [ { "slot": "body", "file": "body.glb" },
			{ "slot": "head", "file": "heads.glb", "mesh": "Head_02" } ], "shapes": { "heavy": 0.5 } })";
	}

	// one recipe, inline: it stands for the model, and its files are as written
	{
		const auto look = CharacterViewComponent::Look::from_json(nlohmann::json::parse(R"({ "height": 1.7,
			"recipe": { "skeleton": "parts/rig.glb", "parts": [ { "slot": "body", "file": "parts/body.glb" } ] } })"),
			CharacterViewComponent::Look());
		check(look.recipes.size() == 1, "an inline recipe is read");
		check(look.recipes.size() == 1 && look.recipes[0].skeleton == "parts/rig.glb", "its skeleton as written");
		check(look.recipes.size() == 1 && look.recipes[0].part("body") && look.recipes[0].part("body")->file == "parts/body.glb",
			"its body part as written");
		check(look.height > 1.69f && look.height < 1.71f, "the rest of the look is read as for a model");
	}

	// a recipe file: its parts' relative files are taken from the file's folder
	{
		nlohmann::json json;
		json["recipe"] = recipeFile;
		const auto look = CharacterViewComponent::Look::from_json(json, CharacterViewComponent::Look());
		const std::string folder = (dir / "crowd").generic_string() + "/";
		check(look.recipes.size() == 1, "a recipe file is read");
		check(look.recipes.size() == 1 && look.recipes[0].skeleton == folder + "rig.glb", "its skeleton from the file's folder");
		const Pakal::CharacterRecipe::Part* head = look.recipes.empty() ? nullptr : look.recipes[0].part("head");
		check(head && head->file == folder + "heads.glb" && head->mesh == "Head_02", "its head part and mesh name");
		check(look.recipes.size() == 1 && look.recipes[0].shapes.count("heavy") == 1, "its shapes");
	}

	// a list of variants with "recipe" first; one that does not read is left out, the others kept in order
	{
		nlohmann::json json;
		json["recipe"] = nlohmann::json::parse(R"({ "parts": [ { "slot": "body", "file": "a.glb" } ] })");
		json["recipes"] = nlohmann::json::array({ recipeFile, (dir / "missing.json").generic_string(), 7,
			nlohmann::json::parse(R"({ "parts": [ { "slot": "body", "file": "c.glb" } ] })") });
		const auto look = CharacterViewComponent::Look::from_json(json, CharacterViewComponent::Look());
		check(look.recipes.size() == 3, "a missing file and a number are left out of the variants");
		check(look.recipes.size() == 3 && look.recipes[0].part("body")->file == "a.glb"
			&& look.recipes[2].part("body")->file == "c.glb", "the variants in order, 'recipe' first");
	}

	// a look over defaults: no recipe keeps the defaults'; a model file alone has none
	{
		CharacterViewComponent::Look defaults;
		defaults.recipes.resize(2);
		const auto kept = CharacterViewComponent::Look::from_json(nlohmann::json::parse(R"({ "height": 2.0 })"), defaults);
		check(kept.recipes.size() == 2, "a look naming no recipe keeps the defaults' recipes");
		const auto replaced = CharacterViewComponent::Look::from_json(nlohmann::json::parse(R"({ "recipes": [] })"), defaults);
		check(replaced.recipes.empty(), "an empty list replaces them");
		const auto model = CharacterViewComponent::Look::from_json(nlohmann::json::parse(R"({ "model": "thug.glb" })"),
			CharacterViewComponent::Look());
		check(model.recipes.empty() && model.model == "thug.glb", "a model file alone has no recipe");
	}

	std::error_code ignored;
	fs::remove_all(dir, ignored);
	std::printf(g_failures ? "%d failed\n" : "all passed\n", g_failures);
	return g_failures ? 1 : 0;
}
