///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RpgComponents.h
// Purpose: The RPG kit's components: a character's resources (mana, stamina: pools that refill), how
//          it grows (experience, levels, attributes that feed its stats, health and resources), what it
//          carries and wears (items whose bonuses are modifiers of its stats), the abilities it uses
//          (actions with a cost and a cooldown; the hero's PlayerWeapon and its model's
//          CharacterViewSource), and the projectiles some abilities throw (actors of their own).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/kits/rpg/RpgRules.h"
#include "topdown/TopDownComponents.h"

#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	// Pools that are spent and refill (mana, stamina), by name
	class ResourceComponent : public Component
	{
	public:
		struct Pool
		{
			float current = 0.0f, maximum = 0.0f;
			float regen = 0.0f;   // per second
		};

		void set(const std::string& name, float maximum, float regen) { m_pools[name] = { maximum, maximum, regen }; }
		void set_maximum(const std::string& name, float maximum);   // keeps the share it had
		// Spends 'amount' if there is that much
		bool spend(const std::string& name, float amount);
		void gain(const std::string& name, float amount);
		const Pool* pool(const std::string& name) const;
		void update(TopDownGame& game, float dt) override;

	private:
		std::map<std::string, Pool> m_pools;
	};

	// Experience, level and attributes (strength, vitality...: bases of its Stats); what the attributes give is applied to
	// the stats (as modifiers), to the maximum health and to the resources' maximums. Needs a StatsComponent (and uses a
	// HealthComponent and a ResourceComponent if it has them), added before it.
	class ProgressionComponent : public Component
	{
	public:
		explicit ProgressionComponent(const ProgressionRules& rules) : m_rules(rules) {}

		// Adds experience; returns how many levels it went up
		int gain_experience(int amount);
		int level() const { return m_level; }
		int experience() const { return m_experience; }
		int experience_to_next() const { return m_rules.experience_for(m_level); }
		float attribute(const std::string& name) const;
		// Its attributes' effects again (after a level, at the start): health and resources are filled up
		void apply();

		void update(TopDownGame& game, float dt) override;

	private:
		const ProgressionRules& m_rules;
		int m_level = 1;
		int m_experience = 0;
		bool m_applied = false;
		std::vector<uint32_t> m_modifiers;   // of its stats, by its attributes
	};

	// What a character carries and wears: items by id (from a catalog the game keeps); a worn item's bonuses are modifiers of
	// its stats (a StatsComponent, added before it)
	class EquipmentComponent : public Component
	{
	public:
		explicit EquipmentComponent(const std::map<std::string, ItemRules>& catalog, size_t capacity = 24) : m_catalog(catalog), m_capacity(capacity) {}

		// Into the bag; false when it is full or the item is unknown
		bool take(const std::string& item);
		// Wears an item of the bag in its slot (what was there goes back to the bag); false if it cannot be worn
		bool equip(const std::string& item);
		bool unequip(const std::string& slot);
		const std::vector<std::string>& bag() const { return m_bag; }
		const std::map<std::string, std::string>& worn() const { return m_worn; }
		const ItemRules* item(const std::string& id) const;

	private:
		const std::map<std::string, ItemRules>& m_catalog;
		size_t m_capacity;
		std::vector<std::string> m_bag;
		std::map<std::string, std::string> m_worn;            // slot -> item
		std::map<std::string, std::vector<uint32_t>> m_bonuses;   // slot -> its modifiers
	};

	// The abilities a character uses, each an action of its own (its cooldown apart), one under way at a time. Its effect
	// comes as the active phase begins. The trigger (PlayerWeapon) uses the first; the model shows the one under way.
	class AbilityComponent : public Component, public PlayerWeapon, public CharacterViewSource
	{
	public:
		// 'runSpeed': meters per second when it walks
		AbilityComponent(std::vector<AbilityRules> abilities, float runSpeed);

		// Starts an ability toward 'direction' (zero: the way it faces); false while one is under way, cooling down, or not paid
		bool use(const std::string& name, const b2Vec2& direction = b2Vec2(0.0f, 0.0f));
		bool busy() const;
		const AbilityRules* current() const { return m_current >= 0 ? &m_abilities[static_cast<size_t>(m_current)] : nullptr; }
		// Seconds of cooldown left, and its share of the whole (the interface's clock)
		float cooldown_left(const std::string& name) const;
		float cooldown_share(const std::string& name) const;
		const std::vector<AbilityRules>& abilities() const { return m_abilities; }
		// Which way it faces: its control's (the player), else the one its brain sets
		b2Vec2 facing() const;
		void face(const b2Vec2& direction);

		void set_trigger(bool pulled) override { m_trigger = pulled; }
		float run_speed() const override;
		CharacterViewState view_state() const override;
		void update(TopDownGame& game, float dt) override;

	private:
		void apply(TopDownGame& game, const AbilityRules& ability);

		std::vector<AbilityRules> m_abilities;
		std::vector<ActionTimeline> m_timelines;
		float m_runSpeed;
		int m_current = -1;
		bool m_effectPending = false;
		b2Vec2 m_direction = b2Vec2(0.0f, 1.0f);
		b2Vec2 m_facing = b2Vec2(0.0f, 1.0f);
		bool m_trigger = false;
		CharacterAction m_view;
		int m_hurtClips = 0;
		float m_lastHealth = -1.0f;
	};

	// A missile of an ability: an actor of its own that flies straight, stops at walls, and bursts on the first character
	// hostile to the one who threw it (hurting what is within its radius), or at the end of its range
	class ProjectileComponent : public Component
	{
	public:
		ProjectileComponent(uintptr_t thrower, int faction, const b2Vec2& direction, float speed, float range, float damage, float radius, int damageType);
		bool burst() const { return m_burst; }
		void update(TopDownGame& game, float dt) override;

	private:
		void explode(TopDownGame& game, const b2Vec2& at);

		uintptr_t m_thrower;
		int m_faction;
		b2Vec2 m_direction;
		float m_speed, m_left, m_damage, m_radius;
		int m_damageType;
		bool m_burst = false;
	};

	// The gold a character carries, and items a game counts (the RPG kit's pickups go here: TopDownGame::take_pickup)
	class PurseComponent : public Component
	{
	public:
		int gold = 0;
	};
}
