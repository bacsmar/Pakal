///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Factions.h
// Purpose: Who is on whose side. A game names its factions and says which are hostile or allied (the
//          rest are neutral); a character belongs to one (FactionComponent). Weapons, bullets and
//          brains ask whether two actors are hostile instead of what kind of actor each one is.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Actor.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Pakal::TopDown
{
	enum class Relation
	{
		Neutral,
		Allied,
		Hostile,
	};

	class Factions
	{
	public:
		static constexpr int None = -1;   // no faction: neutral to everyone

		// A faction by name (the one it already is, if added before)
		int add(const std::string& name)
		{
			const int found = find(name);
			if (found != None) return found;
			m_names.push_back(name);
			return static_cast<int>(m_names.size()) - 1;
		}
		int find(const std::string& name) const
		{
			auto found = std::find(m_names.begin(), m_names.end(), name);
			return found != m_names.end() ? static_cast<int>(found - m_names.begin()) : None;
		}
		const std::string& name(int faction) const
		{
			static const std::string none;
			return faction >= 0 && faction < static_cast<int>(m_names.size()) ? m_names[static_cast<size_t>(faction)] : none;
		}
		size_t size() const { return m_names.size(); }

		// Both ways
		void set(int a, int b, Relation relation)
		{
			m_relations[{ a, b }] = relation;
			m_relations[{ b, a }] = relation;
		}
		// A faction is allied with itself; one without a faction, and two never set, are neutral
		Relation relation(int a, int b) const
		{
			if (a == None || b == None) return Relation::Neutral;
			if (a == b) return Relation::Allied;
			auto found = m_relations.find({ a, b });
			return found != m_relations.end() ? found->second : Relation::Neutral;
		}
		bool hostile(int a, int b) const { return relation(a, b) == Relation::Hostile; }

	private:
		std::vector<std::string> m_names;
		std::map<std::pair<int, int>, Relation> m_relations;
	};

	// The faction a character belongs to (TopDownGame::add_actor gives it)
	class FactionComponent : public Component
	{
	public:
		explicit FactionComponent(int faction) : faction(faction) {}
		int faction;
	};
}
