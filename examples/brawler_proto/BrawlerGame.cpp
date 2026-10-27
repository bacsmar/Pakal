///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerGame.cpp
// Purpose: The brawler's rules, simulation, enemy brain and bot - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BrawlerGame.h"
#include "topdown/Height.h"

#include "box2D/PhysicsWorld2D.h"
#include "LogMgr.h"

#include <algorithm>

namespace Brawler
{
	namespace
	{
		b2Vec2 point(const nlohmann::json& value, const b2Vec2& fallback)
		{
			if (!value.is_array() || value.size() < 2) return fallback;
			return b2Vec2(value[0].get<float>(), value[1].get<float>());
		}

		MoveRules read_move(const nlohmann::json& data)
		{
			MoveRules move;
			move.name = data.value("name", std::string());
			move.timing.windup = data.value("windup", 0.0f);
			move.timing.active = data.value("active", 0.1f);
			move.timing.recovery = data.value("recovery", 0.2f);
			move.timing.cooldown = data.value("cooldown", 0.0f);
			move.damage = data.value("damage", move.damage);
			move.reach = data.value("reach", move.reach);
			move.radius = data.value("radius", move.radius);
			move.low = data.value("low", move.low);
			move.high = data.value("high", move.high);
			move.hitstun = data.value("hitstun", move.hitstun);
			move.push = data.value("push", move.push);
			move.launch = data.value("launch", move.launch);
			move.lunge = data.value("lunge", move.lunge);
			move.air = data.value("air", move.air);
			move.jump = data.value("jump", move.jump);
			move.evade = data.value("evade", move.evade);
			move.next = data.value("next", move.next);
			return move;
		}
	}

	BrawlerRules BrawlerRules::from_json(const nlohmann::json& data)
	{
		BrawlerRules rules;
		const auto& arena = data.value("arena", nlohmann::json::object());
		rules.halfWidth = arena.value("halfWidth", rules.halfWidth);
		rules.halfDepth = arena.value("halfDepth", rules.halfDepth);
		// (each value kept: a loop over items() of a temporary would outlive it)
		const nlohmann::json fighters = data.value("fighters", nlohmann::json::object());
		for (const auto& [type, fighter] : fighters.items())
		{
			FighterRules f;
			f.health = fighter.value("health", f.health);
			f.runSpeed = fighter.value("runSpeed", f.runSpeed);
			f.radius = fighter.value("radius", f.radius);
			f.tall = fighter.value("tall", f.tall);
			const nlohmann::json moves = fighter.value("moves", nlohmann::json::array());
			for (const auto& move : moves) f.moves.push_back(read_move(move));
			f.attackRange = fighter.value("attackRange", f.attackRange);
			f.blockFactor = fighter.value("blockFactor", f.blockFactor);
			f.waitMin = fighter.value("waitMin", f.waitMin);
			f.waitMax = fighter.value("waitMax", f.waitMax);
			f.attacks = fighter.value("attacks", std::vector<std::string>());
			f.look = fighter.value("look", nlohmann::json::object());
			rules.fighters[type] = std::move(f);
		}
		rules.player = data.value("player", rules.player);
		rules.playerAt = point(data.value("playerAt", nlohmann::json()), rules.playerAt);
		const nlohmann::json enemies = data.value("enemies", nlohmann::json::array());
		for (const auto& enemy : enemies)
		{
			rules.enemies.push_back({ enemy.value("type", std::string()), point(enemy.value("at", nlohmann::json()), b2Vec2(0.0f, 0.0f)) });
		}
		const nlohmann::json buttons = data.value("buttons", nlohmann::json::object());
		for (const auto& [button, moves] : buttons.items())
		{
			rules.buttons[button] = moves.get<std::vector<std::string>>();
		}
		rules.visuals = data.value("visuals", nlohmann::json::object());
		return rules;
	}

	BrawlerGame::BrawlerGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const BrawlerRules& rules, uint32_t seed) :
		m_world(world),
		m_nav(nav),
		m_rules(rules),
		m_random(seed)
	{
		factions.set(heroes, thugs, Relation::Hostile);
	}

	BrawlerGame::~BrawlerGame()
	{
		remove_all();
	}

	void BrawlerGame::start()
	{
		// the arena's walls, a meter thick, around its floor
		const float w = m_rules.halfWidth, d = m_rules.halfDepth;
		m_world.add_static_box(b2Vec2(0.0f, d + 0.5f), b2Vec2(w + 1.0f, 0.5f), layers.wall);
		m_world.add_static_box(b2Vec2(0.0f, -d - 0.5f), b2Vec2(w + 1.0f, 0.5f), layers.wall);
		m_world.add_static_box(b2Vec2(w + 0.5f, 0.0f), b2Vec2(0.5f, d + 1.0f), layers.wall);
		m_world.add_static_box(b2Vec2(-w - 0.5f, 0.0f), b2Vec2(0.5f, d + 1.0f), layers.wall);
		add_player(add_fighter(m_rules.player, m_rules.playerAt, true));
		for (const auto& enemy : m_rules.enemies)
		{
			add_fighter(enemy.type, enemy.at, false);
		}
		join_added();
	}

	Actor& BrawlerGame::add_fighter(const std::string& type, const b2Vec2& at, bool player)
	{
		auto found = m_rules.fighters.find(type);
		static const FighterRules none;
		const FighterRules& rules = found != m_rules.fighters.end() ? found->second : none;
		if (found == m_rules.fighters.end()) LOG_WARNING("[Brawler] no fighter '%s' in the rules", type.c_str());
		Actor& actor = add_actor(ActorKind::Character, type, player ? heroes : thugs);
		actor.position = at;
		m_radius[actor.id] = rules.radius;
		// in this order: the brain or the control decide, the fighter acts (and moves the body during a move), then the body
		actor.add<HealthComponent>(rules.health, 0.0f);
		HeightComponent& height = actor.add<HeightComponent>();
		height.tall = rules.tall;
		const b2Vec2 facing(player ? 1.0f : -1.0f, 0.0f);   // the hero looks toward the enemies, they toward it
		if (player) actor.add<PlayerControlComponent>().face(facing);
		else actor.add<BrainComponent>(rules);
		FighterComponent& fighter = actor.add<FighterComponent>(rules.moves, rules.runSpeed);
		fighter.face(facing);
		fighter.blockFactor = rules.blockFactor;
		actor.add<CharacterBodyComponent>(*this, at, rules.radius);
		return actor;
	}

	void BrawlerGame::step(float dt, const PlayerInput& input)
	{
		timeMs += dt * 1000.0;
		take_inputs(std::span<const PlayerInput>(&input, 1));
		join_added();
		remember_positions();
		// a button pressed: the first of its moves that the player can do now (the first button, if several)
		FighterComponent* fighter = player() ? player()->get<FighterComponent>() : nullptr;
		if (fighter) fighter->set_blocking((input.held & (1u << Block)) != 0);   // held: the guard
		for (int button = 0; fighter && button < ButtonCount; ++button)
		{
			auto moves = m_rules.buttons.find(button_name(button));
			if ((input.pressed & (1u << button)) && moves != m_rules.buttons.end())
			{
				fighter->request(moves->second);
				break;
			}
		}
		update_entities(dt);
		m_world.step(dt);
		join_added();
		remove_removed();
		events.deliver();
	}

	int BrawlerGame::enemies_left() const
	{
		int left = 0;
		for (const auto& actor : actors())
		{
			if (faction_of(*actor) == thugs && is_alive(*actor)) ++left;
		}
		return left;
	}

	bool BrawlerGame::lost() const
	{
		const Actor* hero = player();
		return !hero || is_down(*hero);
	}

	bool BrawlerGame::is_alive(const Actor& actor) const
	{
		const HealthComponent* health = actor.get<HealthComponent>();
		return !actor.removed && health && !health->is_dead();
	}

	float BrawlerGame::hit_radius(const Actor& actor) const
	{
		auto found = m_radius.find(actor.id);
		return found != m_radius.end() ? found->second : 0.3f;
	}

	DamageResult BrawlerGame::apply_damage(Actor& target, const DamageInfo& hit)
	{
		DamageResult result;
		HealthComponent* health = target.get<HealthComponent>();
		if (!health || health->is_dead())
		{
			return result;
		}
		result.taken = health->damage(hit.amount);
		result.hit = result.taken > 0.0f;
		result.killed = result.hit && health->is_dead();
		if (result.killed) LOG_INFO("[Brawler] %s is down", target.name.c_str());
		return result;
	}

	void BrawlerGame::entity_removed(Actor& actor)
	{
		m_radius.erase(actor.id);
		TopDownGame::entity_removed(actor);
	}

	void BrainComponent::update(TopDownGame& game, float dt)
	{
		FighterComponent* fighter = actor().get<FighterComponent>();
		CharacterBodyComponent* body = actor().get<CharacterBodyComponent>();
		if (!fighter || !body || !game.is_alive(actor()) || fighter->stunned() || fighter->busy())
		{
			return;   // the fighter moves the body while hit or in a move
		}
		const Actor* target = game.nearest_player(actor().position, true);
		if (!target)
		{
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		// to one side of the target, the side it is on, facing it: the way fighters stand in a brawler. The other side if
		// one of its own is nearer to that spot and nobody stands at the other.
		float side = actor().position.x < target->position.x ? -1.0f : 1.0f;
		auto spot_of = [&](float which) { return target->position + b2Vec2(which * m_rules.attackRange, 0.0f); };
		auto someone = [&](const b2Vec2& spot, float within)
		{
			for (const auto& other : game.actors())
			{
				if (other.get() != &actor() && game.is_alive(*other) && game.faction_of(*other) == game.faction_of(actor())
					&& (other->position - spot).Length() < within)
				{
					return true;
				}
			}
			return false;
		};
		if (someone(spot_of(side), (actor().position - spot_of(side)).Length()) && !someone(spot_of(-side), 0.8f)) side = -side;
		const b2Vec2 spot = spot_of(side);
		// around the target, not through it: by a point beside it first
		b2Vec2 goal = spot;
		const bool across = (actor().position.x - target->position.x) * side < 0.0f;
		if (across && std::fabs(actor().position.y - target->position.y) < 1.0f)
		{
			goal = b2Vec2(target->position.x, target->position.y + (actor().position.y >= target->position.y ? 1.3f : -1.3f));
		}
		b2Vec2 way = goal - actor().position;
		way.Normalize();
		const float distance = (spot - actor().position).Length();
		fighter->face(target->position - actor().position);
		m_wait = std::max(0.0f, m_wait - dt);
		if (distance > 0.25f)
		{
			body->move(way, m_rules.runSpeed);
			return;
		}
		body->move(b2Vec2(0.0f, 0.0f), 0.0f);
		if (m_wait <= 0.0f && !m_rules.attacks.empty())
		{
			Pakal::RandomStream& random = game.random("sim");
			const size_t pick = std::min(m_rules.attacks.size() - 1, static_cast<size_t>(random.uniform() * static_cast<float>(m_rules.attacks.size())));
			fighter->perform(m_rules.attacks[pick]);
			m_wait = random.uniform(m_rules.waitMin, m_rules.waitMax);
		}
	}

	PlayerInput autoplay(BrawlerGame& game)
	{
		PlayerInput command;
		Actor* hero = game.player();
		if (!hero || !game.is_alive(*hero))
		{
			return command;
		}
		const Actor* nearest = nullptr;
		for (const auto& actor : game.actors())
		{
			if (game.hostile(*hero, *actor) && game.is_alive(*actor)
				&& (!nearest || (actor->position - hero->position).LengthSquared() < (nearest->position - hero->position).LengthSquared()))
			{
				nearest = actor.get();
			}
		}
		if (!nearest)
		{
			return command;
		}
		// in line with it, then close: then jab, and a heavy hit now and then
		b2Vec2 to = nearest->position - hero->position;
		const bool inLine = std::fabs(to.y) < 0.35f;
		const bool close = std::fabs(to.x) < 1.1f;
		if (!inLine || !close)
		{
			b2Vec2 move(close ? 0.0f : to.x, inLine ? 0.0f : to.y);
			if (move.Normalize() > 1.0e-4f) command.move = move;
		}
		else
		{
			command.aimDirection = b2Vec2(to.x > 0.0f ? 1.0f : -1.0f, 0.0f);   // turned to it, standing
			const FighterComponent* fighter = hero->get<FighterComponent>();
			if (fighter && !fighter->busy() && !fighter->stunned())
			{
				command.pressed = 1u << (game.random("bot").uniform() < 0.25f ? Heavy : Light);
			}
		}
		return command;
	}
}
