///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteAtlasTests.cpp
// Purpose: The engine's reading of an atlas made by tools/pakal_atlas: the frames and their meshes (a rectangle when the
//          frame has none, the positions around the pivot, the texture coordinates in atlas pixels), the animations (the
//          frame at a time, a loop, one that plays once), the size in meters, and what it refuses.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "gameplay/SpriteAtlas.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
	const char* kAtlas = R"({
	  "format": "pakal-atlas", "version": 1, "image": "set.png", "size": [256, 160], "pixelsPerMeter": 50,
	  "frames": {
	    "box":  { "rect": [200, 100, 40, 30], "offset": [10, 5], "source": [100, 80] },
	    "tri":  { "rect": [0, 0, 20, 20], "offset": [0, 0], "source": [20, 20], "pivot": [0.5, 1.0],
	              "vertices": [[0, 0], [20, 0], [10, 20]], "triangles": [0, 1, 2] },
	    "a":    { "rect": [0, 30, 10, 10], "offset": [0, 0], "source": [10, 10] },
	    "b":    { "rect": [10, 30, 10, 10], "offset": [0, 0], "source": [10, 10] },
	    "c":    { "rect": [20, 30, 10, 10], "offset": [0, 0], "source": [10, 10] }
	  },
	  "animations": {
	    "walk": { "frames": ["a", "b", "c"], "ms": [100, 50, 50], "loop": true },
	    "hit":  { "frames": ["a", "b"], "ms": [40, 60], "loop": false }
	  }
	})";

	bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	Pakal::SpriteAtlas atlas;
	std::string why;
	check(atlas.load_json(kAtlas, &why), "an atlas loads");
	if (!why.empty()) std::printf("  (%s)\n", why.c_str());
	check(atlas.image() == "set.png" && atlas.width() == 256 && atlas.height() == 160 && near(atlas.pixels_per_meter(), 50.0f), "its picture, size and scale");
	check(atlas.frames().size() == 5 && atlas.frame("box") && !atlas.frame("nope"), "its frames, by name");

	// a frame without a mesh: its rectangle, placed in the untrimmed frame around the pivot (the centre)
	const Pakal::AtlasFrame* box = atlas.frame("box");
	check(box && box->mesh.positions.size() == 8 && box->mesh.indices.size() == 6, "a frame without a mesh is a rectangle of two triangles");
	check(box && near(box->mesh.positions[0], 0.1f - 0.5f) && near(box->mesh.positions[1], 0.5f - 5.0f / 80.0f), "its top left corner is where the offset puts it, y up");
	check(box && near(box->mesh.positions[4], 50.0f / 100.0f - 0.5f) && near(box->mesh.positions[5], 0.5f - 35.0f / 80.0f), "and its bottom right corner");
	check(box && near(box->mesh.uvs[0], 200.0f) && near(box->mesh.uvs[1], 100.0f) && near(box->mesh.uvs[4], 240.0f) && near(box->mesh.uvs[5], 130.0f), "its texture coordinates are atlas pixels");

	// a mesh and a pivot at the feet
	const Pakal::AtlasFrame* tri = atlas.frame("tri");
	check(tri && tri->mesh.indices.size() == 3 && tri->mesh.positions.size() == 6, "a frame with a mesh keeps it");
	check(tri && near(tri->mesh.positions[0], -0.5f) && near(tri->mesh.positions[1], 1.0f) && near(tri->mesh.positions[5], 0.0f), "the pivot is where the sprite's centre is (at the feet: the picture goes up from it)");

	float w = 0, h = 0;
	atlas.size_in_meters(*box, w, h);
	check(near(w, 2.0f) && near(h, 1.6f), "the sprite is the size of the untrimmed frame in meters");

	// animations
	const Pakal::AtlasAnimation* walk = atlas.animation("walk");
	const Pakal::AtlasAnimation* hit = atlas.animation("hit");
	check(walk && hit && walk->total_ms() == 200 && !atlas.animation("nope"), "the animations, by name");
	check(walk && atlas.frame_at(*walk, 0)->name == "a" && atlas.frame_at(*walk, 99)->name == "a" && atlas.frame_at(*walk, 100)->name == "b" && atlas.frame_at(*walk, 160)->name == "c", "the frame at a time");
	check(walk && atlas.frame_at(*walk, 200)->name == "a" && atlas.frame_at(*walk, 1310)->name == "b", "a loop goes round");
	check(hit && atlas.frame_at(*hit, 10000)->name == "b", "one that plays once holds its last frame");

	// what it refuses
	Pakal::SpriteAtlas other;
	check(!other.load_json("not json", &why) && !why.empty(), "a text that is not JSON");
	check(!other.load_json(R"({"format":"other","version":1})", &why), "another format");
	check(!other.load_json(R"({"format":"pakal-atlas","version":2})", &why), "another version");
	check(!other.load_json(R"({"format":"pakal-atlas","version":1,"size":[10,10],"frames":{"f":{"rect":[5,5,10,10],"offset":[0,0],"source":[10,10]}}})", &why), "a frame outside the atlas");
	check(!other.load_json(R"({"format":"pakal-atlas","version":1,"size":[10,10],"frames":{"f":{"rect":[0,0,5,5],"offset":[0,0],"source":[5,5],"vertices":[[0,0],[5,0],[5,5]],"triangles":[0,1,3]}}})", &why), "a triangle past its vertices");
	check(!other.load_json(R"({"format":"pakal-atlas","version":1,"size":[10,10],"frames":{},"animations":{"a":{"frames":["ghost"]}}})", &why), "an animation of a frame that does not exist");
	check(other.frames().empty(), "and what failed leaves nothing behind");

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED (0 failures)\n", failures);
	return failures ? 1 : 0;
}
