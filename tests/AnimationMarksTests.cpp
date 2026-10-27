///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AnimationMarksTests.cpp
// Purpose: The marks of a clip the playback passes: once each, in order, across the loop's end,
//          played backwards, and stopping at the ends of a clip that does not loop.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AnimationMarks.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
	std::string names(const std::vector<const Pakal::AnimationMark*>& marks)
	{
		std::string text;
		for (const auto* mark : marks)
		{
			if (!text.empty()) text += ",";
			text += mark->name;
		}
		return text;
	}
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	// A clip of 1 s: a step at 0.25, the fist lands at 0.5, a step at 0.75
	const std::vector<Pakal::AnimationMark> marks = { { 0.25, "stepL" }, { 0.5, "impact" }, { 0.75, "stepR" } };
	check(names(Pakal::marks_passed(marks, 0.0, 0.4, 1.0, false)) == "stepL", "the marks between two times");
	check(names(Pakal::marks_passed(marks, 0.4, 0.5, 1.0, false)) == "impact", "a mark right at the new time is passed");
	check(names(Pakal::marks_passed(marks, 0.5, 0.6, 1.0, false)).empty(), "and not again from there");
	check(names(Pakal::marks_passed(marks, 0.6, 1.4, 1.0, true)) == "stepR,stepL", "a looping clip passes the end into its next lap");
	check(names(Pakal::marks_passed(marks, 0.1, 2.3, 1.0, true)) == "stepL,impact,stepR,stepL,impact,stepR,stepL", "a long frame passes every lap in order");
	check(names(Pakal::marks_passed(marks, 0.9, 1.4, 1.0, false)).empty(), "a clip that does not loop stops at its end");
	check(names(Pakal::marks_passed(marks, 0.8, 0.3, 1.0, false)) == "stepR,impact", "played backwards, the marks come in reverse");
	check(names(Pakal::marks_passed(marks, 0.3, -0.4, 1.0, true)) == "stepL,stepR", "and backwards across the start of a loop");
	check(names(Pakal::marks_passed(marks, 0.2, 0.2, 1.0, true)).empty() && names(Pakal::marks_passed({}, 0.0, 1.0, 1.0, true)).empty(),
		"no time, or no marks: nothing");

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
