///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ComponentSchema.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/ComponentSchema.h"

#include "topdown/Actor.h"
#include "topdown/TopDownComponents.h"
#include "topdown/TopDownGame.h"

#include <algorithm>

namespace Pakal::TopDown
{
	using Pakal::Json;

	const char* FieldSchema::type_name(Type type)
	{
		switch (type)
		{
		case Type::Float: return "float";
		case Type::Int: return "int";
		case Type::Bool: return "bool";
		case Type::String: return "string";
		case Type::Color: return "color";
		case Type::Vec3: return "vec3";
		case Type::Enum: return "enum";
		case Type::IntList: return "int-list";
		case Type::Asset: return "asset";
		}
		return "?";
	}

	void ComponentSchema::add(ComponentKind kind)
	{
		auto found = std::find_if(m_kinds.begin(), m_kinds.end(), [&](const ComponentKind& k) { return k.name == kind.name; });
		if (found != m_kinds.end()) *found = std::move(kind);
		else m_kinds.push_back(std::move(kind));
	}

	const ComponentKind* ComponentSchema::find(const std::string& name) const
	{
		auto found = std::find_if(m_kinds.begin(), m_kinds.end(), [&](const ComponentKind& k) { return k.name == name; });
		return found != m_kinds.end() ? &*found : nullptr;
	}

	Json ComponentSchema::with_defaults(const std::string& kind, const Json& given) const
	{
		Json fields = given.is_object() ? given : Json::object();
		if (const ComponentKind* k = find(kind))
		{
			for (const auto& field : k->fields)
			{
				if (!fields.contains(field.name)) fields[field.name] = field.value;
			}
		}
		return fields;
	}

	std::vector<std::string> ComponentSchema::add_components(PlacedObject& placed) const
	{
		std::vector<std::string> unknown;
		const Json components = placed.resolved.value("components", Json::object());
		for (auto it = components.begin(); it != components.end(); ++it)
		{
			const ComponentKind* kind = find(it.key());
			if (!kind)
			{
				unknown.push_back(it.key());
				continue;
			}
			if (kind->add) kind->add(placed, with_defaults(it.key(), it.value()));
		}
		return unknown;
	}

	Json ComponentSchema::to_json() const
	{
		Json list = Json::array();
		for (const auto& kind : m_kinds)
		{
			Json fields = Json::array();
			for (const auto& f : kind.fields)
			{
				Json item = { { "name", f.name }, { "type", FieldSchema::type_name(f.type) }, { "default", f.value } };
				if (f.max > f.min) { item["min"] = f.min; item["max"] = f.max; }
				if (!f.options.empty()) item["options"] = f.options;
				if (!f.help.empty()) item["help"] = f.help;
				fields.push_back(std::move(item));
			}
			list.push_back({ { "name", kind.name }, { "help", kind.help }, { "fields", std::move(fields) } });
		}
		return list;
	}

	void ComponentSchema::add_topdown_kinds(ComponentSchema& schema)
	{
		using Type = FieldSchema::Type;
		schema.add({ "Destructible", "Breaks under fire: it drops what it holds, and may explode (Explosive).",
			{
				{ "health", Type::Float, 40.0, 1.0, 10000.0, {}, "Damage it takes before it breaks" },
				{ "deathSound", Type::String, "", 0.0, 0.0, {}, "The sound it breaks with" },
				{ "drops", Type::IntList, Json::array(), 0.0, 0.0, {}, "Pickups it may drop (their ids in the rules)" },
				{ "dropRate", Type::Float, 0.0, 0.0, 1.0, {}, "How likely it drops one" },
				{ "randomDrop", Type::Bool, false, 0.0, 0.0, {}, "One of the drops at random instead of each" },
				{ "repairable", Type::Bool, false, 0.0, 0.0, {}, "A barricade: put back up by standing next to it" },
				{ "repairSeconds", Type::Float, 4.0, 0.1, 60.0, {}, "From nothing to whole" },
				{ "objective", Type::Bool, false, 0.0, 0.0, {}, "The thing to defend: enemies go for it" },
			},
			[](PlacedObject& placed, const Json& f)
			{
				PropRules rules;
				rules.id = placed.object.id;
				rules.name = placed.object.name;
				rules.health = f.value("health", 40.0f);
				rules.deathSound = f.value("deathSound", std::string());
				for (const auto& drop : f.value("drops", Json::array()))
				{
					if (drop.is_number_integer()) rules.drops.push_back(drop.get<int>());
				}
				rules.dropRate = f.value("dropRate", 0.0f);
				rules.randomDrop = f.value("randomDrop", false);
				rules.repairable = f.value("repairable", false);
				rules.repairSeconds = f.value("repairSeconds", 4.0f);
				rules.objective = f.value("objective", false);
				const Json components = placed.resolved.value("components", Json::object());
				if (components.contains("Explosive"))
				{
					const Json explosive = components["Explosive"];
					rules.explosive = true;
					rules.blastRadius = explosive.value("radius", 3.0f);
					rules.blastDamage = explosive.value("damage", 80.0f);
					rules.burningEffect = explosive.value("burningEffect", std::string());
				}
				DestructibleComponent& breakable = placed.actor.add<DestructibleComponent>(rules);
				breakable.set_size(placed.size);
				breakable.set_box(placed.low, placed.high);
			} });
		schema.add({ "Explosive", "Blows up when it breaks (with Destructible), hurting what is near.",
			{
				{ "radius", Type::Float, 3.0, 0.1, 50.0, {}, "Meters the blast reaches" },
				{ "damage", Type::Float, 80.0, 0.0, 10000.0, {}, "Damage at its center" },
				{ "burningEffect", Type::String, "", 0.0, 0.0, {}, "The effect it keeps going once it blew up" },
			},
			{} });
		schema.add({ "GiftBox", "Gives the player one consumable of the gift table when armed.",
			{
				{ "hidden", Type::Bool, true, 0.0, 0.0, {}, "It waits hidden until a script arms it" },
			},
			[](PlacedObject& placed, const Json& f)
			{
				placed.actor.add<GiftBoxComponent>();
				if (f.value("hidden", true))
				{
					if (auto* prop = placed.actor.get<PropComponent>())
					{
						prop->show(false);
						prop->enable(false);
					}
				}
			} });
	}
}
