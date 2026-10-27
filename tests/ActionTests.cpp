///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ActionTests.cpp
// Purpose: The action timeline: phases in order with their times, phases of no time passing at once,
//          the cooldown, the cost, and cancelling.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/Action.h"

#include <cstdio>
#include <vector>

using Pakal::TopDown::ActionPhase;
using Pakal::TopDown::ActionTimeline;

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};
	const float dt = 0.125f;   // times in eighths of a second: exact in binary, so the phases end on whole steps

	{
		ActionTimeline punch;
		std::vector<std::pair<int, ActionPhase>> seen;   // (step, phase)
		int step = 0;
		punch.onPhase = [&](ActionPhase phase) { seen.push_back({ step, phase }); };
		check(punch.start({ .windup = 0.25f, .active = 0.125f, .recovery = 0.375f, .cooldown = 0.5f }), "an idle action starts");
		check(!punch.start({}), "and cannot start again while under way");
		for (step = 1; step <= 6; ++step) punch.update(dt);
		check(seen.size() == 4 && seen[0] == std::make_pair(0, ActionPhase::Windup) && seen[1] == std::make_pair(2, ActionPhase::Active)
			&& seen[2] == std::make_pair(3, ActionPhase::Recovery) && seen[3] == std::make_pair(6, ActionPhase::Idle),
			"it winds up, is active and recovers for their times, then it is over");
		check(!punch.busy() && !punch.ready() && punch.cooldown_left() > 0.0f, "then it cools down");
		for (int i = 0; i < 4; ++i) punch.update(dt);
		check(punch.ready(), "and is ready again when the cooldown is over");
	}
	{
		ActionTimeline swing;
		std::vector<ActionPhase> seen;
		swing.onPhase = [&](ActionPhase phase) { seen.push_back(phase); };
		swing.start({ .cooldown = 0.375f });
		check(seen.size() == 4 && seen[1] == ActionPhase::Active && seen[3] == ActionPhase::Idle && !swing.busy(),
			"phases of no time pass at once: a swing with no windup is active and over before start() returns");
		swing.update(dt);
		swing.update(dt);
		check(!swing.ready(), "its cooldown still holds it");
		swing.update(dt);
		check(swing.ready(), "until it runs out");
	}
	{
		ActionTimeline spell;
		int mana = 1;
		auto pay = [&mana]() { if (mana <= 0) return false; --mana; return true; };
		check(spell.start({ .active = 0.1f }, pay) && mana == 0, "an action that can be paid for starts and is paid");
		spell.update(dt);
		check(!spell.start({ .active = 0.1f }, pay) && !spell.busy(), "one that cannot be paid for does not start");
	}
	{
		ActionTimeline reload;
		bool loaded = false;
		reload.onPhase = [&](ActionPhase phase) { if (phase == ActionPhase::Active) loaded = true; };
		reload.start({ .windup = 0.75f, .cooldown = 1.0f });
		reload.update(dt);
		reload.cancel();
		check(!loaded && !reload.busy() && reload.ready(), "a cancelled action stops where it is, with no cooldown");
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
