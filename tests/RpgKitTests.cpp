///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RpgKitTests.cpp
// Purpose: The RPG kit on a bare TopDown game: resources spent and refilled, attributes that feed the
//          stats and grow with levels, items worn as stat modifiers, abilities (a swing in an arc, a
//          blow all around, a projectile, a heal) with their costs and cooldowns, and loot.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/kits/rpg/RpgComponents.h"
#include "topdown/TopDownGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <memory>

using namespace Pakal::TopDown;

namespace
{
	const float kDt = 1.0f / 60.0f;

	// The least a TopDown game is: health takes the hits
	class TestGame : public TopDownGame
	{
	public:
		TestGame() : m_world(b2Vec2(0.0f, 0.0f)), m_nav(0.5f), m_random(7)
		{
			factions.set(heroes, monsters, Relation::Hostile);
		}
		~TestGame() override { remove_all(); }

		const int heroes = factions.add("heroes");
		const int monsters = factions.add("monsters");

		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const TopDownRules& rules() const override { return m_rules; }
		bool is_alive(const Actor& actor) const override { return !actor.removed && actor.get<HealthComponent>() && !actor.get<HealthComponent>()->is_dead(); }
		float hit_radius(const Actor&) const override { return 0.4f; }
		float random01() override { return m_random["sim"].uniform(); }
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }

		Actor& character(int faction, const b2Vec2& at, float health = 100.0f)
		{
			Actor& actor = add_actor(ActorKind::Character, "c", faction);
			actor.position = at;
			actor.add<StatsComponent>();
			actor.add<HealthComponent>(health, 0.0f);
			return actor;
		}
		void step(float dt)
		{
			join_added();
			remember_positions();
			update_entities(dt);
			m_world.step(dt);
			join_added();
			remove_removed();
			events.deliver();
		}

	protected:
		DamageResult apply_damage(Actor& target, const DamageInfo& hit) override
		{
			DamageResult result;
			if (HealthComponent* health = target.get<HealthComponent>())
			{
				result.taken = health->damage(hit.amount);
				result.hit = result.taken > 0.0f;
				result.killed = result.hit && health->is_dead();
			}
			return result;
		}

	private:
		Pakal::PhysicsWorld2D m_world;
		Pakal::NavGrid2D m_nav;
		TopDownRules m_rules;
		Pakal::RandomStreams m_random;
	};

	float health_of(const Actor& actor) { return actor.get<HealthComponent>()->current(); }
}

int main()
{
	// Unbuffered: the process ends without flushing stdout (a pipe under ctest), and its lines would be lost
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	// Resources: spent only when there is enough, refilled over time
	{
		TestGame game;
		Actor& hero = game.character(game.heroes, b2Vec2(0.0f, 0.0f));
		ResourceComponent& pools = hero.add<ResourceComponent>();
		pools.set("mana", 50.0f, 10.0f);
		check(pools.spend("mana", 30.0f) && !pools.spend("mana", 30.0f), "a resource is spent only when there is enough");
		for (int i = 0; i < 60; ++i) game.step(kDt);
		check(std::fabs(pools.pool("mana")->current - 30.0f) < 0.1f, "and it refills at its rate (20 + 10 a second)");
	}

	// Progression: the attributes feed the stats, the health and the resources, and grow each level
	{
		const ProgressionRules rules = ProgressionRules::from_json(nlohmann::json::parse(R"({
			"experienceBase": 100, "experienceGrowth": 2.0, "baseHealth": 50, "baseResources": { "mana": 20 },
			"attributes": { "strength": 10, "vitality": 10, "intelligence": 5 },
			"perLevel": { "strength": 2, "vitality": 2, "intelligence": 2 },
			"gives": { "strength": { "damage": 0.02 }, "vitality": { "health": 5 }, "intelligence": { "resource:mana": 4 } } })"));
		TestGame game;
		Actor& hero = game.character(game.heroes, b2Vec2(0.0f, 0.0f));
		hero.add<ResourceComponent>();
		ProgressionComponent& growth = hero.add<ProgressionComponent>(rules);
		game.step(kDt);
		check(std::fabs(stat_of(hero, Stat::damage) - 1.2f) < 0.001f && hero.get<HealthComponent>()->maximum() == 100.0f
			&& hero.get<ResourceComponent>()->pool("mana")->maximum == 40.0f, "the attributes give damage, health and mana");
		const int levels = growth.gain_experience(350);   // 100 to level 2, 200 more to level 3
		check(levels == 2 && growth.level() == 3 && growth.experience() == 50 && growth.attribute("strength") == 14.0f,
			"experience brings levels, and each level its attributes");
		check(hero.get<HealthComponent>()->maximum() == 120.0f && health_of(hero) == 120.0f, "a level up raises the health and fills it");
	}

	// Equipment: a worn item's bonuses are modifiers of the stats, gone when it comes off
	{
		std::map<std::string, ItemRules> catalog;
		catalog["axe"] = ItemRules::from_json(nlohmann::json::parse(R"({ "id": "axe", "name": "Axe", "slot": "weapon",
			"bonuses": [ { "stat": "damage", "multiply": 1.5 } ] })"));
		catalog["ring"] = ItemRules::from_json(nlohmann::json::parse(R"({ "id": "ring", "slot": "ring", "bonuses": [ { "stat": "speed", "add": 0.1 } ] })"));
		TestGame game;
		Actor& hero = game.character(game.heroes, b2Vec2(0.0f, 0.0f));
		EquipmentComponent& gear = hero.add<EquipmentComponent>(catalog, 2);
		check(gear.take("axe") && gear.take("ring") && !gear.take("axe") && !gear.take("nothing"), "the bag holds what fits of known items");
		check(gear.equip("axe") && stat_of(hero, Stat::damage) == 1.5f && gear.bag().size() == 1, "wearing an item applies its bonuses");
		check(gear.unequip("weapon") && stat_of(hero, Stat::damage) == 1.0f && gear.bag().size() == 2, "taking it off removes them");
	}

	// Abilities: a swing hits what is ahead, costs and cools down; a blow all around; a projectile; a heal
	{
		const nlohmann::json data = nlohmann::json::parse(R"([
			{ "name": "strike", "kind": "arc", "windup": 0.1, "active": 0.05, "recovery": 0.1, "cooldown": 0.5, "damage": 10, "reach": 1.5, "arcDegrees": 90 },
			{ "name": "whirl", "kind": "area", "windup": 0.1, "active": 0.05, "recovery": 0.1, "cooldown": 2, "damage": 5, "radius": 2.5, "resource": "mana", "cost": 30 },
			{ "name": "bolt", "kind": "projectile", "windup": 0.05, "active": 0.05, "recovery": 0.1, "damage": 8, "reach": 10, "speed": 20, "radius": 0 },
			{ "name": "mend", "kind": "heal", "windup": 0.1, "active": 0.05, "recovery": 0.1, "heal": 0.5 } ])");
		std::vector<AbilityRules> abilities;
		for (const auto& entry : data) abilities.push_back(AbilityRules::from_json(entry));
		TestGame game;
		Actor& hero = game.character(game.heroes, b2Vec2(0.0f, 0.0f));
		hero.add<ResourceComponent>().set("mana", 40.0f, 0.0f);
		AbilityComponent& skills = hero.add<AbilityComponent>(abilities, 4.0f);
		Actor& ahead = game.character(game.monsters, b2Vec2(1.0f, 0.0f));
		Actor& behind = game.character(game.monsters, b2Vec2(-1.0f, 0.0f));
		Actor& far = game.character(game.monsters, b2Vec2(6.0f, 0.3f));
		game.step(kDt);
		check(skills.use("strike", b2Vec2(1.0f, 0.0f)) && !skills.use("strike"), "an ability starts, and not again while it is under way");
		for (int i = 0; i < 20; ++i) game.step(kDt);
		check(health_of(ahead) == 90.0f && health_of(behind) == 100.0f, "a swing hits what is in its arc ahead, not behind");
		check(!skills.use("strike", b2Vec2(1.0f, 0.0f)) && skills.cooldown_share("strike") > 0.0f, "then it cools down");
		check(skills.use("whirl") && !skills.use("whirl"), "a blow all around costs its mana");
		for (int i = 0; i < 20; ++i) game.step(kDt);
		check(health_of(ahead) == 85.0f && health_of(behind) == 95.0f && health_of(far) == 100.0f, "and hits everything within its radius");
		for (int i = 0; i < 150; ++i) game.step(kDt);
		check(!skills.use("whirl"), "without mana for it, it does not start");
		check(skills.use("bolt", b2Vec2(1.0f, 0.05f)), "a projectile is thrown");
		for (int i = 0; i < 60; ++i) game.step(kDt);
		check(health_of(ahead) == 77.0f, "it bursts on the first foe in its way");
		hero.get<HealthComponent>()->damage(60.0f);
		check(skills.use("mend"), "a heal starts");
		for (int i = 0; i < 20; ++i) game.step(kDt);
		check(health_of(hero) == 90.0f, "and gives back half the health");
	}

	// Loot: the same seed drops the same things
	{
		const LootTable table = LootTable::from_json(nlohmann::json::parse(R"({ "nothing": 0.3, "gold": [5, 10],
			"items": [ { "item": "axe", "weight": 1 }, { "item": "ring", "weight": 3 } ] })"));
		Pakal::RandomStreams a(11), b(11);
		int items = 0, rings = 0, goldOk = 0;
		bool same = true;
		for (int i = 0; i < 400; ++i)
		{
			const std::string one = table.roll_item(a["loot"]);
			same = same && one == table.roll_item(b["loot"]);
			if (!one.empty()) ++items;
			if (one == "ring") ++rings;
			const int gold = table.roll_gold(a["loot"]);
			table.roll_gold(b["loot"]);
			if (gold >= 5 && gold <= 10) ++goldOk;
		}
		std::printf("  loot: %d items of 400, %d rings\n", items, rings);
		check(same && items > 240 && items < 320 && rings > items / 2 && goldOk == 400, "loot follows its chances and weights, the same with the same seed");
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
