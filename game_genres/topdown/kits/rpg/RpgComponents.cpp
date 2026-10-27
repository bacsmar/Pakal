///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RpgComponents.cpp
// Purpose: The RPG kit's components - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/kits/rpg/RpgComponents.h"
#include "topdown/TopDownGame.h"

#include "box2D/PhysicsWorld2D.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		bool down(const Actor& actor)
		{
			const HealthComponent* health = actor.get<HealthComponent>();
			return health && health->is_dead();
		}

		// The characters hostile to 'faction' that are alive, and close enough by 'within'
		template <class Within>
		std::vector<Actor*> foes_of(TopDownGame& game, int faction, Within within)
		{
			std::vector<Actor*> found;
			for (const auto& other : game.actors())
			{
				if (other->removed || other->kind != ActorKind::Character || !game.factions.hostile(faction, game.faction_of(*other))
					|| !game.is_alive(*other))
				{
					continue;
				}
				if (within(*other)) found.push_back(other.get());
			}
			return found;
		}
	}

	// ResourceComponent ----------------------------------------------------------------------------

	void ResourceComponent::set_maximum(const std::string& name, float maximum)
	{
		Pool& pool = m_pools[name];
		const float share = pool.maximum > 0.0f ? pool.current / pool.maximum : 1.0f;
		pool.maximum = maximum;
		pool.current = share * maximum;
	}

	bool ResourceComponent::spend(const std::string& name, float amount)
	{
		auto found = m_pools.find(name);
		if (found == m_pools.end() || found->second.current < amount) return false;
		found->second.current -= amount;
		return true;
	}

	void ResourceComponent::gain(const std::string& name, float amount)
	{
		auto found = m_pools.find(name);
		if (found != m_pools.end()) found->second.current = std::min(found->second.maximum, found->second.current + amount);
	}

	const ResourceComponent::Pool* ResourceComponent::pool(const std::string& name) const
	{
		auto found = m_pools.find(name);
		return found != m_pools.end() ? &found->second : nullptr;
	}

	void ResourceComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		if (down(actor())) return;
		for (auto& [name, pool] : m_pools) pool.current = std::min(pool.maximum, pool.current + pool.regen * dt);
	}

	// ProgressionComponent -------------------------------------------------------------------------

	float ProgressionComponent::attribute(const std::string& name) const
	{
		const StatsComponent* stats = actor().get<StatsComponent>();
		return stats ? stats->stats.value(name, 0.0f) : 0.0f;
	}

	int ProgressionComponent::gain_experience(int amount)
	{
		m_experience += amount;
		int levels = 0;
		StatsComponent* stats = actor().get<StatsComponent>();
		while (m_experience >= experience_to_next())
		{
			m_experience -= experience_to_next();
			++m_level;
			++levels;
			if (stats)
			{
				for (const auto& [name, more] : m_rules.perLevel) stats->stats.set_base(name, attribute(name) + more);
			}
		}
		if (levels > 0) apply();
		return levels;
	}

	void ProgressionComponent::apply()
	{
		StatsComponent* stats = actor().get<StatsComponent>();
		if (!stats) return;
		for (const auto id : m_modifiers) stats->stats.remove_modifier(id);
		m_modifiers.clear();
		float health = m_rules.baseHealth;
		std::map<std::string, float> resources = m_rules.baseResources;
		for (const auto& [attributeName, gives] : m_rules.gives)
		{
			const float points = attribute(attributeName);
			for (const auto& give : gives)
			{
				const float amount = points * give.amount;
				if (give.what == "health") health += amount;
				else if (give.what.rfind("resource:", 0) == 0) resources[give.what.substr(9)] += amount;
				else m_modifiers.push_back(stats->stats.add_modifier(give.what, amount, 1.0f));
			}
		}
		// grown: full again (as a level up does in the genre)
		if (HealthComponent* body = actor().get<HealthComponent>())
		{
			body->set_maximum(health);
			body->heal(health);
		}
		if (ResourceComponent* pools = actor().get<ResourceComponent>())
		{
			for (const auto& [name, maximum] : resources)
			{
				if (!pools->pool(name)) pools->set(name, maximum, 0.0f);
				pools->set_maximum(name, maximum);
				pools->gain(name, maximum);
			}
		}
	}

	void ProgressionComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		(void)dt;
		if (m_applied) return;
		// the first level's attributes, the first time
		m_applied = true;
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			for (const auto& [name, value] : m_rules.attributes) stats->stats.set_base(name, value);
		}
		apply();
	}

	// EquipmentComponent ---------------------------------------------------------------------------

	const ItemRules* EquipmentComponent::item(const std::string& id) const
	{
		auto found = m_catalog.find(id);
		return found != m_catalog.end() ? &found->second : nullptr;
	}

	bool EquipmentComponent::take(const std::string& id)
	{
		if (!item(id) || m_bag.size() >= m_capacity) return false;
		m_bag.push_back(id);
		return true;
	}

	bool EquipmentComponent::equip(const std::string& id)
	{
		const ItemRules* rules = item(id);
		auto inBag = std::find(m_bag.begin(), m_bag.end(), id);
		if (!rules || rules->slot.empty() || inBag == m_bag.end()) return false;
		m_bag.erase(inBag);
		unequip(rules->slot);
		m_worn[rules->slot] = id;
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			auto& modifiers = m_bonuses[rules->slot];
			for (const auto& bonus : rules->bonuses) modifiers.push_back(stats->stats.add_modifier(bonus.stat, bonus.add, bonus.multiply));
		}
		return true;
	}

	bool EquipmentComponent::unequip(const std::string& slot)
	{
		auto found = m_worn.find(slot);
		if (found == m_worn.end()) return false;
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			for (const auto id : m_bonuses[slot]) stats->stats.remove_modifier(id);
		}
		m_bonuses.erase(slot);
		m_bag.push_back(found->second);
		m_worn.erase(found);
		return true;
	}

	// AbilityComponent -----------------------------------------------------------------------------

	AbilityComponent::AbilityComponent(std::vector<AbilityRules> abilities, float runSpeed) :
		m_abilities(std::move(abilities)),
		m_timelines(m_abilities.size()),
		m_runSpeed(runSpeed)
	{
		for (size_t i = 0; i < m_timelines.size(); ++i)
		{
			m_timelines[i].onPhase = [this, i](ActionPhase phase)
			{
				if (static_cast<int>(i) != m_current) return;
				if (phase == ActionPhase::Active) m_effectPending = true;
				if (phase == ActionPhase::Idle) m_current = -1;
			};
		}
	}

	bool AbilityComponent::busy() const
	{
		return m_current >= 0 && m_timelines[static_cast<size_t>(m_current)].busy();
	}

	b2Vec2 AbilityComponent::facing() const
	{
		if (busy()) return m_direction;
		const PlayerControlComponent* control = actor().get<PlayerControlComponent>();
		return control ? control->facing() : m_facing;
	}

	void AbilityComponent::face(const b2Vec2& direction)
	{
		b2Vec2 way = direction;
		if (!busy() && way.Normalize() > 1.0e-4f) m_facing = way;
	}

	bool AbilityComponent::use(const std::string& name, const b2Vec2& direction)
	{
		if (busy() || down(actor())) return false;
		auto found = std::find_if(m_abilities.begin(), m_abilities.end(), [&](const AbilityRules& a) { return a.name == name; });
		if (found == m_abilities.end()) return false;
		const size_t index = static_cast<size_t>(found - m_abilities.begin());
		ActionTimeline& timeline = m_timelines[index];
		const AbilityRules& ability = *found;
		ResourceComponent* pools = actor().get<ResourceComponent>();
		auto pay = [&]() { return ability.cost <= 0.0f || ability.resource.empty() || (pools && pools->spend(ability.resource, ability.cost)); };
		b2Vec2 way = direction;
		if (way.Normalize() < 1.0e-4f) way = facing();
		m_direction = way;
		m_facing = way;
		m_current = static_cast<int>(index);
		m_view.name = ability.name;
		m_view.seconds = ability.timing.windup + ability.timing.active + ability.timing.recovery;
		m_view.loop = false;
		++m_view.serial;
		if (!timeline.start(ability.timing, pay))
		{
			m_current = -1;
			return false;
		}
		return true;
	}

	float AbilityComponent::cooldown_left(const std::string& name) const
	{
		for (size_t i = 0; i < m_abilities.size(); ++i)
		{
			if (m_abilities[i].name == name) return m_timelines[i].cooldown_left();
		}
		return 0.0f;
	}

	float AbilityComponent::cooldown_share(const std::string& name) const
	{
		for (size_t i = 0; i < m_abilities.size(); ++i)
		{
			if (m_abilities[i].name == name)
			{
				const float whole = m_abilities[i].timing.cooldown;
				return whole > 0.0f ? std::clamp(m_timelines[i].cooldown_left() / whole, 0.0f, 1.0f) : 0.0f;
			}
		}
		return 0.0f;
	}

	float AbilityComponent::run_speed() const
	{
		if (down(actor())) return 0.0f;
		const AbilityRules* ability = current();
		if (busy() && ability && !ability->moves) return 0.0f;
		return m_runSpeed * stat_of(actor(), Stat::speed);
	}

	void AbilityComponent::update(TopDownGame& game, float dt)
	{
		if (const HealthComponent* health = actor().get<HealthComponent>())
		{
			if (m_lastHealth >= 0.0f && health->current() < m_lastHealth - 0.001f) ++m_hurtClips;   // the model flinches
			m_lastHealth = health->current();
		}
		if (down(actor()))
		{
			for (auto& timeline : m_timelines) timeline.cancel();
			m_current = -1;
			return;
		}
		const int before = m_current;
		for (auto& timeline : m_timelines) timeline.update(dt);
		if (m_trigger && !busy() && !m_abilities.empty()) use(m_abilities.front().name);
		if (m_effectPending)
		{
			m_effectPending = false;
			const int index = m_current >= 0 ? m_current : before;
			if (index >= 0) apply(game, m_abilities[static_cast<size_t>(index)]);
		}
		// an ability that does not let it walk holds the body still (the control or the brain moved it before)
		const AbilityRules* ability = current();
		if (busy() && ability && !ability->moves)
		{
			if (CharacterBodyComponent* body = actor().get<CharacterBodyComponent>()) body->move(b2Vec2(0.0f, 0.0f), 0.0f);
		}
	}

	void AbilityComponent::apply(TopDownGame& game, const AbilityRules& ability)
	{
		const b2Vec2 at = actor().position;
		const int faction = game.faction_of(actor());
		const DamageInfo hit{ .amount = ability.damage * stat_of(actor(), Stat::damage), .type = ability.damageType, .source = actor().id,
			.from = at, .knockback = ability.push > 0.0f, .hitstun = 0.2f };
		switch (ability.kind)
		{
		case AbilityKind::Arc:
		{
			const float halfArc = 0.5f * ability.arcDegrees * 0.0174532925f;
			for (Actor* foe : foes_of(game, faction, [&](const Actor& other)
			{
				b2Vec2 to = other.position - at;
				const float distance = to.Normalize();
				return distance <= ability.reach + game.hit_radius(other) && (distance < 0.3f || std::acos(std::clamp(b2Dot(to, m_direction), -1.0f, 1.0f)) <= halfArc);
			}))
			{
				game.damage(*foe, hit);
			}
			break;
		}
		case AbilityKind::Area:
			for (Actor* foe : foes_of(game, faction, [&](const Actor& other) { return (other.position - at).Length() <= ability.radius + game.hit_radius(other); }))
			{
				game.damage(*foe, hit);
			}
			game.events.emit(AreaEvent{ at, ability.radius });
			break;
		case AbilityKind::Projectile:
		{
			Actor& missile = game.add_actor(ActorKind::Missile, ability.name);
			missile.position = at + 0.6f * m_direction;
			missile.add<ProjectileComponent>(actor().id, faction, m_direction, ability.speed, ability.reach, hit.amount, ability.radius, ability.damageType);
			game.events.emit(ShotEvent{ missile.position, m_direction });
			break;
		}
		case AbilityKind::Heal:
			if (HealthComponent* health = actor().get<HealthComponent>()) health->heal(ability.heal * health->maximum());
			break;
		}
	}

	CharacterViewState AbilityComponent::view_state() const
	{
		CharacterViewState state;
		const HealthComponent* health = actor().get<HealthComponent>();
		state.dead = down(actor());
		state.hurt = health && health->hurt_recently();
		state.hurtClips = m_hurtClips;
		state.attacking = busy();
		state.attackDirection = m_direction;
		state.attackSeconds = m_view.seconds;
		state.special = busy();
		state.action = busy() ? &m_view : nullptr;
		state.facing = m_direction;
		return state;
	}

	// ProjectileComponent --------------------------------------------------------------------------

	ProjectileComponent::ProjectileComponent(uintptr_t thrower, int faction, const b2Vec2& direction, float speed, float range, float damage,
		float radius, int damageType) :
		m_thrower(thrower), m_faction(faction), m_direction(direction), m_speed(speed), m_left(range), m_damage(damage), m_radius(radius),
		m_damageType(damageType)
	{
	}

	void ProjectileComponent::update(TopDownGame& game, float dt)
	{
		if (m_burst) return;
		const b2Vec2 from = actor().position;
		const float step = std::min(m_left, m_speed * dt);
		const b2Vec2 to = from + step * m_direction;
		// a wall stops it
		const Pakal::RayCastHit wall = game.world().raycast(from, to, game.layers.wall);
		const b2Vec2 end = wall.hit ? wall.point : to;
		// the first foe along the way
		Actor* struck = nullptr;
		float nearest = (end - from).Length();
		for (const auto& other : game.actors())
		{
			if (other->removed || other->kind != ActorKind::Character || !game.factions.hostile(m_faction, game.faction_of(*other)) || !game.is_alive(*other))
			{
				continue;
			}
			const b2Vec2 toCenter = other->position - from;
			const float along = std::clamp(b2Dot(toCenter, m_direction), 0.0f, nearest);
			if ((from + along * m_direction - other->position).Length() <= game.hit_radius(*other) + 0.15f && along <= nearest)
			{
				nearest = along;
				struck = other.get();
			}
		}
		actor().position = from + nearest * m_direction;
		m_left -= nearest;
		if (struck || wall.hit || m_left <= 0.0f)
		{
			if (struck && m_radius <= 0.0f)
			{
				game.damage(*struck, DamageInfo{ .amount = m_damage, .type = m_damageType, .source = m_thrower, .from = from, .hitstun = 0.2f });
			}
			explode(game, actor().position);
		}
	}

	void ProjectileComponent::explode(TopDownGame& game, const b2Vec2& at)
	{
		m_burst = true;
		if (m_radius > 0.0f)
		{
			for (Actor* foe : foes_of(game, m_faction, [&](const Actor& other) { return (other.position - at).Length() <= m_radius + game.hit_radius(other); }))
			{
				game.damage(*foe, DamageInfo{ .amount = m_damage, .type = m_damageType, .source = m_thrower, .from = at, .hitstun = 0.2f });
			}
			game.events.emit(ExplosionEvent{ at, m_radius, 0.0f });
		}
		else
		{
			game.events.emit(ImpactEvent{ at, m_direction, ImpactKind::Enemy });
		}
		actor().removed = true;
	}
}
