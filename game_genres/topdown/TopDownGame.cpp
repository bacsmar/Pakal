///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownGame.cpp
// Purpose: What every TopDown game does the same way: a hit goes through the target's resistances and
//          defence before the game applies it, and is reported; the players and their inputs.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/TopDownGame.h"
#include "topdown/Stats.h"
#include "topdown/TopDownComponents.h"

#include <algorithm>

namespace Pakal::TopDown
{
	bool TopDownGame::damage(Actor& target, const DamageInfo& damage)
	{
		if (target.removed)
		{
			return false;
		}
		DamageInfo hit = damage;
		if (const StatsComponent* stats = target.get<StatsComponent>())
		{
			auto resistance = stats->resistances.find(hit.type);
			if (resistance != stats->resistances.end()) hit.amount *= resistance->second;
			hit.amount = hit.amount / std::max(stats->stats.value(Stat::defence), 0.01f);
		}
		const DamageResult result = apply_damage(target, hit);
		if (result.hit)
		{
			events.emit(DamageEvent{ target.id, hit, result });
		}
		return result.hit;
	}

	bool TopDownGame::is_down(const Actor& player)
	{
		const HealthComponent* health = player.get<HealthComponent>();
		return health && health->is_dead();
	}

	Actor* TopDownGame::nearest_player(const b2Vec2& at, bool living) const
	{
		Actor* nearest = nullptr;
		float best = 0.0f;
		for (Actor* player : m_players)
		{
			if (!player || (living && is_down(*player))) continue;
			const float distance = (player->position - at).LengthSquared();
			if (!nearest || distance < best)
			{
				nearest = player;
				best = distance;
			}
		}
		return nearest;
	}

	const PlayerInput& TopDownGame::input_for(const Actor& actor) const
	{
		for (size_t i = 0; i < m_players.size(); ++i)
		{
			if (m_players[i] == &actor) return input(i);
		}
		return m_noInput;
	}
}
