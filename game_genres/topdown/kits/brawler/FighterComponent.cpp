///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: FighterComponent.cpp
// Purpose: The Brawler kit's fighter - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/kits/brawler/FighterComponent.h"
#include "topdown/Height.h"
#include "topdown/TopDownGame.h"

#include <algorithm>

namespace Pakal::TopDown
{
	namespace
	{
		const float kHurtFlashSeconds = 0.15f;
		const float kKnockedAbove = 0.5f;   // a hitstun this long, or one in the air, throws the body down

		bool down(const Actor& actor)
		{
			const HealthComponent* health = actor.get<HealthComponent>();
			return health && health->is_dead();
		}
	}

	FighterComponent::FighterComponent(std::vector<MoveRules> moves, float runSpeed) :
		m_moves(std::move(moves)),
		m_runSpeed(runSpeed)
	{
		m_action.onPhase = [this](ActionPhase phase)
		{
			if (phase == ActionPhase::Active) m_struck.clear();
			if (phase == ActionPhase::Idle) m_move = nullptr;
		};
	}

	const MoveRules* FighterComponent::find(const std::string& move) const
	{
		auto found = std::find_if(m_moves.begin(), m_moves.end(), [&](const MoveRules& rules) { return rules.name == move; });
		return found != m_moves.end() ? &*found : nullptr;
	}

	bool FighterComponent::perform(const std::string& name)
	{
		const MoveRules* move = find(name);
		const HeightComponent* height = actor().get<HeightComponent>();
		const bool airborne = height && height->airborne();
		if (!move || stunned() || down(actor()) || move->air != airborne || m_action.busy())
		{
			return false;
		}
		const b2Vec2 facing = this->facing();
		if (!m_action.start(move->timing))
		{
			return false;
		}
		m_move = m_action.busy() ? move : nullptr;
		m_facing = facing;   // the move goes the way it started
		if (move->jump)
		{
			if (HeightComponent* body = actor().get<HeightComponent>()) body->jump(move->launch);
		}
		m_view.name = move->name;
		m_view.seconds = move->timing.windup + move->timing.active + move->timing.recovery;
		m_view.loop = false;
		++m_view.serial;
		return true;
	}

	void FighterComponent::set_trigger(bool pulled)
	{
		m_trigger = pulled;
	}

	float FighterComponent::run_speed() const
	{
		if (stunned() || m_action.busy() || blocking() || down(actor()))
		{
			return 0.0f;
		}
		return m_runSpeed * stat_of(actor(), Stat::speed);
	}

	void FighterComponent::stagger(float seconds, const b2Vec2& push)
	{
		m_action.cancel();
		m_move = nullptr;
		m_stun = std::max(m_stun, seconds);
		m_stunTotal = m_stun;
		m_push = push;
		++m_hurtClips;
		m_hurtFlash = kHurtFlashSeconds;
	}

	b2Vec2 FighterComponent::facing() const
	{
		if (m_action.busy()) return m_facing;
		const PlayerControlComponent* control = actor().get<PlayerControlComponent>();
		return control ? control->facing() : m_facing;
	}

	void FighterComponent::face(const b2Vec2& direction)
	{
		b2Vec2 way = direction;
		if (!m_action.busy() && way.Normalize() > 1.0e-4f) m_facing = way;
	}

	void FighterComponent::update(TopDownGame& game, float dt)
	{
		m_hurtFlash = std::max(0.0f, m_hurtFlash - dt);
		CharacterBodyComponent* body = actor().get<CharacterBodyComponent>();
		if (down(actor()))
		{
			m_action.cancel();
			m_move = nullptr;
			if (body) body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		if (m_stun > 0.0f)
		{
			m_requested.clear();   // a button pressed while hit is lost
			// pushed away, slowing down to a stop at the end of the stun
			const float left = m_stun / std::max(m_stunTotal, 1.0e-4f);
			b2Vec2 way = m_push;
			const float speed = way.Normalize() * left;
			if (body) body->move(way, speed);
			m_stun = std::max(0.0f, m_stun - dt);
			return;
		}
		m_action.update(dt);
		if (m_action.busy() && m_move && !m_move->next.empty()
			&& std::find(m_requested.begin(), m_requested.end(), m_chainRoot) != m_requested.end())
		{
			m_queued = find(m_move->next);   // the combo goes on
		}
		else if (!m_action.busy())
		{
			for (const auto& move : m_requested)
			{
				if (perform(move))
				{
					m_chainRoot = move;
					break;
				}
			}
		}
		m_requested.clear();
		// the next move of a combo comes out as the one before it recovers
		if (m_queued && (!m_action.busy() || m_action.phase() == ActionPhase::Recovery))
		{
			const MoveRules* next = m_queued;
			m_queued = nullptr;
			m_action.cancel();
			m_move = nullptr;
			perform(next->name);
		}
		if (blocking())
		{
			if (body) body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		if (!m_action.busy() && m_trigger && !m_moves.empty())
		{
			perform(m_moves.front().name);
		}
		if (m_action.busy())
		{
			if (body) body->move(m_facing, m_move ? m_move->lunge : 0.0f);
			if (m_action.phase() == ActionPhase::Active) strike(game);
		}
	}

	void FighterComponent::strike(TopDownGame& game)
	{
		if (!m_move || m_move->damage <= 0.0f)
		{
			return;   // a move that hurts no one (a dodge)
		}
		const MoveRules& move = *m_move;
		const b2Vec2 center = actor().position + move.reach * m_facing;
		const float from = height_of(actor());
		for (const auto& candidate : game.actors())
		{
			Actor& target = *candidate;
			const FighterComponent* other = target.get<FighterComponent>();
			if (&target == &actor() || target.removed || target.kind != ActorKind::Character || !game.hostile(actor(), target)
				|| (other && other->evading())
				|| !game.is_alive(target) || std::find(m_struck.begin(), m_struck.end(), target.id) != m_struck.end())
			{
				continue;
			}
			if ((target.position - center).Length() > move.radius + game.hit_radius(target))
			{
				continue;
			}
			// the heights the hitbox covers against the target's body
			const HeightComponent* body = target.get<HeightComponent>();
			const float bottom = body ? body->height : 0.0f;
			const float top = bottom + (body ? body->tall : 1.8f);
			if (from + move.high < bottom || from + move.low > top)
			{
				continue;
			}
			m_struck.push_back(target.id);
			b2Vec2 away = target.position - actor().position;
			if (away.Normalize() < 1.0e-4f) away = m_facing;
			// a fighter guarding and facing this one takes a share of it, and is neither stunned nor flung
			const bool guarded = other && other->blocking() && b2Dot(other->facing(), -away) > 0.0f;
			DamageInfo hit{ .amount = move.damage * stat_of(actor(), Stat::damage), .type = DamageType::Melee, .source = actor().id,
				.from = actor().position, .knockback = move.push > 0.0f && !guarded, .hitstun = guarded ? 0.0f : move.hitstun };
			if (guarded) hit.amount *= other->blockFactor;
			if (!game.damage(target, hit))
			{
				continue;
			}
			++m_hits;
			if (guarded)
			{
				continue;
			}
			if (FighterComponent* fighter = target.get<FighterComponent>())
			{
				fighter->stagger(move.hitstun, move.push * away);
			}
			if (move.launch > 0.0f && !move.jump)
			{
				if (HeightComponent* lifted = target.get<HeightComponent>()) lifted->jump(move.launch, true);
			}
		}
	}

	CharacterViewState FighterComponent::view_state() const
	{
		CharacterViewState state;
		state.dead = down(actor());
		state.attacking = m_action.busy();
		state.attackDirection = m_facing;
		state.attackSeconds = m_view.seconds;
		state.hurt = m_hurtFlash > 0.0f;
		state.hurtClips = m_hurtClips;
		const HeightComponent* height = actor().get<HeightComponent>();
		state.knocked = m_stun > 0.0f && (m_stunTotal >= kKnockedAbove || (height && height->airborne()));
		state.knockLeft = m_stun;
		// a move's clip, or the guard's
		state.special = m_action.busy() || blocking();
		state.action = m_action.busy() ? &m_view : (blocking() ? &m_guard : nullptr);
		state.facing = m_facing;
		return state;
	}
}
