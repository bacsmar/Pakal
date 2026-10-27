///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CollisionLayersTests.cpp
// Purpose: Collision layers by name: bits in the order they are added, masks, names back, the limit.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CollisionLayers.h"

#include <cstdio>
#include <string>

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	Pakal::CollisionLayers layers;
	const uint16_t wall = layers.add("wall"), player = layers.add("player"), enemy = layers.add("enemy");
	check(wall == 0x0001 && player == 0x0002 && enemy == 0x0004, "layers take bits in the order they are added");
	check(layers.add("player") == player && layers.size() == 3, "adding a layer again gives its bit");
	check(layers.bit("enemy") == enemy && layers.bit("ghost") == 0, "a layer's bit by name, 0 for an unknown one");
	check(layers.mask({ "wall", "enemy" }) == 0x0005 && layers.mask({ "wall", "ghost" }) == 0x0001, "masks of several layers");
	check(layers.name_of(0x0004) == "enemy" && layers.name_of(0x0003).empty(), "the name of a bit");
	for (int i = 0; i < 20; ++i) layers.add("more" + std::to_string(i));
	check(layers.size() == Pakal::CollisionLayers::kMaxLayers && layers.add("one too many") == 0, "at most 16 layers");

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
