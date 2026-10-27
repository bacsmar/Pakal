///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RandomStreamsTests.cpp
// Purpose: Named random streams: the same seed gives the same numbers, the streams do not touch each
//          other, and a different seed gives different numbers.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RandomStreams.h"

#include <cstdio>
#include <vector>

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};
	auto draw = [](Pakal::RandomStream& stream, int count)
	{
		std::vector<float> out;
		for (int i = 0; i < count; ++i) out.push_back(stream.uniform());
		return out;
	};

	{
		Pakal::RandomStreams a(2026), b(2026);
		check(draw(a["sim"], 20) == draw(b["sim"], 20), "the same seed gives the same numbers");
	}
	{
		Pakal::RandomStreams quiet(2026), busy(2026);
		draw(busy["fx"], 1000); // the presentation draws a lot...
		check(draw(quiet["sim"], 20) == draw(busy["sim"], 20), "... and the simulation's stream does not change");
	}
	{
		Pakal::RandomStreams streams(2026);
		check(draw(streams["sim"], 20) != draw(streams["fx"], 20), "two streams of one seed differ");
	}
	{
		Pakal::RandomStreams a(2026), b(2027);
		check(draw(a["sim"], 20) != draw(b["sim"], 20), "another seed gives other numbers");
		check(Pakal::RandomStreams::seed_for(2026, "sim") != Pakal::RandomStreams::seed_for(2027, "sim"), "nearby seeds give apart stream seeds");
	}
	{
		Pakal::RandomStream stream(1);
		bool inside = true;
		for (int i = 0; i < 1000; ++i)
		{
			const float u = stream.uniform(2.0f, 3.0f);
			const int n = stream.between(0, 3);
			inside = inside && u >= 2.0f && u < 3.0f && n >= 0 && n <= 3;
		}
		check(inside, "uniform and between stay in their ranges");
	}
	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
