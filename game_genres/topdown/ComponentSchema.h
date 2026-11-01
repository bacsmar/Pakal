///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ComponentSchema.h
// Purpose: The components a level's object can carry (in its prefab or its overrides: "components":
//          { "Destructible": { "health": 40 } }), each with its fields (type, default, range, help) and
//          how it is added to the object's actor. The editor's inspector is made from it, pakal-level
//          and agents read it as JSON, and LevelObjects builds a level's objects with it. TopDown's kinds
//          come with add_topdown_kinds; a game adds its own, or replaces one by adding it again.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"
#include "topdown/Actor.h"

#include <box2d/b2_math.h>

#include <functional>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	class TopDownGame;

	// An object of the level as its components see it while they are added
	struct PlacedObject
	{
		TopDownGame& game;
		Actor& actor;
		const Pakal::LevelObject& object;
		const Pakal::Json& resolved;   // its prefab with its overrides (PrefabLibrary::resolve)
		float size = 1.0f;             // meters across its footprint
		b2Vec2 low, high;              // its footprint on the floor (meters)
	};

	struct FieldSchema
	{
		enum class Type { Float, Int, Bool, String, Color, Vec3, Enum, IntList, Asset };
		std::string name;
		Type type = Type::Float;
		Pakal::Json value;                  // the default
		double min = 0.0, max = 0.0;        // a range when max > min
		std::vector<std::string> options;   // Enum's
		std::string help;

		static const char* type_name(Type type);
	};

	struct ComponentKind
	{
		std::string name;
		std::string help;
		std::vector<FieldSchema> fields;
		// Adds it to the object's actor, its fields with their defaults filled in; empty when another kind reads it
		// (Explosive is part of Destructible)
		std::function<void(PlacedObject& placed, const Pakal::Json& fields)> add;
	};

	class ComponentSchema
	{
	public:
		// Adds a kind, or replaces the one of its name
		void add(ComponentKind kind);
		const ComponentKind* find(const std::string& name) const;
		const std::vector<ComponentKind>& kinds() const { return m_kinds; }

		// A kind's fields as given, with the defaults of the ones missing
		Pakal::Json with_defaults(const std::string& kind, const Pakal::Json& given) const;
		// Adds the object's components (resolved["components"]) to its actor; returns the names it does not know
		std::vector<std::string> add_components(PlacedObject& placed) const;

		// The schema as JSON: [{ "name", "help", "fields": [{ "name", "type", "default", "min", "max", "options", "help" }] }]
		Pakal::Json to_json() const;

		// TopDown's own: Destructible, Explosive, GiftBox
		static void add_topdown_kinds(ComponentSchema& schema);

	private:
		std::vector<ComponentKind> m_kinds;
	};
}
