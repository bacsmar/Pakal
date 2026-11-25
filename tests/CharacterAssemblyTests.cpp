// CharacterAssemblyTests: a character made of parts (CharacterRecipe, CharacterAssembly). Two tiny glTF parts are written
// here (a three-bone skeleton: root, spine, and a hair bone under the spine), so the test needs no asset pack.

#include "CharacterRecipe.h"
#include "ResourceManager.h"
#include "bgfx/CharacterAssembly.h"

#include <assimp/scene.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* description)
	{
		std::printf("%s %s\n", condition ? "[ OK ]" : "[FAIL]", description);
		if (!condition) ++g_failures;
	}

	std::string base64(const std::vector<uint8_t>& bytes)
	{
		static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		for (size_t i = 0; i < bytes.size(); i += 3)
		{
			const uint32_t a = bytes[i], b = i + 1 < bytes.size() ? bytes[i + 1] : 0, c = i + 2 < bytes.size() ? bytes[i + 2] : 0;
			const uint32_t n = (a << 16) | (b << 8) | c;
			out += table[(n >> 18) & 63];
			out += table[(n >> 12) & 63];
			out += i + 1 < bytes.size() ? table[(n >> 6) & 63] : '=';
			out += i + 2 < bytes.size() ? table[n & 63] : '=';
		}
		return out;
	}

	template <typename T>
	size_t append(std::vector<uint8_t>& buffer, const std::vector<T>& values)
	{
		while (buffer.size() % 4) buffer.push_back(0);
		const size_t at = buffer.size();
		buffer.resize(at + values.size() * sizeof(T));
		std::memcpy(buffer.data() + at, values.data(), values.size() * sizeof(T));
		return at;
	}

	// A part: one triangle at x = 'offset', skinned to 'joint' (one vertex to the hair bone), with a blend shape "heavy"
	// that moves every vertex 1 m along x when 'shape'
	// 'extraBone': a fourth bone under the spine ("acc_dyn_1") that only this part has, its third vertex skinned to it;
	// 'rootScale': the root node scaled (an FBX-born rig at 0.01), the bind matrices made for it
	void write_part(const std::string& path, float offset, int joint, bool shape, bool extraBone = false, float rootScale = 1.0f)
	{
		std::vector<uint8_t> buffer;
		const std::vector<float> positions = { offset, 0, 0, offset + 1, 0, 0, offset, 1, 0 };
		const uint16_t last = extraBone ? 3 : 2;
		const std::vector<uint16_t> joints = { uint16_t(joint), 0, 0, 0, uint16_t(joint), 0, 0, 0, last, 0, 0, 0 };
		const std::vector<float> weights = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
		const std::vector<uint16_t> indices = { 0, 1, 2, 0 };
		const std::vector<float> moved = { 1, 0, 0, 1, 0, 0, 1, 0, 0 };
		std::vector<float> inverseBind;
		const int jointCount = extraBone ? 4 : 3;
		for (int j = 0; j < jointCount; ++j)
		{
			for (int i = 0; i < 16; ++i) inverseBind.push_back(i == 15 ? 1.0f : i % 5 == 0 ? 1.0f / rootScale : 0.0f);
		}
		const size_t pPos = append(buffer, positions), pJoints = append(buffer, joints), pWeights = append(buffer, weights);
		const size_t pIndices = append(buffer, indices), pMoved = append(buffer, moved), pBind = append(buffer, inverseBind);
		nlohmann::json views = nlohmann::json::array({
			{ { "buffer", 0 }, { "byteOffset", pPos }, { "byteLength", positions.size() * 4 } },
			{ { "buffer", 0 }, { "byteOffset", pJoints }, { "byteLength", joints.size() * 2 } },
			{ { "buffer", 0 }, { "byteOffset", pWeights }, { "byteLength", weights.size() * 4 } },
			{ { "buffer", 0 }, { "byteOffset", pIndices }, { "byteLength", 6 } },
			{ { "buffer", 0 }, { "byteOffset", pMoved }, { "byteLength", moved.size() * 4 } },
			{ { "buffer", 0 }, { "byteOffset", pBind }, { "byteLength", inverseBind.size() * 4 } } });
		nlohmann::json accessors = nlohmann::json::array({
			{ { "bufferView", 0 }, { "componentType", 5126 }, { "count", 3 }, { "type", "VEC3" },
				{ "min", { offset, 0, 0 } }, { "max", { offset + 1, 1, 0 } } },
			{ { "bufferView", 1 }, { "componentType", 5123 }, { "count", 3 }, { "type", "VEC4" } },
			{ { "bufferView", 2 }, { "componentType", 5126 }, { "count", 3 }, { "type", "VEC4" } },
			{ { "bufferView", 3 }, { "componentType", 5123 }, { "count", 3 }, { "type", "SCALAR" } },
			{ { "bufferView", 4 }, { "componentType", 5126 }, { "count", 3 }, { "type", "VEC3" }, { "min", { 1, 0, 0 } }, { "max", { 1, 0, 0 } } },
			{ { "bufferView", 5 }, { "componentType", 5126 }, { "count", jointCount }, { "type", "MAT4" } } });
		nlohmann::json primitive = { { "attributes", { { "POSITION", 0 }, { "JOINTS_0", 1 }, { "WEIGHTS_0", 2 } } }, { "indices", 3 },
			{ "material", 0 } };
		nlohmann::json mesh = { { "name", "part" }, { "primitives", nlohmann::json::array({ primitive }) } };
		if (shape)
		{
			mesh["primitives"][0]["targets"] = nlohmann::json::array({ { { "POSITION", 4 } } });
			mesh["extras"] = { { "targetNames", { "heavy" } } };
			mesh["weights"] = { 0.0 };
		}
		nlohmann::json nodes = nlohmann::json::array({
			{ { "name", "root" }, { "children", { 1 } }, { "scale", { rootScale, rootScale, rootScale } } },
			{ { "name", "spine" }, { "children", extraBone ? nlohmann::json({ 2, 4 }) : nlohmann::json({ 2 }) } },
			{ { "name", "hair_dyn_1" } },
			{ { "name", "body" }, { "mesh", 0 }, { "skin", 0 } } });
		if (extraBone) nodes.push_back({ { "name", "acc_dyn_1" }, { "translation", { 0.0, 0.5, 0.0 } } });
		const nlohmann::json jointList = extraBone ? nlohmann::json({ 0, 1, 2, 4 }) : nlohmann::json({ 0, 1, 2 });
		nlohmann::json gltf = {
			{ "asset", { { "version", "2.0" } } },
			{ "scene", 0 },
			{ "scenes", nlohmann::json::array({ { { "nodes", { 0, 3 } } } }) },
			{ "nodes", nodes },
			{ "skins", nlohmann::json::array({ { { "joints", jointList }, { "inverseBindMatrices", 5 } } }) },
			{ "meshes", nlohmann::json::array({ mesh }) },
			{ "materials", nlohmann::json::array({ { { "name", "skin" },
				{ "pbrMetallicRoughness", { { "baseColorFactor", { 0.8, 0.6, 0.5, 1.0 } } } } } }) },
			{ "buffers", nlohmann::json::array({ { { "byteLength", buffer.size() },
				{ "uri", "data:application/octet-stream;base64," + base64(buffer) } } }) },
			{ "bufferViews", views },
			{ "accessors", accessors } };
		std::ofstream(path, std::ios::binary) << gltf.dump();
	}

	// (by hand: the test links no Assimp of its own)
	const aiNode* find_node(const aiNode* node, const char* name)
	{
		if (std::strcmp(node->mName.C_Str(), name) == 0) return node;
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			if (const aiNode* found = find_node(node->mChildren[i], name)) return found;
		}
		return nullptr;
	}

	const aiBone* find_bone(const aiScene* scene, const char* name)
	{
		for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
		{
			for (unsigned int b = 0; b < scene->mMeshes[m]->mNumBones; ++b)
			{
				if (std::strcmp(scene->mMeshes[m]->mBones[b]->mName.C_Str(), name) == 0) return scene->mMeshes[m]->mBones[b];
			}
		}
		return nullptr;
	}
}

int main()
{
	const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pakal_character_tests";
	std::filesystem::create_directories(dir);
	ResourceMgr.set_root_path(dir.generic_string());
	write_part((dir / "torso.gltf").generic_string(), 0.0f, 1, true);
	write_part((dir / "legs.gltf").generic_string(), 5.0f, 0, false);
	write_part((dir / "pack.gltf").generic_string(), 2.0f, 1, false, true);          // a bone the skeleton lacks
	write_part((dir / "scaled.gltf").generic_string(), 3.0f, 1, false, false, 0.01f); // a root at 0.01

	// The recipe's JSON form
	{
		Pakal::CharacterRecipe recipe;
		std::string error;
		check(recipe.from_json(R"({"parts":[{"slot":"torso","file":"torso.gltf"},{"slot":"legs","file":"legs.gltf"}],
			"shapes":{"heavy":0.5},"foldBones":["hair_"]})", dir.generic_string(), error), "a recipe reads from JSON");
		check(recipe.parts.size() == 2 && recipe.part("legs") && recipe.shapes.at("heavy") == 0.5f && recipe.foldBones.size() == 1,
			"its parts, shapes and folded bones are read");
		Pakal::CharacterRecipe again;
		check(again.from_json(recipe.to_json(), std::string(), error) && again.parts.size() == 2 && again.part("torso")->file == recipe.part("torso")->file,
			"it writes back to the same JSON");
		check(!again.from_json(R"({"parts":[]})", std::string(), error), "a recipe with no parts is refused");
	}

	Pakal::CharacterRecipe recipe;
	recipe.parts = { { "torso", (dir / "torso.gltf").generic_string(), "" }, { "legs", (dir / "legs.gltf").generic_string(), "" } };

	// Assembled as it is
	{
		Pakal::AssembledCharacter character;
		std::string error;
		const bool ok = Pakal::assemble_character(recipe, character, error);
		check(ok && character.scene && character.scene->mNumMeshes == 2, "two parts assemble into one scene with both meshes");
		if (ok)
		{
			const aiScene* scene = character.scene.get();
			check(scene->mMeshes[0]->mMaterialIndex == scene->mMeshes[1]->mMaterialIndex,
				"parts with the same material share it (one draw call)");
			check(scene->mMeshes[0]->mNumAnimMeshes == 0, "no blend shape is left in the assembled model");
			check(std::fabs(scene->mMeshes[0]->mVertices[0].x - 0.0f) < 1e-4f, "an unbaked shape leaves the vertices where they were");
			check(find_bone(scene, "hair_dyn_1") != nullptr && find_bone(scene, "spine") != nullptr, "the bones with weights are kept");
			check(find_node(scene->mRootNode, "spine") != nullptr, "the skeleton's nodes are the scene's");
		}
	}

	// A shape baked in, the hair bone folded
	{
		recipe.shapes["heavy"] = 0.5f;
		recipe.foldBones = { "hair_" };
		Pakal::AssembledCharacter character;
		std::string error;
		const bool ok = Pakal::assemble_character(recipe, character, error);
		check(ok, "a recipe with a shape and a folded bone assembles");
		if (ok)
		{
			const aiScene* scene = character.scene.get();
			check(std::fabs(scene->mMeshes[0]->mVertices[0].x - 0.5f) < 1e-4f && std::fabs(scene->mMeshes[0]->mVertices[1].x - 1.5f) < 1e-4f,
				"the shape is baked into the vertices at its weight (half of its 1 m)");
			check(std::fabs(scene->mMeshes[1]->mVertices[0].x - 5.0f) < 1e-4f, "a part without the shape is left as it is");
			check(find_bone(scene, "hair_dyn_1") == nullptr, "the folded bone is gone");
			const aiBone* spine = find_bone(scene, "spine");
			check(spine && spine->mNumWeights == 3, "its weights went to the bone above it (spine: its two vertices and the hair's)");
		}
	}

	// A part that brings a bone the skeleton lacks, and a part whose rig is scaled at its root
	{
		Pakal::CharacterRecipe mixed;
		mixed.skeleton = (dir / "legs.gltf").generic_string();
		mixed.parts = { { "legs", (dir / "legs.gltf").generic_string(), "" }, { "pack", (dir / "pack.gltf").generic_string(), "" },
			{ "scaled", (dir / "scaled.gltf").generic_string(), "" } };
		Pakal::AssembledCharacter character;
		std::string error;
		bool ok = Pakal::assemble_character(mixed, character, error);
		const aiNode* added = ok ? find_node(character.scene->mRootNode, "acc_dyn_1") : nullptr;
		check(added && added->mParent && std::strcmp(added->mParent->mName.C_Str(), "spine") == 0,
			"a bone the skeleton lacks comes from its part, under the same parent");
		if (ok)
		{
			// the scaled part's bones: their place in the assembled skeleton times their offset is the part's bind (the identity
			// here: its vertices stand at rest where its own file has them)
			const aiBone* spine = nullptr;
			for (unsigned int b = 0; b < character.scene->mMeshes[2]->mNumBones; ++b)
				if (std::strcmp(character.scene->mMeshes[2]->mBones[b]->mName.C_Str(), "spine") == 0) spine = character.scene->mMeshes[2]->mBones[b];
			check(spine && std::fabs(spine->mOffsetMatrix.a1 - 1.0f) < 1e-3f && std::fabs(spine->mOffsetMatrix.b2 - 1.0f) < 1e-3f,
				"a part whose rig is scaled at its root lands at the skeleton's scale (its offsets made again)");
		}
		mixed.foldBones = { "acc_", "hair_" };
		ok = Pakal::assemble_character(mixed, character, error);
		check(ok && find_bone(character.scene.get(), "acc_dyn_1") == nullptr && find_bone(character.scene.get(), "hair_dyn_1") == nullptr,
			"a bone a part brings folds into its parent there, not into the root");
	}

	// Over the limit
	{
		recipe.foldBones.clear();
		Pakal::AssembledCharacter character;
		std::string error;
		check(!Pakal::assemble_character(recipe, character, error, 2) && error.find("bones") != std::string::npos,
			"more bones than the skinning takes is refused, and says so");
		recipe.parts.push_back({ "hat", (dir / "missing.gltf").generic_string(), "" });
		check(!Pakal::assemble_character(recipe, character, error) && error.find("missing") != std::string::npos,
			"a part that cannot be read is refused, naming it");
	}

	std::error_code ignored;
	std::filesystem::remove_all(dir, ignored);
	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	return g_failures == 0 ? 0 : 1;
}
