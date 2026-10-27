///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownComponents.h
// Purpose: The components any top-down action game shares: health, stats, the player's body and
//          control, consumables, escorts, props that break, gift boxes and pickups (a weapon comes from
//          a kit: the Shooter kit's WeaponComponent). They talk to the game
//          only through TopDownGame; what is a game's own goes through its hooks.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Actor.h"
#include "topdown/TopDownRules.h"
#include "topdown/Health.h"
#include "topdown/Stats.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class CharacterController2D;
}

namespace Pakal::TopDown
{
	// What the model shows during a special attack: the look's clip for 'name' (CharacterViewComponent::Clips::actions)
	// over 'seconds' (0: its own length), looped or once, started again when 'serial' changes. No name: the usual clips.
	struct CharacterAction
	{
		std::string name;
		float seconds = 0.0f;
		bool loop = false;
		int serial = 0;
	};

	// What a character's brain shows on its model (an enemy, an NPC): the view reads this instead of the brain itself
	struct CharacterViewState
	{
		bool dead = false;
		bool attacking = false;
		b2Vec2 attackDirection = b2Vec2(0.0f, 1.0f);
		float attackSeconds = 0.0f;        // how long an attack lasts (its clip is stretched over it)
		bool hurt = false;                 // hit a moment ago (the body flashes)
		int hurtClips = 0;                 // how many times its hurt clip started: the model plays it when this changes
		bool knocked = false;              // knocked back...
		float knockLeft = 0.0f;            // ... for this long still
		bool burst = false;                // blown to pieces: no body is left
		float lift = 0.0f, tumble = 0.0f;  // a body flung into the air: meters up, radians turned over
		float sink = 0.0f;                 // dead and sinking into the floor (0..1)
		bool enraged = false;              // its hurt flash is red, not white (berserk zombies)
		// A special attack: its steps have clips of their own, and it faces the way its brain turns it
		bool special = false;
		const CharacterAction* action = nullptr;
		b2Vec2 facing = b2Vec2(0.0f, 1.0f);
	};

	// A component that tells the view how its character looks now (EnemyBrainComponent)
	class CharacterViewSource
	{
	public:
		virtual ~CharacterViewSource() = default;
		virtual CharacterViewState view_state() const = 0;
	};

	// Health of the player and of props (enemies keep theirs in EnemyAI2D)
	class HealthComponent : public Component
	{
	public:
		HealthComponent(float maximum, float invincibleSeconds);

		// Applies a hit. Returns the damage taken: 0 while invincible or dead.
		float damage(float amount);
		void heal(float amount);
		// Back up with all its health, invincible for a while (the player revived)
		void revive(float invincibleSeconds) { m_health.current = m_health.maximum; m_invincibleTimer = invincibleSeconds; }
		bool is_dead() const { return m_health.is_dead(); }
		float current() const { return m_health.current; }
		float maximum() const { return m_health.maximum; }
		// A new maximum (a character grows): it keeps the share of health it had
		void set_maximum(float maximum)
		{
			const float share = m_health.maximum > 0.0f ? m_health.current / m_health.maximum : 1.0f;
			m_health.maximum = maximum;
			m_health.current = share * maximum;
		}
		bool hurt_recently() const { return m_hurtTimer > 0.0f; }

		void update(TopDownGame& game, float dt) override;

	private:
		Health m_health;
		float m_invincibleSeconds;
		float m_invincibleTimer = 0.0f;
		float m_hurtTimer = 0.0f;
	};

	// A character's body (the player, someone escorted, a brawler's fighter): a character controller that walls stop. Its
	// collision layer: the player's unless given (TopDownLayers).
	class CharacterBodyComponent : public Component
	{
	public:
		CharacterBodyComponent(TopDownGame& game, const b2Vec2& at, float radius, uint16_t layer = 0);
		~CharacterBodyComponent() override;

		void move(const b2Vec2& direction, float speed);
		void teleport(const b2Vec2& at);
		// Off: nothing bumps into it (a body on the floor)
		void set_enabled(bool enabled);
		void update(TopDownGame& game, float dt) override;

	private:
		std::unique_ptr<Pakal::CharacterController2D> m_body;
	};

	// A survivor the player escorts: it walks the route's points in order along the walkable grid, waits while the player
	// is far behind (or ahead), and is done at the last point. It needs a HealthComponent and a CharacterBodyComponent added
	// after it.
	class EscortComponent : public Component
	{
	public:
		enum class State { Waiting, Walking, Arrived, Dead };
		explicit EscortComponent(std::vector<b2Vec2> route) : m_route(std::move(route)) {}
		State state() const { return m_state; }
		size_t next_point() const { return m_next; }
		void update(TopDownGame& game, float dt) override;

	private:
		std::vector<b2Vec2> m_route;
		size_t m_next = 0;
		std::vector<b2Vec2> m_path;
		float m_pathTimer = 0.0f;
		State m_state = State::Waiting;
	};

	// What a consumable from the gift box is doing to the player now. Its speed, damage and defence factors are modifiers
	// of the player's stats (StatsComponent) while it lasts.
	class ConsumableComponent : public Component
	{
	public:
		// Starts a consumable. Instant ones (health) end at once; the others last their timer.
		void apply(TopDownGame& game, const ConsumableRules& rules);
		bool is_active() const { return m_active != nullptr; }
		const ConsumableRules* active() const { return m_active; }
		float seconds_left() const { return m_secondsLeft; }

		const MeleeRules* melee() const { return m_active && m_active->melee ? &m_active->meleeRules : nullptr; }
		const GunRules* firearm() const { return m_active && m_active->firearm ? &m_active->gun : nullptr; }

		void update(TopDownGame& game, float dt) override;

	private:
		void end();

		const ConsumableRules* m_active = nullptr;
		float m_secondsLeft = 0.0f;
		std::vector<uint32_t> m_modifiers;   // of the actor's stats, while it lasts
	};

	// What the player holds and uses (the Shooter kit's WeaponComponent, or a game's own): PlayerControlComponent pulls
	// its trigger and walks at its speed
	class PlayerWeapon
	{
	public:
		virtual ~PlayerWeapon() = default;
		virtual void set_trigger(bool pulled) = 0;
		virtual float run_speed() const = 0;   // meters per second
	};

	// How a weapon in the hands is doing, for the character's model to pose (the Shooter kit's WeaponComponent)
	class WeaponPose
	{
	public:
		virtual ~WeaponPose() = default;
		virtual bool firing() const = 0;          // in a fire cycle
		virtual bool swinging() const = 0;        // a swing of a melee weapon
		virtual bool reloading() const = 0;
		virtual float recoil() const = 0;         // 1 right after a shot, back to 0 in a moment
		virtual float reload_seconds() const = 0; // how long a reload takes
		virtual float cycle_seconds() const = 0;  // one fire cycle
	};

	// Turns the player's input into movement, facing and the trigger
	class PlayerControlComponent : public Component
	{
	public:
		void update(TopDownGame& game, float dt) override;

		bool enabled = true;          // EnableControls
		bool moving() const { return m_moving; }
		b2Vec2 facing() const { return m_facing; }
		// Turns it to face this way (where it starts, a cut scene); its input turns it again
		void face(const b2Vec2& direction);
		// A paparazzi's flash: the player walks at 'factor' of its speed for 'seconds' (a modifier of its speed stat with that
		// duration, the one of a flash before it replaced; without stats, its move is scaled meanwhile)
		void slow_down(float factor, float seconds);
		bool slowed() const;

	private:
		bool m_moving = false;
		b2Vec2 m_facing = b2Vec2(0.0f, 1.0f);
		float m_slowFactor = 1.0f;
		float m_slowTimer = 0.0f;
		uint32_t m_slowModifier = 0;   // of the speed stat, while it slows it
	};

	// How a prop is drawn (view/PropMesh: its model): told each time the prop's state changes
	class PropView
	{
	public:
		virtual ~PropView() = default;
		// 'drawn': shown and enabled; 'broken': flattened and dark
		virtual void show_state(bool drawn, bool broken) = 0;
	};

	// A thing placed in the level. Visibility (Show/Hide) and logical state (Enable/Disable) are
	// separate, as in the game; it is drawn only when it is both (by its PropView, if it has one).
	class PropComponent : public Component
	{
	public:
		explicit PropComponent(std::unique_ptr<PropView> view = nullptr);

		void show(bool shown) { m_visible = shown; apply(); }
		void enable(bool enabled) { m_enabled = enabled; apply(); }
		bool visible() const { return m_visible; }
		bool enabled() const { return m_enabled; }
		bool broken() const { return m_broken; }
		// Broken: the model stays (its collision stays too, KeepCollisionDead) but flattened and dark
		void set_broken() { m_broken = true; apply(); }
		void set_repaired() { m_broken = false; apply(); }   // a barricade put back up

	private:
		void apply();

		std::unique_ptr<PropView> m_view;
		bool m_visible = true;
		bool m_enabled = true;
		bool m_broken = false;
	};

	// A prop that breaks under fire: drops, and may explode
	class DestructibleComponent : public Component
	{
	public:
		explicit DestructibleComponent(const PropRules& rules);

		void damage(TopDownGame& game, float amount);
		bool is_broken() const { return m_broken; }
		float health() const { return m_broken ? 0.0f : m_health; }
		float max_health() const { return m_rules.health; }
		const PropRules& rules() const { return m_rules; }
		// Its footprint (an axis-aligned box, level meters)
		void set_box(const b2Vec2& low, const b2Vec2& high) { m_low = low; m_high = high; }
		// Where something at 'from' stands to reach it: the nearest point of its footprint, 'out' meters off it
		b2Vec2 reach_point(const b2Vec2& from, float out) const;
		// A barricade: repaired while the player stands next to it; how much of it is up while broken (0..1)
		float repair_share() const { return m_rules.health > 0.0f ? std::max(0.0f, m_health) / m_rules.health : 0.0f; }
		void update(TopDownGame& game, float dt) override;
		bool explosive() const { return m_rules.explosive; }
		// ReelObject: when it breaks, it also drops this pickup (0: nothing extra)
		void set_hidden_pickup(int pickupId) { m_hiddenPickup = pickupId; }
		// About how wide it is (meters): how big its debris and fire are
		void set_size(float meters) { m_size = meters; }

	private:
		PropRules m_rules;
		int m_hiddenPickup = 0;
		float m_size = 1.0f;
		float m_health;
		bool m_broken = false;
		b2Vec2 m_low = b2Vec2(0.0f, 0.0f), m_high = b2Vec2(0.0f, 0.0f);
	};

	// The gift box: armed by EnableRandomObject, it gives the player one consumable of its table
	class GiftBoxComponent : public Component
	{
	public:
		void arm(bool on) { m_armed = on && !m_opened; }
		bool armed() const { return m_armed; }
		bool opened() const { return m_opened; }
		int given() const { return m_given; }
		void update(TopDownGame& game, float dt) override;

	private:
		bool m_armed = false;
		bool m_opened = false;
		int m_given = 0;
	};

	// Something on the floor (money, a consumable...) until the player takes it or its time runs out. A consumable is
	// applied; anything else goes to the game (TopDownGame::take_pickup).
	class PickupComponent : public Component
	{
	public:
		// 'lifeSeconds' 0: the rules' lifetime (none: it stays)
		explicit PickupComponent(const PickupRules& rules, float lifeSeconds = 0.0f);
		const PickupRules& rules() const { return m_rules; }
		float seconds_left() const { return m_secondsLeft; }
		void update(TopDownGame& game, float dt) override;

	private:
		PickupRules m_rules;
		float m_secondsLeft;
		float m_lying = 0.0f;       // seconds on the floor
	};

}
