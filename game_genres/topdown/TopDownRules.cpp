///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownRules.cpp
// Purpose: The rules any top-down action game reads from its data - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/TopDownRules.h"

#include <algorithm>

namespace Pakal::TopDown
{
	const std::string& TopDownRules::sound(const std::string& event) const
	{
		static const std::string none;
		auto own = sounds.find(event);
		if (own != sounds.end()) return own->second;
		auto fallback = defaultSounds.find(event);
		return fallback != defaultSounds.end() ? fallback->second : none;
	}

	int TopDownRules::pick_gift(float roll) const
	{
		if (giftTable.empty()) return 0;
		for (const auto& entry : giftTable)
		{
			if (roll < entry.chance) return entry.consumable;
			roll -= entry.chance;
		}
		return giftTable.back().consumable;
	}

	int TopDownRules::pick_drop(bool random, const std::vector<int>& drops, float rate, float roll, float pick) const
	{
		const std::vector<int>& from = random ? randomPickups : drops;
		if (from.empty() || roll >= rate) return 0;
		const size_t index = std::min(from.size() - 1, static_cast<size_t>(pick * static_cast<float>(from.size())));
		return from[index];
	}
}
