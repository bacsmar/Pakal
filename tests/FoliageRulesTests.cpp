// The level's foliage rules as data: which models cast their shadow through a proxy (ShadowProxies), and the field of GPU
// grass made from the places of the grass models it replaces (GrassFromPlants). No graphics: both are what a level loader
// asks before it draws.
#include "topdown/view/GrassFromPlants.h"
#include "topdown/ShadowProxies.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		std::fflush(stdout);
		if (!ok) ++failures;
	};

	// --- shadow proxies
	{
		const std::set<std::string> files = { "trees/Tree_Big_01_shadow.glb", "trees/Palm_02_shadow.glb" };
		int asked = 0;
		Pakal::TopDown::ShadowProxies rules(nlohmann::json::parse(R"({ "suffix": "_shadow", "models": ["Tree_", "Palm_"] })"));
		rules.set_exists([&](const std::string& path) { ++asked; return files.count(path) > 0; });
		check(!rules.empty(), "rules with prefixes are not empty");
		check(rules.proxy_for("trees/Tree_Big_01.gltf") == "trees/Tree_Big_01_shadow.glb", "a tree of a listed prefix gets the proxy beside it");
		check(rules.proxy_for("trees/Palm_02.glb") == "trees/Palm_02_shadow.glb", "... whatever the model's own extension");
		check(rules.proxy_for("trees/Tree_Small_03.gltf").empty(), "a listed tree without a proxy file casts its own shadow");
		check(rules.proxy_for("props/Bench_01.gltf").empty(), "a model of no listed prefix gets none");
		check(rules.proxy_for("trees/Tree_Big_01_shadow.glb").empty(), "a proxy has no proxy of its own");
		const int before = asked;
		rules.proxy_for("trees/Tree_Big_01.gltf");
		rules.proxy_for("trees/Tree_Small_03.gltf");
		check(asked == before, "each file is asked about once, then remembered");

		Pakal::TopDown::ShadowProxies none(nlohmann::json::object());
		check(none.empty() && none.proxy_for("trees/Tree_Big_01.gltf").empty(), "no rules: no proxies");
		Pakal::TopDown::ShadowProxies other(nlohmann::json::parse(R"({ "suffix": "_sh", "models": ["Tree_"] })"));
		other.set_exists([](const std::string& path) { return path == "Tree_A_sh.glb"; });
		check(other.proxy_for("Tree_A.gltf") == "Tree_A_sh.glb", "the suffix is the rules' own");
	}

	// --- grass from plants
	{
		const nlohmann::json spec = nlohmann::json::parse(R"({
			"replaces": ["Grass_Short", "Grass_Med"], "cell": 2, "full": 0.3,
			"heights": [0.25, 0.6], "width": 0.06, "bladesPerSquareMeter": 40, "reach": 30,
			"root": [0.1, 0.2, 0.05], "tip": [0.4, 0.6, 0.2],
			"clumpColors": [ { "color": [0.6, 0.5, 0.3], "share": 0.3 } ], "deadShare": 0.05, "tuftBlades": 3
		})");
		Pakal::TopDown::GrassFromPlants grass(spec);
		check(!grass.empty(), "a spec with 'replaces' is not empty");
		check(grass.replaces("Grass_Short_Clump_01") && grass.replaces("nature/Grass_Med_02.gltf"), "the grass models it names, by name or by file");
		check(!grass.replaces("Bush_01") && !grass.replaces("Tall_Grass_Short"), "... and only those (a prefix of the name)");

		// a 10 x 10 m patch, a plant a meter, on a ground 2 m high; a lone plant far off on a ground at 5 m
		for (int x = 0; x < 10; ++x)
		{
			for (int z = 0; z < 10; ++z) grass.add(float(x), 2.0f, float(z));
		}
		grass.add(60.0f, 5.0f, 60.0f);
		check(grass.plants() == 101, "the places are kept");
		const Pakal::GraphicsSystem::GrassField field = grass.field();
		check(field.densityWidth > 0 && field.densityHeight > 0
			&& field.density.size() == size_t(field.densityWidth) * size_t(field.densityHeight)
			&& field.heights.size() == field.density.size(), "a density and a height a cell over the rectangle");
		check(field.minX < 0.0f && field.minZ < 0.0f && field.maxX > 60.0f && field.maxZ > 60.0f, "the rectangle holds every plant, with a margin");
		auto cell_of = [&field](float x, float z)
		{
			const int cx = int((x - field.minX) / (field.maxX - field.minX) * float(field.densityWidth));
			const int cz = int((z - field.minZ) / (field.maxZ - field.minZ) * float(field.densityHeight));
			return size_t(cz) * size_t(field.densityWidth) + size_t(cx);
		};
		check(field.density[cell_of(5.0f, 5.0f)] == 255, "the middle of the patch is full");
		check(field.density[cell_of(30.0f, 30.0f)] == 0, "nothing grows where no plant was");
		const uint8_t lone = field.density[cell_of(60.0f, 60.0f)];
		check(lone > 0 && lone < 255, "a lone plant makes a thin tuft, not a full cell");
		check(std::fabs(field.heights[cell_of(5.0f, 5.0f)] - 2.0f) < 0.01f && std::fabs(field.heights[cell_of(60.0f, 60.0f)] - 5.0f) < 0.01f,
			"the ground's height is the plants' own where they stand");
		check(field.heightMin == 0.25f && field.heightMax == 0.6f && field.width == 0.06f && field.bladesPerSquareMeter == 40.0f
			&& field.reach == 30.0f && field.tuftBlades == 3 && field.deadShare == 0.05f, "the blades' look comes from the spec");
		check(field.colorTip[1] == 0.6f && field.clumpColors[0].share == 0.3f && field.clumpColors[1].share == 0.0f,
			"colors and clump colors (the ones not given take no share)");

		Pakal::TopDown::GrassFromPlants empty(spec);
		check(empty.field().density.empty(), "no places: no field");
		Pakal::TopDown::GrassFromPlants off(nlohmann::json::object());
		check(off.empty() && !off.replaces("Grass_Short_01"), "no 'replaces': every grass model stays a model");
	}

	std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
	return failures ? 1 : 0;
}
