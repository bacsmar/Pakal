///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CollisionLayers.h
// Purpose: Collision layers by name. A game declares its layers ("wall", "player", "enemy"...) and
//          gets each one's bit (a Box2D category) and masks of several, so the engine never writes a
//          game's categories by hand. Up to 16 layers, in the order they are added.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Pakal
{
	class CollisionLayers
	{
	public:
		static constexpr size_t kMaxLayers = 16;

		// The bit of a layer, added the first time (0 when the 16 are taken)
		uint16_t add(std::string_view name)
		{
			if (const uint16_t known = bit(name)) return known;
			if (m_names.size() >= kMaxLayers) return 0;
			m_names.emplace_back(name);
			return static_cast<uint16_t>(1u << (m_names.size() - 1));
		}

		// The bit of a layer, 0 for one not declared
		uint16_t bit(std::string_view name) const
		{
			for (size_t i = 0; i < m_names.size(); ++i)
			{
				if (m_names[i] == name) return static_cast<uint16_t>(1u << i);
			}
			return 0;
		}

		// The bits of several layers together (a ray that stops at walls and enemies)
		uint16_t mask(std::initializer_list<std::string_view> names) const
		{
			uint16_t bits = 0;
			for (const std::string_view name : names) bits = static_cast<uint16_t>(bits | bit(name));
			return bits;
		}

		// The name of a single bit ("" when none)
		std::string name_of(uint16_t bit) const
		{
			for (size_t i = 0; i < m_names.size(); ++i)
			{
				if (bit == (1u << i)) return m_names[i];
			}
			return std::string();
		}

		size_t size() const { return m_names.size(); }

	private:
		std::vector<std::string> m_names;
	};
}
