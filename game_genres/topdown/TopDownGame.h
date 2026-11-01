///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownGame.h
// Purpose: What a top-down action game gives its components: the actors, the physics world and the
//          walkable grid, the player's input, damage, bullets, blasts and drops, sounds and the events
//          the presentation draws. The generic components (TopDownComponents.h) see only this; a game
//          implements it and keeps its own rules to itself, answering through the hooks.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Actor.h"
#include "topdown/Damage.h"
#include "topdown/Factions.h"
#include "topdown/TopDownEvents.h"
#include "CollisionLayers.h"
#include "EventBus.h"
#include "SimWorld.h"
#include "RandomStreams.h"
#include "topdown/TopDownRules.h"

#include <box2d/b2_math.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Pakal
{
	class NavGrid2D;
	class PhysicsWorld2D;
}

namespace Pakal::TopDown
{
	// The collision layers the components use, declared by name in the game's CollisionLayers (in this order, so their
	// bits are 1, 2, 4, 8 and 16). A game may add its own after them.
	struct TopDownLayers
	{
		explicit TopDownLayers(Pakal::CollisionLayers& layers) :
			wall(layers.add("wall")),
			player(layers.add("player")),
			enemy(layers.add("enemy")),
			playerWall(layers.add("player_wall")),
			barricade(layers.add("barricade"))
		{
		}

		uint16_t wall;        // stops bodies, bullets, sight and the walkable grid
		uint16_t player;
		uint16_t enemy;       // EnemyAI2D's bodies
		uint16_t playerWall;  // walls that stop only the player
		// Barricades: they stop bodies, but bullets, sight and the walkable grid go through them (enemies walk up to them
		// and break them)
		uint16_t barricade;
	};

	constexpr float kUnitsPerMeter = 100.0f;         // level units are centimeters
	inline float meters(float units) { return units / kUnitsPerMeter; }

	// What the player asks for in one step: from the keyboard and mouse, or from a test
	struct PlayerInput
	{
		b2Vec2 move = b2Vec2(0.0f, 0.0f);   // level axes, length up to 1
		bool hasAimPoint = false;            // aim at a point of the floor (the mouse)
		b2Vec2 aimPoint = b2Vec2(0.0f, 0.0f);
		b2Vec2 aimDirection = b2Vec2(0.0f, 0.0f); // aim this way (keys); zero for none
		bool fire = false;
		// A game's own buttons, one bit each (a brawler's light, heavy, jump: the game names them): down now, and gone down
		// in this step
		uint32_t held = 0;
		uint32_t pressed = 0;
	};

	struct Bullet
	{
		b2Vec2 position;
		b2Vec2 direction;
		float speed;      // meters per second
		float damage;
		float secondsLeft;
		bool hit = false; // stopped by something this step (drawn once more, then dropped)
		bool knockback = false;             // its bullets knock enemies back
		float piercingLeft = 1.0f;          // each body it goes through takes 'piercing' from it
		float piercing = 1.0f;
		std::vector<uintptr_t> pierced;     // the bodies it went through (not hit again)
		bool fromEnemy = false;             // an enemy's shot: it flies through enemies and hits the player
		int faction = Factions::None;       // the shooter's: it pierces the characters hostile to it
		float height = 1.0f;                // meters over the floor: it passes over (or under) a body not at that height
		float blastRadius = 0.0f;           // a grenade: it bursts where it lands (or at the end of its flight), meters...
		float blastDamage = 0.0f;           // ... hurting everything within that much
	};

	// A TopDown game is a SimWorld: it owns the actors (SyncEntities) and steps them
	class TopDownGame : public Pakal::SimWorld
	{
	public:

		virtual Pakal::PhysicsWorld2D& world() = 0;
		// The collision layers by name, and the ones the components use
		Pakal::CollisionLayers collisionLayers;
		const TopDownLayers layers{ collisionLayers };
		virtual Pakal::NavGrid2D& nav() = 0;
		virtual const TopDownRules& rules() const = 0;

		// Actors (the world's entities). Ids are unique and never 0 (0 means "no actor" in ray cast hits); 'kind' is an
		// ActorKind; a character belongs to a faction (FactionComponent). One made during a step joins the others at the
		// next.
		Actor& add_actor(int kind, const std::string& name, int faction = Factions::None)
		{
			Actor& actor = add_entity(kind, name);
			if (faction != Factions::None) actor.add<FactionComponent>(faction);
			return actor;
		}
		const std::vector<std::unique_ptr<Actor>>& actors() const { return entities(); }
		// The players: the characters the people playing move, in the order they joined (player 0 first). A player that
		// leaves the world keeps its place as nullptr, so the others keep their inputs.
		const std::vector<Actor*>& players() const { return m_players; }
		Actor* player(size_t index = 0) const { return index < m_players.size() ? m_players[index] : nullptr; }
		// Makes the actor the next player; returns its index
		size_t add_player(Actor& actor) { m_players.push_back(&actor); return m_players.size() - 1; }
		// The player nearest to a point, or nullptr; 'living' leaves out the ones down (HealthComponent)
		Actor* nearest_player(const b2Vec2& at, bool living) const;
		// A player is down: its HealthComponent is dead
		static bool is_down(const Actor& player);
		// Who is on whose side: the game names its factions and their relations
		Factions factions;
		int faction_of(const Actor& actor) const
		{
			const FactionComponent* faction = actor.get<FactionComponent>();
			return faction ? faction->faction : Factions::None;
		}
		bool hostile(const Actor& a, const Actor& b) const { return factions.hostile(faction_of(a), faction_of(b)); }
		// Still in the fight: not dead, not broken, not gone
		virtual bool is_alive(const Actor& actor) const = 0;
		// How wide an actor is for a melee swing (meters)
		virtual float hit_radius(const Actor& actor) const = 0;

		// What player 'index' asks for in this step (nothing, for one that gave no input)
		const PlayerInput& input(size_t index = 0) const { return index < m_inputs.size() ? m_inputs[index] : m_noInput; }
		// ... and what the player that is this actor asks for (nothing, for an actor that is not a player)
		const PlayerInput& input_for(const Actor& actor) const;
		// The simulation's random numbers: random01() draws from the "sim" stream (0..1). A system that rolls its own
		// decisions (the spawners) takes a stream of its own, so it does not shift the others.
		virtual float random01() = 0;
		virtual Pakal::RandomStream& random(const std::string& stream) = 0;

		// Damages the actor: the hit goes through its resistances and defence (StatsComponent), the game applies it
		// (apply_damage) and a DamageEvent reports it. Returns true if it was alive and took the hit.
		bool damage(Actor& target, const DamageInfo& hit);
		// A plain hit (a script, a test)
		bool damage(Actor& target, float amount) { return damage(target, DamageInfo{ .amount = amount }); }
		// The services of the components that shoot, blow up and drop things. A game without them (a brawler) leaves them
		// as they are: they do nothing.
		// The bullet fired last bursts where it lands (a grenade launcher's): a game that has blasts gives it this
		virtual void make_last_bullet_burst(float radius, float damage) { (void)radius; (void)damage; }
		// A bullet of the shooter's 'faction' (meters, seconds), flying 'height' meters over the floor
		virtual void fire_bullet(const b2Vec2& from, const b2Vec2& direction, float speed, float damage, float range, float piercing = 1.0f, bool knockback = false, int faction = Factions::None, float height = 1.0f)
		{ (void)from; (void)direction; (void)speed; (void)damage; (void)range; (void)piercing; (void)knockback; (void)faction; (void)height; }
		// Damage to what is around 'at' (an explosion), shaking the camera this hard (negative: the rules' explosion)
		virtual void blast(const b2Vec2& at, float radius, float amount, uintptr_t source, float shake = -1.0f)
		{ (void)at; (void)radius; (void)amount; (void)source; (void)shake; }
		// A pickup on the floor, from the rules by game id
		virtual void drop_pickup(int pickupId, const b2Vec2& at) { (void)pickupId; (void)at; }
		// The prop drop rules (PickupSpawn): rolls and drops at most one pickup
		virtual void roll_drop(bool random, const std::vector<int>& drops, float rate, const b2Vec2& at) { (void)random; (void)drops; (void)rate; (void)at; }

		// The game's own answers (none by default)
		// A consumable whose effect is not a stat (an air strike): true if the game ran it
		virtual bool apply_effect(const std::string& effect, Actor& user) { (void)effect; (void)user; return false; }
		// The player takes a pickup that is not a consumable (money, a token): true if the game took it
		virtual bool take_pickup(Actor& taker, const PickupRules& rules) { (void)taker; (void)rules; return false; }
		// A pickup has been lying on the floor this long (the game may send someone for it)
		virtual void pickup_lying(Actor& pickup, float lyingSeconds, float dt) { (void)pickup; (void)lyingSeconds; (void)dt; }

		// What happened, for the rest of the program (TopDownEvents.h and the game's own): the game delivers them at the end
		// of each step, so a listener (sound, effects, the camera, a test) never runs in the middle of one
		Pakal::EventBus events;
		void play(const std::string& sound, const b2Vec2& at) { if (!sound.empty()) events.emit(SoundEvent{ sound, at }); }

	protected:
		// The game applies a hit, already through the target's resistances and defence, to whatever holds its health (an
		// enemy's brain, a prop, a HealthComponent)
		virtual DamageResult apply_damage(Actor& target, const DamageInfo& hit) = 0;

		// The inputs of a step, one per player in order
		void take_inputs(std::span<const PlayerInput> inputs) { m_inputs.assign(inputs.begin(), inputs.end()); }
		// A player that leaves stops being one (a game that overrides this calls it)
		void entity_removed(Actor& actor) override { std::replace(m_players.begin(), m_players.end(), &actor, static_cast<Actor*>(nullptr)); }

	private:
		std::vector<Actor*> m_players;
		std::vector<PlayerInput> m_inputs;
		const PlayerInput m_noInput;
	};
}
