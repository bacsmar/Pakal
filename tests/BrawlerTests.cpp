///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerTests.cpp
// Purpose: The brawler prototype's simulation on the TopDown module, without a window: moves hit in
//          their active phase and stun, launchers fling into the air, air moves only in the air, the
//          enemies attack, and the bot wins the arena the same way every time with one seed.
//          Argument: the path of brawler.json.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BrawlerGame.h"
#include "topdown/Height.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>

using namespace Brawler;

namespace
{
	const float kDt = 1.0f / 60.0f;

	// A game of the rules, with its own world; 'enemies' false leaves only the player
	struct Match
	{
		std::unique_ptr<Pakal::PhysicsWorld2D> world = std::make_unique<Pakal::PhysicsWorld2D>(b2Vec2(0.0f, 0.0f));
		Pakal::NavGrid2D nav{ 0.5f };
		BrawlerRules rules;
		std::unique_ptr<BrawlerGame> game;

		Match(const BrawlerRules& from, bool enemies, uint32_t seed = 7) : rules(from)
		{
			if (!enemies) rules.enemies.clear();
			game = std::make_unique<BrawlerGame>(*world, nav, rules, seed);
			game->start();
		}
		Actor& hero() { return *game->player(); }
		void run(float seconds, const PlayerInput& input = PlayerInput())
		{
			for (float t = 0.0f; t < seconds; t += kDt) game->step(kDt, input);
		}
		// The button (the hero starts facing east, where the enemies are put)
		void press(Button button)
		{
			PlayerInput input;
			input.held = input.pressed = 1u << button;
			game->step(kDt, input);
		}
		float health(const Actor& actor) const { return actor.get<HealthComponent>()->current(); }
	};
}

int main(int argc, char** argv)
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};
	if (argc < 2)
	{
		std::printf("usage: BrawlerTests <brawler.json>\n");
		return 1;
	}
	std::ifstream file(argv[1]);
	const BrawlerRules rules = BrawlerRules::from_json(nlohmann::json::parse(file));
	check(rules.fighters.size() == 3 && rules.enemies.size() == 2 && rules.buttons.count("light") == 1, "the rules are read");

	// A jab: it hits in its active phase, once, and stuns
	{
		Match match(rules, false);
		Actor& thug = match.game->add_fighter("thug", match.hero().position + b2Vec2(0.9f, 0.0f), false);
		match.run(0.05f);
		const float before = match.health(thug);
		match.press(Light);
		match.run(0.04f);
		const float duringWindup = match.health(thug);
		match.run(0.3f);
		check(duringWindup == before && std::fabs(match.health(thug) - (before - 8.0f)) < 0.01f,
			"a jab hits once its windup is over, for its damage, and only once");
		check(match.hero().get<FighterComponent>()->hits() == 1, "the fighter counts the hit");
	}

	// A hook launches: the one hit flies up and lands; while it is stunned it does not attack
	{
		Match match(rules, false);
		Actor& thug = match.game->add_fighter("thug", match.hero().position + b2Vec2(0.95f, 0.0f), false);
		match.run(0.05f);
		match.press(Heavy);
		float peak = 0.0f;
		bool stunned = false;
		for (int i = 0; i < 90; ++i)
		{
			match.game->step(kDt, PlayerInput());
			peak = std::max(peak, height_of(thug));
			stunned = stunned || thug.get<FighterComponent>()->stunned();
		}
		std::printf("  hook: the thug flew %.2f m up\n", peak);
		check(peak > 0.4f && height_of(thug) == 0.0f && stunned, "a hook flings the one hit into the air, stunned, and it lands");
	}

	// Air moves only in the air: a jab while jumping is the air kick
	{
		Match match(rules, false);
		FighterComponent& fighter = *match.hero().get<FighterComponent>();
		check(!fighter.perform("airKick"), "the air kick cannot be done on the floor");
		match.press(Jump);
		match.run(0.15f);
		check(height_of(match.hero()) > 0.3f, "the jump leaves the floor");
		match.press(Light);
		check(fighter.move() && fighter.move()->name == "airKick", "the light button in the air does the air kick");
		match.run(1.5f);
		check(height_of(match.hero()) == 0.0f, "and the hero lands");
	}

	// A combo: the light button pressed again during each punch gives jab, jab, cross
	{
		Match match(rules, false);
		FighterComponent& fighter = *match.hero().get<FighterComponent>();
		std::vector<std::string> seen;
		auto note = [&]() { if (fighter.move() && (seen.empty() || seen.back() != fighter.move()->name)) seen.push_back(fighter.move()->name); };
		for (int i = 0; i < 90; ++i)
		{
			PlayerInput input;
			if (i % 8 == 0) input.held = input.pressed = 1u << Light;   // pressed again and again
			match.game->step(kDt, input);
			note();
		}
		check(seen.size() >= 3 && seen[0] == "jab" && seen[1] == "jab2" && seen[2] == "cross", "pressing light again chains jab, jab, cross");
	}

	// The guard: a punch from the front does a fifth of its damage and does not stun
	{
		Match match(rules, false);
		Actor& thug = match.game->add_fighter("thug", match.hero().position + b2Vec2(0.9f, 0.0f), false);
		PlayerInput guard;
		guard.held = 1u << Block;
		match.game->step(kDt, guard);
		thug.get<FighterComponent>()->perform("punch");
		bool stunned = false;
		for (int i = 0; i < 40; ++i)
		{
			match.game->step(kDt, guard);
			stunned = stunned || match.hero().get<FighterComponent>()->stunned();
		}
		std::printf("  guard: the punch took %.1f\n", 100.0f - match.health(match.hero()));
		check(std::fabs(match.health(match.hero()) - (100.0f - 7.0f * 0.2f)) < 0.01f && !stunned, "a guarded punch does a fifth and does not stun");
	}

	// A dodge: while the roll is under way the punches miss
	{
		Match match(rules, false);
		Actor& thug = match.game->add_fighter("thug", match.hero().position + b2Vec2(0.9f, 0.0f), false);
		match.run(0.05f);
		thug.get<FighterComponent>()->perform("punch");
		match.press(Dodge);
		check(match.hero().get<FighterComponent>()->evading(), "the dodge evades while it is under way");
		match.run(0.5f);
		check(match.health(match.hero()) == 100.0f, "a punch during the dodge misses");
	}

	// The enemies come and attack
	{
		Match match(rules, true);
		match.run(6.0f);
		check(match.health(match.hero()) < 100.0f, "the enemies walk up to the hero and hit it");
	}

	// The bot wins the arena, the same way with the same seed
	auto fight = [&](uint32_t seed, double& seconds, float& health)
	{
		Match match(rules, true, seed);
		for (int i = 0; i < 60 * 120 && !match.game->won() && !match.game->lost(); ++i)
		{
			match.game->step(kDt, autoplay(*match.game));
		}
		seconds = match.game->timeMs / 1000.0;
		health = match.health(match.hero());
		return match.game->won();
	};
	double seconds = 0.0, again = 0.0;
	float health = 0.0f, healthAgain = 0.0f;
	const bool won = fight(2026, seconds, health);
	const bool wonAgain = fight(2026, again, healthAgain);
	std::printf("  bot: %s in %.1f s with %.0f health\n", won ? "won" : "lost", seconds, health);
	check(won, "the bot beats the two enemies");
	check(wonAgain && seconds == again && health == healthAgain, "the same seed plays the same fight");

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
