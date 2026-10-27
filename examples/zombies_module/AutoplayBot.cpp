///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AutoplayBot.cpp
// Purpose: A simple player for unattended checks - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AutoplayBot.h"
#include "Components.h"
#include "ZombiesGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace Zombies
{
	PlayerInput autoplay_input(ZombiesGame& game, uintptr_t playerId)
	{
		PlayerInput input;
		static const std::string mode = std::getenv("ZOMBIES_AUTOPLAY") ? std::getenv("ZOMBIES_AUTOPLAY") : "";
		Actor* player = game.find(playerId);
		if (!player || mode == "idle")
		{
			return input;
		}
		const b2Vec2 at = player->position;
		auto sees = [&game, &at](const b2Vec2& to) { return !game.world().raycast(at, to, game.layers.wall).hit; };
		auto route_to = [&game, &at](const b2Vec2& goal)
		{
			std::vector<b2Vec2> route;
			b2Vec2 next = goal;
			if (game.nav().find_path(at, goal, route))
			{
				for (const auto& waypoint : route)
				{
					if ((waypoint - at).Length() > 0.4f) { next = waypoint; break; }
				}
			}
			b2Vec2 direction = next - at;
			direction.Normalize();
			return direction;
		};

		const Actor* nearest = nullptr;
		float nearestDistance = 1e9f;
		for (const auto& actor : game.actors())
		{
			const EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
			if (!brain || brain->is_dead()) continue;
			const float distance = (actor->position - at).Length();
			if (distance < nearestDistance) { nearestDistance = distance; nearest = actor.get(); }
		}

		const bool visible = nearest && nearestDistance < 13.0f && sees(nearest->position);
		if (visible)
		{
			input.hasAimPoint = true;
			input.aimPoint = nearest->position;
			input.fire = true;
		}
		if (nearest && nearestDistance < 4.0f)
		{
			// Back away, turning if a wall is behind
			b2Vec2 away = at - nearest->position;
			away.Normalize();
			for (float turn : { 0.0f, 0.8f, -0.8f, 1.6f, -1.6f, 2.4f, -2.4f })
			{
				const b2Vec2 dir(away.x * std::cos(turn) - away.y * std::sin(turn), away.x * std::sin(turn) + away.y * std::cos(turn));
				if (!game.world().raycast(at, at + 1.2f * dir, game.layers.wall | game.layers.playerWall).hit)
				{
					input.move = dir;
					break;
				}
			}
			return input;
		}
		// The gift, then money nearby, then the nearest zombie when none is in sight
		const ConsumableComponent* consumable = player->get<ConsumableComponent>();
		for (const auto& actor : game.actors())
		{
			const GiftBoxComponent* gift = actor->get<GiftBoxComponent>();
			if (gift && gift->armed() && consumable && !consumable->is_active())
			{
				input.move = route_to(actor->position);
				return input;
			}
		}
		for (const auto& actor : game.actors())
		{
			if (actor->kind == ActorKind::Pickup && (actor->position - at).Length() < 7.0f)
			{
				input.move = route_to(actor->position);
				return input;
			}
		}
		if (nearest && !visible)
		{
			input.move = route_to(nearest->position);
		}
		return input;
	}
}
