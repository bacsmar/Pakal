///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RpgRules.cpp
// Purpose: The RPG kit's data - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/kits/rpg/RpgRules.h"

#include <nlohmann/json.hpp>

#include <cmath>

namespace Pakal::TopDown
{
	ItemRules ItemRules::from_json(const nlohmann::json& data)
	{
		ItemRules item;
		item.id = data.value("id", std::string());
		item.name = data.value("name", item.id);
		item.slot = data.value("slot", std::string());
		item.rarity = data.value("rarity", item.rarity);
		item.value = data.value("value", 0);
		const nlohmann::json bonuses = data.value("bonuses", nlohmann::json::array());
		for (const auto& bonus : bonuses)
		{
			item.bonuses.push_back({ bonus.value("stat", std::string()), bonus.value("add", 0.0f), bonus.value("multiply", 1.0f) });
		}
		return item;
	}

	AbilityRules AbilityRules::from_json(const nlohmann::json& data)
	{
		AbilityRules ability;
		ability.name = data.value("name", std::string());
		const std::string kind = data.value("kind", std::string("arc"));
		ability.kind = kind == "area" ? AbilityKind::Area : kind == "projectile" ? AbilityKind::Projectile : kind == "heal" ? AbilityKind::Heal : AbilityKind::Arc;
		ability.timing.windup = data.value("windup", 0.1f);
		ability.timing.active = data.value("active", 0.05f);
		ability.timing.recovery = data.value("recovery", 0.2f);
		ability.timing.cooldown = data.value("cooldown", 0.0f);
		ability.resource = data.value("resource", std::string());
		ability.cost = data.value("cost", 0.0f);
		ability.damage = data.value("damage", 0.0f);
		ability.damageType = ability.kind == AbilityKind::Projectile ? DamageType::Blast : DamageType::Melee;
		ability.reach = data.value("reach", ability.reach);
		ability.arcDegrees = data.value("arcDegrees", ability.arcDegrees);
		ability.radius = data.value("radius", ability.radius);
		ability.speed = data.value("speed", ability.speed);
		ability.heal = data.value("heal", 0.0f);
		ability.push = data.value("push", 0.0f);
		ability.moves = data.value("moves", false);
		return ability;
	}

	std::string LootTable::roll_item(Pakal::RandomStream& random) const
	{
		if (items.empty() || random.uniform() < nothing) return std::string();
		float total = 0.0f;
		for (const auto& entry : items) total += entry.weight;
		float pick = random.uniform() * total;
		for (const auto& entry : items)
		{
			pick -= entry.weight;
			if (pick <= 0.0f) return entry.item;
		}
		return items.back().item;
	}

	int LootTable::roll_gold(Pakal::RandomStream& random) const
	{
		if (goldMax <= 0) return 0;
		return goldMin + static_cast<int>(random.uniform() * static_cast<float>(goldMax - goldMin + 1));
	}

	LootTable LootTable::from_json(const nlohmann::json& data)
	{
		LootTable table;
		table.nothing = data.value("nothing", table.nothing);
		const nlohmann::json items = data.value("items", nlohmann::json::array());
		for (const auto& entry : items) table.items.push_back({ entry.value("item", std::string()), entry.value("weight", 1.0f) });
		const nlohmann::json gold = data.value("gold", nlohmann::json::array());
		if (gold.is_array() && gold.size() == 2)
		{
			table.goldMin = gold[0].get<int>();
			table.goldMax = gold[1].get<int>();
		}
		return table;
	}

	int ProgressionRules::experience_for(int level) const
	{
		return static_cast<int>(std::round(experienceBase * std::pow(experienceGrowth, static_cast<float>(std::max(0, level - 1)))));
	}

	ProgressionRules ProgressionRules::from_json(const nlohmann::json& data)
	{
		ProgressionRules rules;
		rules.experienceBase = data.value("experienceBase", rules.experienceBase);
		rules.experienceGrowth = data.value("experienceGrowth", rules.experienceGrowth);
		rules.baseHealth = data.value("baseHealth", rules.baseHealth);
		rules.attributes = data.value("attributes", std::map<std::string, float>());
		rules.perLevel = data.value("perLevel", std::map<std::string, float>());
		rules.baseResources = data.value("baseResources", std::map<std::string, float>());
		const nlohmann::json gives = data.value("gives", nlohmann::json::object());
		for (const auto& [attribute, list] : gives.items())
		{
			for (const auto& [what, amount] : list.items()) rules.gives[attribute].push_back({ what, amount.get<float>() });
		}
		return rules;
	}
}
