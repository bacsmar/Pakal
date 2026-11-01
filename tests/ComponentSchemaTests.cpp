///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ComponentSchemaTests.cpp
// Purpose: The components a level's object can carry: TopDown's kinds and their fields as JSON, the
//          defaults filled in, a kind replaced by a game, and an object's components added to its actor
//          (a breakable that explodes from Destructible + Explosive, a hidden gift box), the unknown ones
//          reported.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/ComponentSchema.h"
#include "topdown/TopDownComponents.h"
#include "topdown/TopDownGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <cstdio>

using namespace Pakal::TopDown;
using Pakal::Json;

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

	class TestGame : public TopDownGame
	{
	public:
		TestGame() : m_world(b2Vec2(0.0f, 0.0f)), m_nav(0.5f), m_random(7) {}
		~TestGame() override { remove_all(); }
		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const TopDownRules& rules() const override { return m_rules; }
		bool is_alive(const Actor& actor) const override { return !actor.removed; }
		float hit_radius(const Actor&) const override { return 0.4f; }
		float random01() override { return m_random["sim"].uniform(); }
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }

	protected:
		DamageResult apply_damage(Actor&, const DamageInfo&) override { return DamageResult(); }

	private:
		Pakal::PhysicsWorld2D m_world;
		Pakal::NavGrid2D m_nav;
		TopDownRules m_rules;
		Pakal::RandomStreams m_random;
	};
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	ComponentSchema schema;
	ComponentSchema::add_topdown_kinds(schema);

	const Json described = schema.to_json();
	check(described.size() == 3 && described[0]["name"] == "Destructible" && described[0]["fields"][0]["name"] == "health"
		&& described[0]["fields"][0]["type"] == "float" && described[0]["fields"][0]["max"] == 10000.0, "the kinds and their fields as JSON");

	const Json filled = schema.with_defaults("Destructible", Json{ { "health", 7 } });
	check(filled["health"] == 7 && filled["repairSeconds"] == 4.0 && filled["objective"] == false, "the defaults fill what is not given");

	TestGame game;
	Pakal::LevelObject object;
	object.name = "Barrel";
	object.id = 12;
	const Json resolved = Json::parse(R"({ "mesh": "b.glb", "components": {
		"Destructible": { "health": 25 }, "Explosive": { "radius": 4 }, "GiftBox": {}, "Teleporter": {} } })");
	Actor& actor = game.add_actor(ActorKind::Prop, object.name);
	actor.add<PropComponent>();
	PlacedObject placed{ game, actor, object, resolved };
	placed.low = b2Vec2(-1.0f, -1.0f);
	placed.high = b2Vec2(1.0f, 1.0f);
	placed.size = 2.0f;
	const auto unknown = schema.add_components(placed);
	check(unknown == std::vector<std::string>({ "Teleporter" }), "an unknown component is reported");
	const auto* breakable = actor.get<DestructibleComponent>();
	check(breakable && breakable->max_health() == 25.0f && breakable->explosive() && breakable->rules().blastRadius == 4.0f
		&& breakable->rules().blastDamage == 80.0f && breakable->rules().id == 12, "Destructible with Explosive makes one breakable that explodes");
	check(actor.get<GiftBoxComponent>() && !actor.get<PropComponent>()->visible() && !actor.get<PropComponent>()->enabled(),
		"the gift box waits hidden");

	// a game replaces a kind
	bool replaced = false;
	schema.add({ "GiftBox", "the game's", {}, [&](PlacedObject&, const Json&) { replaced = true; } });
	Actor& other = game.add_actor(ActorKind::Prop, "Gift");
	PlacedObject again{ game, other, object, Json::parse(R"({ "components": { "GiftBox": {} } })") };
	schema.add_components(again);
	check(replaced && schema.kinds().size() == 3, "a kind added again replaces the old one");

	if (g_failures == 0) std::printf("ComponentSchemaTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
