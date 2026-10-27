///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Stats.h
// Purpose: An actor's numbers that things change for a while: a stat is (base + the added amounts) times
//          the factors of its modifiers. A boost adds a modifier and takes it away when it ends; one with
//          its own duration runs out by itself. TopDown's components read the factors in Stat (speed,
//          damage, defence); a game names its own.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Actor.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	// The stats TopDown's components read. They are factors: 1 leaves things as the rules say.
	namespace Stat
	{
		inline const std::string speed = "speed";       // times the run speed of the weapon in hand
		inline const std::string damage = "damage";     // times the damage of the actor's weapons
		inline const std::string defence = "defence";   // the damage the actor takes is divided by it
	}

	class Stats
	{
	public:
		using ModifierId = uint32_t;

		void set_base(const std::string& stat, float value) { m_base[stat] = value; }

		// (base + the added amounts) times the factors, in the order the modifiers came; 'fallback' is the base of a
		// stat never set
		float value(const std::string& stat, float fallback = 1.0f) const
		{
			auto base = m_base.find(stat);
			float added = 0.0f;
			float factor = 1.0f;
			bool any = false;
			for (const Modifier& modifier : m_modifiers)
			{
				if (modifier.stat != stat) continue;
				added += modifier.add;
				factor *= modifier.multiply;
				any = true;
			}
			const float from = base != m_base.end() ? base->second : fallback;
			return any ? (from + added) * factor : from;
		}

		// Changes a stat until it is removed ('seconds' 0), or for that long
		ModifierId add_modifier(const std::string& stat, float add, float multiply, float seconds = 0.0f)
		{
			const ModifierId id = m_nextId++;
			m_modifiers.push_back({ id, stat, add, multiply, seconds });
			return id;
		}
		void remove_modifier(ModifierId id)
		{
			std::erase_if(m_modifiers, [id](const Modifier& modifier) { return modifier.id == id; });
		}
		bool has_modifier(ModifierId id) const
		{
			return std::any_of(m_modifiers.begin(), m_modifiers.end(), [id](const Modifier& modifier) { return modifier.id == id; });
		}

		// The modifiers with a duration run out
		void update(float dt)
		{
			for (Modifier& modifier : m_modifiers)
			{
				if (modifier.secondsLeft > 0.0f)
				{
					modifier.secondsLeft -= dt;
					if (modifier.secondsLeft <= 0.0f) modifier.expired = true;
				}
			}
			std::erase_if(m_modifiers, [](const Modifier& modifier) { return modifier.expired; });
		}

	private:
		struct Modifier
		{
			ModifierId id;
			std::string stat;
			float add;
			float multiply;
			float secondsLeft;   // 0: until removed
			bool expired = false;
		};

		std::map<std::string, float> m_base;
		std::vector<Modifier> m_modifiers;
		ModifierId m_nextId = 1;
	};

	// An actor's stats, and how much of each damage type it takes
	class StatsComponent : public Component
	{
	public:
		Stats stats;
		// Times the damage of a type (DamageType) it takes: 0.5 takes half; a type not listed, all of it
		std::map<int, float> resistances;

		void update(TopDownGame& game, float dt) override { (void)game; stats.update(dt); }
	};

	// An actor's stat, or 'fallback' when it has no stats
	inline float stat_of(const Actor& actor, const std::string& stat, float fallback = 1.0f)
	{
		const StatsComponent* stats = actor.get<StatsComponent>();
		return stats ? stats->stats.value(stat, fallback) : fallback;
	}
}
