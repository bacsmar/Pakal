///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ModelShapeTests.cpp
// Purpose: A model's shape (graphics/ModelShape.h) as the loader fills it, without a GPU: a tree whose leaf
//          cards are split off at load gives its crown's real lowest point; a model with no split gives
//          none. The models are written here (OBJ, a cutout material named by its .materials.xml).
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "ResourceManager.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	int g_failed = 0;
	void check(bool ok, const char* what)
	{
		std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
		if (!ok) ++g_failed;
	}

	// A trunk (a box 3 units tall) and 40 leaf cards (each a quad of its own) from 'cardsFrom' up, one material "leaf"
	void write_tree(const std::string& name, float cardsFrom, bool cutout)
	{
		std::ofstream mtl(name + ".mtl");
		mtl << "newmtl leaf\nKd 0.2 0.6 0.2\n";
		std::ofstream obj(name + ".obj");
		obj << "mtllib " << name << ".mtl\nusemtl leaf\n";
		int v = 0;
		auto vertex = [&](float x, float y, float z) { obj << "v " << x << ' ' << y << ' ' << z << '\n'; return ++v; };
		// the trunk: 8 corners, 12 triangles (one piece: they share corners)
		const float r = 0.2f;
		int c[8];
		for (int i = 0; i < 8; ++i) c[i] = vertex(i & 1 ? r : -r, i & 2 ? 3.0f : 0.0f, i & 4 ? r : -r);
		const int faces[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
		for (const auto& f : faces)
		{
			obj << "f " << c[f[0]] << ' ' << c[f[1]] << ' ' << c[f[2]] << '\n';
			obj << "f " << c[f[0]] << ' ' << c[f[2]] << ' ' << c[f[3]] << '\n';
		}
		// the cards: each its own four corners, spread around and up from 'cardsFrom' to 'cardsFrom' + 2
		for (int k = 0; k < 40; ++k)
		{
			const float a = float(k) * 0.7f, x = std::cos(a) * 1.5f, z = std::sin(a) * 1.5f;
			const float y = cardsFrom + 2.0f * float(k) / 39.0f;
			const int p = vertex(x, y, z), q = vertex(x + 0.3f, y, z), s = vertex(x + 0.3f, y + 0.3f, z), t = vertex(x, y + 0.3f, z);
			obj << "f " << p << ' ' << q << ' ' << s << "\nf " << p << ' ' << s << ' ' << t << '\n';
		}
		if (cutout)
		{
			std::ofstream xml(name + ".obj.materials.xml");
			xml << "<materials><material name=\"leaf\" blend=\"alphatest\"/></materials>\n";
		}
	}
}

int main()
{
	namespace fs = std::filesystem;
	const fs::path dir = fs::temp_directory_path() / "pakal_model_shape_tests";
	fs::remove_all(dir);
	fs::create_directories(dir);
	fs::current_path(dir);
	ResourceMgr.set_root_path(".");

	write_tree("tree", 4.0f, true);
	write_tree("bush", 4.0f, false);

	auto tree = Pakal::MeshComponent3D_Bgfx::decode("tree.obj");
	check(tree != nullptr, "the tree decodes without a GPU");
	if (tree)
	{
		const Pakal::ModelShape& shape = tree->shape();
		check(shape.has_crown(), "a tree whose cards stand above a third of its height has a crown split off");
		check(shape.has_crown() && std::fabs(shape.crownBase - 4.0f) < 1e-3f, "its crown base is its lowest card's lowest point");
		check(std::fabs(shape.lo[1] - 0.0f) < 1e-3f && std::fabs(shape.hi[1] - 6.3f) < 1e-3f, "its box is the model's");
		float lo[3], hi[3];
		tree->get_bounds(lo, hi);
		check(lo[1] == shape.lo[1] && hi[1] == shape.hi[1], "get_bounds reads the same box");
	}

	auto bush = Pakal::MeshComponent3D_Bgfx::decode("bush.obj");
	check(bush != nullptr, "the opaque model decodes");
	if (bush) check(!bush->shape().has_crown(), "an opaque model (no cutout cards) has no crown");

	fs::current_path(fs::temp_directory_path());
	fs::remove_all(dir);
	std::printf("%s\n", g_failed ? "ModelShapeTests: FAILED" : "ModelShapeTests: all passed");
	return g_failed ? 1 : 0;
}
