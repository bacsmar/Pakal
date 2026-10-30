///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLayerEditingTests.cpp
// Purpose: A layer edited as one level and saved as a layer: a new layer over a base, a base object
//          moved (adjust), one deleted (hide) and one added, a zone added; the layer file holds only
//          that, and loading it composed gives back the edited level. A base without keys is refused.
//          The intro camera ("camera": {"intro": ...}) edited in the layer is saved whole in it, the base's
//          other camera settings left to the base; read as a CameraPan's keys (LevelCameraPath).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"
#include "LevelEditor.h"
#include "LevelLayerEditing.h"
#include "topdown/LevelCameraPath.h"

#include <cmath>

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Pakal;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		if (!condition)
		{
			++g_failures;
			std::printf("FAILED: %s\n", what);
		}
	}

	void write(const std::filesystem::path& path, const char* text)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
	}
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	namespace fs = std::filesystem;
	const fs::path root = fs::temp_directory_path() / "pakal_layer_editing_tests";
	fs::remove_all(root);
	write(root / "town" / "level.json", R"({ "format": "pakal-level", "version": 2, "name": "town", "units_per_meter": 100, "spawn": [0, 0],
		"objects": [
			{ "key": "house_01", "name": "House", "mesh": "meshes/house.glb", "x": 0, "y": 0, "z": 0, "rot": [0, 0, 0], "scale": 1 },
			{ "key": "car_01", "name": "Car", "mesh": "meshes/car.glb", "x": 500, "y": 0, "z": 0, "rot": [0, 0, 0], "scale": 1 },
			{ "key": "tree_01", "name": "Tree", "mesh": "meshes/tree.glb", "x": 900, "y": 0, "z": 0, "rot": [0, 0, 0], "scale": 1 } ],
		"zones": [ { "name": "town", "kind": "street", "rect": [0, 0, 1000, 1000] } ],
		"camera": { "distance": 1.5 } })");

	std::string error;
	const std::string layerPath = (root / "mission_01" / "level.json").string();
	check(LevelLayerEditing::create(layerPath, (root / "town" / "level.json").string(), error), "a new layer is made over a base");
	check(!LevelLayerEditing::create(layerPath, (root / "town" / "level.json").string(), error), "it is not made twice");

	LevelDocument doc;
	LevelLayerEditing layer;
	check(layer.open(layerPath, doc, error) && layer.layered(), "the layer opens over its base");
	check(doc.objects.size() == 3 && layer.is_base("car_01") && doc.objects[0].mesh == "../town/meshes/house.glb", "with the base's objects, their paths from the layer");

	LevelEditor editor(doc);
	check(editor.apply({ { "op", "move" }, { "object", "car_01" }, { "x", 7.0 }, { "y", 1.0 } }).empty(), "a base object moves");
	check(editor.apply({ { "op", "delete" }, { "object", "tree_01" } }).empty(), "a base object is deleted");
	check(editor.apply({ { "op", "add" }, { "mesh", "meshes/crate.glb" }, { "x", 2.0 }, { "y", 2.0 }, { "key", "crate_01" } }).empty(), "an object is added");
	check(editor.apply({ { "op", "zone" }, { "name", "arena" }, { "kind", "spawn" }, { "rect", Json::array({ 1, 1, 3, 3 }) } }).empty(), "a zone is added");

	const Json saved = layer.layer_json(doc);
	check(saved.value("extends", std::string()) == "../town/level.json", "the layer names its base");
	check(saved["objects"].size() == 1 && saved["objects"][0]["key"] == "crate_01", "only the added object is the layer's");
	check(saved["hide"] == Json::array({ "tree_01" }), "the deleted base object is hidden");
	check(saved["adjust"].size() == 1 && saved["adjust"]["car_01"] == Json::parse(R"({"x":700.0,"y":100.0})"), "the moved one is adjusted, and only where it moved");
	check(saved["zones"].size() == 1 && saved["zones"][0]["name"] == "arena", "only the added zone is the layer's");
	check(!saved.contains("meta") || !saved["meta"].contains("layers"), "what composing added is not saved");

	check(layer.hidden(doc) == std::vector<std::string>({ "tree_01" }), "the hidden objects are known");
	check(layer.base_object("tree_01").value("mesh", std::string()) == "../town/meshes/tree.glb", "a hidden one can be put back as the base has it");
	check(editor.apply({ { "op", "insert" }, { "data", layer.base_object("tree_01") }, { "at", layer.base_index("tree_01", doc) } }).empty()
		&& layer.hidden(doc).empty() && doc.objects[2].key == "tree_01", "put back where it was");
	check(!layer.layer_json(doc).contains("hide"), "and no longer hidden");
	editor.undo();

	// the intro camera: an operation like the others (undoable), saved whole in the layer
	const Json intro = Json::parse(R"({ "seconds": 6, "shots": [ { "eye": [100, 200, 300], "look": [0, 0, 0] }, { "eye": [400, 0, 500], "look": [0, 0, 0], "fov": 40 } ] })");
	check(!editor.apply({ { "op", "camera" }, { "data", Json::parse(R"({ "shots": [ { "eye": [1, 2] } ] })") } }).empty(), "a shot without its points is refused");
	check(editor.apply({ { "op", "camera" }, { "data", intro } }).empty(), "the intro camera is set");
	check(doc.extra["camera"]["intro"] == intro && doc.extra["camera"]["distance"] == 1.5, "beside the base's camera settings");
	const Json savedCamera = layer.layer_json(doc).value("camera", Json());
	check(savedCamera == Json({ { "intro", intro } }), "the layer saves the intro, not the base's distance");

	Pakal::TopDown::LevelCameraPath path;
	check(path.read(doc.extra["camera"]["intro"], doc.unitsPerMeter, 22.0f) && path.keys.size() == 2 && path.endOnPlay && path.seconds == 6.0f, "it reads as two shots ending on play");
	const auto& first = path.keys[0];
	check(std::fabs(first.eye[0] - 1.0f) < 1e-5f && std::fabs(first.eye[1] - 3.0f) < 1e-5f && std::fabs(first.eye[2] + 2.0f) < 1e-5f, "in render space: x, z up, -y, meters");
	check(first.at == 0.0f && std::fabs(path.keys[1].at - 0.5f) < 1e-5f && first.fovY == 22.0f && path.keys[1].fovY == 40.0f, "spread evenly before the play camera; the fov its own or the play one's");

	check(editor.apply({ { "op", "camera" }, { "data", nullptr } }).empty() && doc.extra["camera"] == Json({ { "distance", 1.5 } }), "the intro camera is removed");
	editor.undo();
	check(doc.extra["camera"]["intro"] == intro, "and comes back with undo");

	check(layer.save(layerPath, doc, error), "the layer saves");
	LevelDocument again;
	check(load_level(layerPath, again, error), "it loads composed");
	bool same = again.objects.size() == doc.objects.size();
	for (size_t i = 0; same && i < doc.objects.size(); ++i)
	{
		same = again.objects[i].key == doc.objects[i].key && again.objects[i].x == doc.objects[i].x && again.objects[i].mesh == doc.objects[i].mesh;
	}
	check(same, "as it was edited");
	check(again.extra["camera"]["intro"] == intro && again.extra["camera"]["distance"] == 1.5, "with its intro camera over the base's settings");

	// a base whose objects have no keys cannot be layered over
	write(root / "old" / "level.json", R"({ "format": "pakal-level", "version": 2, "objects": [ { "name": "Box", "mesh": "b.glb" } ] })");
	write(root / "over_old" / "level.json", R"({ "format": "pakal-level", "version": 2, "extends": "../old/level.json" })");
	LevelDocument refused;
	LevelLayerEditing old;
	check(!old.open((root / "over_old" / "level.json").string(), refused, error) && error.find("no key") != std::string::npos, "a base without keys is refused");

	// a base with an intro camera: the layer's changed intro drops what it lacks (composing merges key by key)
	write(root / "shot" / "level.json", R"({ "format": "pakal-level", "version": 2, "units_per_meter": 100,
		"objects": [ { "key": "box_01", "name": "Box", "mesh": "b.glb" } ],
		"camera": { "intro": { "seconds": 5, "end": "last", "shots": [ { "eye": [0, 0, 500], "look": [0, 0, 0] } ] } } })");
	write(root / "over_shot" / "level.json", R"({ "format": "pakal-level", "version": 2, "extends": "../shot/level.json" })");
	{
		LevelDocument shotDoc;
		LevelLayerEditing shotLayer;
		check(shotLayer.open((root / "over_shot" / "level.json").string(), shotDoc, error), "a layer over a base with an intro camera opens");
		check(!shotLayer.layer_json(shotDoc).contains("camera"), "an unchanged camera is not the layer's");
		LevelEditor shotEditor(shotDoc);
		const Json mine = Json::parse(R"({ "seconds": 7, "shots": [ { "eye": [100, 0, 500], "look": [0, 0, 0] } ] })");
		check(shotEditor.apply({ { "op", "camera" }, { "data", mine } }).empty(), "the layer changes it");
		check(shotLayer.layer_json(shotDoc)["camera"]["intro"].value("end", Json(0)).is_null(), "what it lacks is written as null");
		check(shotLayer.save((root / "over_shot" / "level.json").string(), shotDoc, error), "and saved");
		LevelDocument composed;
		check(load_level((root / "over_shot" / "level.json").string(), composed, error) && composed.extra["camera"]["intro"] == mine, "it loads as the layer has it");
	}

	// a whole level is not layered
	LevelDocument whole;
	LevelLayerEditing plain;
	check(plain.open((root / "town" / "level.json").string(), whole, error) && !plain.layered() && whole.objects.size() == 3, "a whole level opens as it is");

	fs::remove_all(root);
	if (g_failures == 0) std::printf("LevelLayerEditingTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
