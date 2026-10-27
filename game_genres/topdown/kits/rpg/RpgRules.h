///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RpgRules.h
// Purpose: The RPG kit's data: items (a slot and the stats they change), abilities (the phases of an
//          action, what they cost and what they do: a swing in an arc, a blow all around, a projectile,
//          a heal), loot tables (what a defeated enemy drops) and how a character grows (experience,
//          levels, attributes and what each attribute gives). All of it reads from JSON.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "topdown/Action.h"
#include "topdown/Damage.h"

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	// One change an item makes to a stat (Stats: added, then multiplied)
	struct StatBonus
	{
		std::string stat;
		float add = 0.0f;
		float multiply = 1.0f;
	};

	struct ItemRules
	{
		std::string id;
		std::string name;
		std::string slot;              // where it is worn ("weapon", "armor", "ring"...); empty: it is not worn
		std::vector<StatBonus> bonuses;
		std::string rarity = "common"; // for the interface
		int value = 0;                 // what it is worth in gold
		static ItemRules from_json(const nlohmann::json& data);
	};

	enum class AbilityKind
	{
		Arc,         // a swing: what is in a cone ahead, within its reach
		Area,        // a blow all around the user, within its radius
		Projectile,  // a missile that flies straight and bursts on the first enemy (or a wall)
		Heal,        // the user gets back a share of its health
	};

	struct AbilityRules
	{
		std::string name;              // also the look's action clip (CharacterViewComponent::Clips::actions)
		AbilityKind kind = AbilityKind::Arc;
		ActionTiming timing;           // the effect happens as the active phase begins; then the cooldown
		std::string resource;          // what it costs (ResourceComponent): "" for nothing
		float cost = 0.0f;
		float damage = 0.0f;           // times the user's damage stat
		int damageType = DamageType::Melee;
		float reach = 1.6f;            // Arc: how far; Projectile: how far it flies
		float arcDegrees = 120.0f;     // Arc: how wide
		float radius = 2.0f;           // Area: around the user; Projectile: its burst (0: only the one it hits)
		float speed = 12.0f;           // Projectile: meters per second
		float heal = 0.0f;             // Heal: share of the user's maximum health
		float push = 0.0f;             // meters per second the ones hit are pushed away (CharacterBodyComponent)
		bool moves = false;            // the user may walk while it is under way
		static AbilityRules from_json(const nlohmann::json& data);
	};

	struct LootTable
	{
		float nothing = 0.5f;          // the chance of dropping no item
		struct Entry { std::string item; float weight = 1.0f; };
		std::vector<Entry> items;
		int goldMin = 0, goldMax = 0;

		// The item it drops ("" for none) and its gold
		std::string roll_item(Pakal::RandomStream& random) const;
		int roll_gold(Pakal::RandomStream& random) const;
		static LootTable from_json(const nlohmann::json& data);
	};

	// How a character grows: the experience each level needs (base, then times 'growth' each level), what each level adds to
	// its attributes, and what each point of an attribute gives (to a stat's added amount, to health, to a resource's maximum)
	struct ProgressionRules
	{
		int experienceBase = 100;
		float experienceGrowth = 1.5f;
		std::map<std::string, float> attributes;        // the first level's ("strength": 10...)
		std::map<std::string, float> perLevel;          // added each level
		struct Gives { std::string what; float amount; };   // what: a stat, "health", or "resource:<name>"
		std::map<std::string, std::vector<Gives>> gives;    // by attribute
		float baseHealth = 50.0f;
		std::map<std::string, float> baseResources;     // "mana": 30

		int experience_for(int level) const;            // to go from 'level' to the next
		static ProgressionRules from_json(const nlohmann::json& data);
	};
}
