///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonGame.cpp
// Purpose: The dungeon's rules, layout, simulation, monster brain and bot - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "LogMgr.h"

#include <algorithm>
#include <cmath>

namespace Dungeon
{
	namespace
	{
		int pick(Pakal::RandomStream& random, int count)
		{
			return std::min(count - 1, static_cast<int>(random.uniform() * static_cast<float>(count)));
		}

		int between(Pakal::RandomStream& random, int low, int high)
		{
			return low + pick(random, high - low + 1);
		}

		bool sees(TopDownGame& game, const b2Vec2& from, const b2Vec2& to)
		{
			return !game.world().raycast(from, to, game.layers.wall).hit;
		}

		const AbilityRules* ability_named(const AbilityComponent& skills, const std::string& name)
		{
			for (const auto& ability : skills.abilities())
			{
				if (ability.name == name) return &ability;
			}
			return nullptr;
		}

		// It can use it now: not cooling down, and paid
		bool ready(const Actor& actor, const AbilityComponent& skills, const std::string& name)
		{
			const AbilityRules* ability = ability_named(skills, name);
			if (!ability || skills.cooldown_left(name) > 0.0f) return false;
			if (ability->cost <= 0.0f || ability->resource.empty()) return true;
			const ResourceComponent* pools = actor.get<ResourceComponent>();
			const ResourceComponent::Pool* pool = pools ? pools->pool(ability->resource) : nullptr;
			return pool && pool->current >= ability->cost;
		}
	}

	b2Vec2 DungeonLayout::room_center(const Room& room) const
	{
		return b2Vec2((static_cast<float>(room.x) + 0.5f * static_cast<float>(room.w)) * cell, (static_cast<float>(room.y) + 0.5f * static_cast<float>(room.h)) * cell);
	}

	DungeonRules DungeonRules::from_json(const nlohmann::json& data)
	{
		DungeonRules rules;
		// (each value kept: a loop over items() of a temporary would outlive it)
		const nlohmann::json dungeon = data.value("dungeon", nlohmann::json::object());
		rules.columns = dungeon.value("columns", rules.columns);
		rules.rows = dungeon.value("rows", rules.rows);
		rules.roomMin = dungeon.value("roomMin", rules.roomMin);
		rules.roomMax = dungeon.value("roomMax", rules.roomMax);
		rules.spacing = dungeon.value("spacing", rules.spacing);
		rules.corridor = dungeon.value("corridor", rules.corridor);
		rules.cell = dungeon.value("cell", rules.cell);
		rules.extraDoors = dungeon.value("extraDoors", rules.extraDoors);
		rules.monstersBase = dungeon.value("monstersBase", rules.monstersBase);
		rules.monstersPerDepth = dungeon.value("monstersPerDepth", rules.monstersPerDepth);
		rules.monsterTypes = dungeon.value("monsters", std::vector<std::string>());
		rules.boss = dungeon.value("boss", std::string());
		rules.chestChance = dungeon.value("chestChance", rules.chestChance);
		rules.chestLoot = dungeon.value("chestLoot", rules.chestLoot);
		rules.chestRolls = dungeon.value("chestRolls", rules.chestRolls);
		rules.hero = data.value("hero", rules.hero);

		const nlohmann::json creatures = data.value("creatures", nlohmann::json::object());
		for (const auto& [type, creature] : creatures.items())
		{
			CreatureRules c;
			c.health = creature.value("health", c.health);
			c.runSpeed = creature.value("runSpeed", c.runSpeed);
			c.radius = creature.value("radius", c.radius);
			const nlohmann::json abilities = creature.value("abilities", nlohmann::json::array());
			for (const auto& ability : abilities) c.abilities.push_back(AbilityRules::from_json(ability));
			c.aggro = creature.value("aggro", c.aggro);
			c.attackRange = creature.value("attackRange", c.attackRange);
			c.shootRange = creature.value("shootRange", c.shootRange);
			c.waitMin = creature.value("waitMin", c.waitMin);
			c.waitMax = creature.value("waitMax", c.waitMax);
			c.experience = creature.value("experience", c.experience);
			c.loot = creature.value("loot", std::string());
			c.boss = creature.value("boss", false);
			c.look = creature.value("look", nlohmann::json::object());
			rules.creatures[type] = std::move(c);
		}
		rules.progression = ProgressionRules::from_json(data.value("progression", nlohmann::json::object()));
		rules.manaRegen = data.value("manaRegen", rules.manaRegen);
		const nlohmann::json items = data.value("items", nlohmann::json::array());
		for (const auto& item : items)
		{
			ItemRules rulesOfItem = ItemRules::from_json(item);
			rules.items[rulesOfItem.id] = std::move(rulesOfItem);
		}
		const nlohmann::json loot = data.value("loot", nlohmann::json::object());
		for (const auto& [name, table] : loot.items()) rules.loot[name] = LootTable::from_json(table);
		rules.startItems = data.value("startItems", std::vector<std::string>());
		rules.buttons = data.value("buttons", std::vector<std::string>());
		rules.visuals = data.value("visuals", nlohmann::json::object());
		return rules;
	}

	// LootComponent --------------------------------------------------------------------------------

	void LootComponent::update(TopDownGame& game, float dt)
	{
		(void)dt;
		Actor* hero = game.nearest_player(actor().position, true);
		if (!hero || actor().removed || (hero->position - actor().position).Length() > 0.9f) return;
		// a component of the dungeon's own, on the dungeon's game
		DungeonGame& dungeon = static_cast<DungeonGame&>(game);
		if (!item.empty())
		{
			EquipmentComponent* gear = hero->get<EquipmentComponent>();
			if (!gear || !gear->take(item)) return;   // the bag is full: it stays there
			const ItemRules* rules = gear->item(item);
			// worn at once when that slot is empty
			if (rules && !rules->slot.empty() && gear->worn().find(rules->slot) == gear->worn().end()) gear->equip(item);
			++dungeon.itemsTaken;
			dungeon.say(rules ? rules->name : item);
		}
		if (gold > 0)
		{
			if (PurseComponent* purse = hero->get<PurseComponent>()) purse->gold += gold;
		}
		actor().removed = true;
	}

	// DungeonGame ----------------------------------------------------------------------------------

	DungeonGame::DungeonGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const DungeonRules& rules, uint32_t seed) :
		m_world(world),
		m_nav(nav),
		m_rules(rules),
		m_random(seed)
	{
		factions.set(heroes, monsters, Relation::Hostile);
	}

	DungeonGame::~DungeonGame()
	{
		remove_all();
	}

	void DungeonGame::start(int heroes, bool monsters)
	{
		make_layout();
		build_walls();
		populate(heroes, monsters);
		join_added();
	}

	Actor& DungeonGame::add_hero()
	{
		// side by side in the first room, a meter apart: the first in the middle, then right, left, right...
		const int index = static_cast<int>(players().size());
		const float side = index == 0 ? 0.0f : static_cast<float>((index % 2 ? 1 : -1) * ((index + 1) / 2));
		const b2Vec2 at = m_layout.room_center(m_layout.rooms.front()) + b2Vec2(side, 0.0f);
		Actor& hero = add_creature(m_rules.hero, at, true);
		add_player(hero);
		return hero;
	}

	// Rooms on a grid of slots, each of a random size in its slot; a random spanning tree of corridors joins them all (and a
	// few more corridors make loops). A corridor goes from one room's center along x, then along y to the other's.
	void DungeonGame::make_layout()
	{
		Pakal::RandomStream& random = m_random["layout"];
		DungeonLayout& layout = m_layout;
		layout.cell = m_rules.cell;
		layout.width = m_rules.columns * m_rules.spacing + 2;
		layout.height = m_rules.rows * m_rules.spacing + 2;
		layout.floor.assign(static_cast<size_t>(layout.width * layout.height), 0);
		auto carve = [&](int x0, int y0, int x1, int y1)
		{
			for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
			{
				for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x)
				{
					if (x > 0 && y > 0 && x < layout.width - 1 && y < layout.height - 1) layout.floor[static_cast<size_t>(y * layout.width + x)] = 1;
				}
			}
		};
		for (int row = 0; row < m_rules.rows; ++row)
		{
			for (int column = 0; column < m_rules.columns; ++column)
			{
				DungeonLayout::Room room;
				room.w = between(random, m_rules.roomMin, m_rules.roomMax);
				room.h = between(random, m_rules.roomMin, m_rules.roomMax);
				const int slack = m_rules.spacing - 1;
				room.x = 1 + column * m_rules.spacing + between(random, 0, std::max(0, slack - room.w));
				room.y = 1 + row * m_rules.spacing + between(random, 0, std::max(0, slack - room.h));
				carve(room.x, room.y, room.x + room.w - 1, room.y + room.h - 1);
				layout.rooms.push_back(room);
			}
		}
		// the corridors: a depth-first walk from the first room over the grid's neighbors, at random
		const int count = static_cast<int>(layout.rooms.size());
		std::vector<std::vector<int>> joined(static_cast<size_t>(count));
		auto join = [&](int a, int b)
		{
			const DungeonLayout::Room& ra = layout.rooms[static_cast<size_t>(a)];
			const DungeonLayout::Room& rb = layout.rooms[static_cast<size_t>(b)];
			const int ax = ra.x + ra.w / 2, ay = ra.y + ra.h / 2, bx = rb.x + rb.w / 2, by = rb.y + rb.h / 2;
			const int wide = std::max(1, m_rules.corridor) - 1;
			carve(ax, ay, bx, ay + wide);
			carve(bx, ay, bx + wide, by);
			joined[static_cast<size_t>(a)].push_back(b);
			joined[static_cast<size_t>(b)].push_back(a);
		};
		auto neighbors = [&](int index)
		{
			std::vector<int> around;
			const int column = index % m_rules.columns, row = index / m_rules.columns;
			if (column > 0) around.push_back(index - 1);
			if (column + 1 < m_rules.columns) around.push_back(index + 1);
			if (row > 0) around.push_back(index - m_rules.columns);
			if (row + 1 < m_rules.rows) around.push_back(index + m_rules.columns);
			return around;
		};
		std::vector<bool> seen(static_cast<size_t>(count), false);
		std::vector<int> stack{ 0 };
		seen[0] = true;
		while (!stack.empty())
		{
			const int at = stack.back();
			std::vector<int> fresh;
			for (int next : neighbors(at))
			{
				if (!seen[static_cast<size_t>(next)]) fresh.push_back(next);
			}
			if (fresh.empty())
			{
				stack.pop_back();
				continue;
			}
			const int next = fresh[static_cast<size_t>(pick(random, static_cast<int>(fresh.size())))];
			seen[static_cast<size_t>(next)] = true;
			join(at, next);
			stack.push_back(next);
		}
		for (int index = 0; index < count; ++index)
		{
			for (int next : neighbors(index))
			{
				const auto& already = joined[static_cast<size_t>(index)];
				if (next > index && std::find(already.begin(), already.end(), next) == already.end() && random.uniform() < m_rules.extraDoors) join(index, next);
			}
		}
		// how deep each room is: corridors from the first
		std::vector<int> queue{ 0 };
		std::vector<bool> reached(static_cast<size_t>(count), false);
		reached[0] = true;
		for (size_t head = 0; head < queue.size(); ++head)
		{
			const int at = queue[head];
			for (int next : joined[static_cast<size_t>(at)])
			{
				if (reached[static_cast<size_t>(next)]) continue;
				reached[static_cast<size_t>(next)] = true;
				layout.rooms[static_cast<size_t>(next)].depth = layout.rooms[static_cast<size_t>(at)].depth + 1;
				queue.push_back(next);
			}
		}
	}

	// A wall on each side of a floor cell that has no floor beyond it; then the walkable grid
	void DungeonGame::build_walls()
	{
		const float cell = m_layout.cell, thick = 0.2f;
		for (int y = 0; y < m_layout.height; ++y)
		{
			for (int x = 0; x < m_layout.width; ++x)
			{
				if (!m_layout.is_floor(x, y)) continue;
				const b2Vec2 c = m_layout.center(x, y);
				if (!m_layout.is_floor(x, y + 1)) m_world.add_static_box(c + b2Vec2(0.0f, 0.5f * cell + thick), b2Vec2(0.5f * cell + thick, thick), layers.wall);
				if (!m_layout.is_floor(x, y - 1)) m_world.add_static_box(c - b2Vec2(0.0f, 0.5f * cell + thick), b2Vec2(0.5f * cell + thick, thick), layers.wall);
				if (!m_layout.is_floor(x + 1, y)) m_world.add_static_box(c + b2Vec2(0.5f * cell + thick, 0.0f), b2Vec2(thick, 0.5f * cell + thick), layers.wall);
				if (!m_layout.is_floor(x - 1, y)) m_world.add_static_box(c - b2Vec2(0.5f * cell + thick, 0.0f), b2Vec2(thick, 0.5f * cell + thick), layers.wall);
			}
		}
		m_nav.build(m_world, b2Vec2(0.0f, 0.0f), b2Vec2(static_cast<float>(m_layout.width) * cell, static_cast<float>(m_layout.height) * cell), 0.4f, layers.wall);
	}

	// The hero in the first room; in each other room, more monsters the deeper it is, and the boss in the deepest
	void DungeonGame::populate(int heroes, bool monsters)
	{
		Pakal::RandomStream& random = m_random["layout"];
		for (int i = 0; i < heroes; ++i) add_hero();
		if (!monsters) return;
		size_t deepest = 0;
		for (size_t i = 0; i < m_layout.rooms.size(); ++i)
		{
			if (m_layout.rooms[i].depth >= m_layout.rooms[deepest].depth) deepest = i;
		}
		for (size_t i = 1; i < m_layout.rooms.size(); ++i)
		{
			const DungeonLayout::Room& room = m_layout.rooms[i];
			int count = m_rules.monstersBase + static_cast<int>(m_rules.monstersPerDepth * static_cast<float>(room.depth));
			std::vector<std::pair<int, int>> used;
			auto free_cell = [&]()
			{
				for (int tries = 0; tries < 20; ++tries)
				{
					const std::pair<int, int> spot(room.x + between(random, 0, room.w - 1), room.y + between(random, 0, room.h - 1));
					if (std::find(used.begin(), used.end(), spot) == used.end())
					{
						used.push_back(spot);
						return m_layout.center(spot.first, spot.second);
					}
				}
				return m_layout.room_center(room);
			};
			if (i == deepest && !m_rules.boss.empty())
			{
				const std::pair<int, int> middle(room.x + room.w / 2, room.y + room.h / 2);
				used.push_back(middle);
				add_creature(m_rules.boss, m_layout.center(middle.first, middle.second), false);
				count /= 2;
			}
			for (int n = 0; n < count && !m_rules.monsterTypes.empty(); ++n)
			{
				const std::string& type = m_rules.monsterTypes[static_cast<size_t>(pick(random, static_cast<int>(m_rules.monsterTypes.size())))];
				add_creature(type, free_cell(), false);
			}
		}
		// the chests, from a stream of their own: the rest of the dungeon stays as the seed made it before there were chests.
		// One stands in the first room (to try them at once), the others in some rooms: in a corner, where no corridor comes in.
		Pakal::RandomStream& chests = m_random["chests"];
		for (size_t i = 0; i < m_layout.rooms.size(); ++i)
		{
			const DungeonLayout::Room& room = m_layout.rooms[i];
			if (i > 0 && chests.uniform() >= m_rules.chestChance) continue;
			const int cx = i == 0 || chests.uniform() < 0.5f ? room.x : room.x + room.w - 1;
			const int cy = i == 0 || chests.uniform() < 0.5f ? room.y + room.h - 1 : room.y;
			Actor& chest = add_actor(ActorKind::Prop, "chest");
			chest.position = m_layout.center(cx, cy);
			chest.add<ChestComponent>();
		}
	}

	Actor& DungeonGame::add_creature(const std::string& type, const b2Vec2& at, bool hero)
	{
		auto found = m_rules.creatures.find(type);
		static const CreatureRules none;
		const CreatureRules& rules = found != m_rules.creatures.end() ? found->second : none;
		if (found == m_rules.creatures.end()) LOG_WARNING("[Dungeon] no creature '%s' in the rules", type.c_str());
		Actor& actor = add_actor(ActorKind::Character, type, hero ? heroes : monsters);
		actor.position = at;
		m_types[actor.id] = type;
		// in this order: the stats and what feeds them, then the brain or the control decide, the abilities act, the body moves
		actor.add<StatsComponent>();
		actor.add<HealthComponent>(rules.health, 0.0f);
		if (hero)
		{
			actor.add<ResourceComponent>().set("mana", 1.0f, m_rules.manaRegen);   // its maximum comes from its attributes
			actor.add<ProgressionComponent>(m_rules.progression);
			EquipmentComponent& gear = actor.add<EquipmentComponent>(m_rules.items, 16);
			for (const auto& item : m_rules.startItems)
			{
				if (gear.take(item)) gear.equip(item);
			}
			actor.add<PurseComponent>();
			actor.add<PlayerControlComponent>().face(b2Vec2(0.0f, 1.0f));
		}
		else
		{
			actor.add<MonsterBrainComponent>(rules);
		}
		actor.add<AbilityComponent>(rules.abilities, rules.runSpeed).face(b2Vec2(0.0f, -1.0f));
		actor.add<CharacterBodyComponent>(*this, at, rules.radius, hero ? layers.player : layers.enemy);
		return actor;
	}

	b2Vec2 DungeonGame::auto_aim(const Actor& hero)
	{
		const float reach = 9.0f;
		const Actor* nearest = nullptr;
		float best = reach;
		for (const auto& actor : actors())
		{
			if (actor->removed || actor->kind != ActorKind::Character || !hostile(hero, *actor) || !is_alive(*actor)) continue;
			const float distance = (actor->position - hero.position).Length();
			if (distance < best && !m_world.raycast(hero.position, actor->position, layers.wall).hit)
			{
				best = distance;
				nearest = actor.get();
			}
		}
		if (nearest) return nearest->position - hero.position;
		const PlayerControlComponent* control = hero.get<PlayerControlComponent>();
		return control ? control->facing() : b2Vec2(0.0f, 1.0f);
	}

	void DungeonGame::step(float dt, std::span<const PlayerInput> inputs)
	{
		timeMs += dt * 1000.0;
		take_inputs(inputs);
		join_added();
		remember_positions();
		// a button pressed: its ability, toward where that player aims (the first button, if several)
		for (size_t index = 0; index < players().size(); ++index)
		{
			Actor* hero = player(index);
			AbilityComponent* skills = hero ? hero->get<AbilityComponent>() : nullptr;
			const PlayerInput& input = this->input(index);
			for (size_t button = 0; skills && button < m_rules.buttons.size(); ++button)
			{
				if (input.pressed & (1u << button))
				{
					b2Vec2 way = input.hasAimPoint ? input.aimPoint - hero->position : input.aimDirection;
					// not aimed (a touch screen's attack button): at the nearest monster in reach, or where the hero faces
					if (way.LengthSquared() < 1e-6f) way = auto_aim(*hero);
					skills->use(m_rules.buttons[button], way);
					break;
				}
			}
		}
		update_entities(dt);
		m_world.step(dt);
		join_added();
		remove_removed();
		events.deliver();
	}

	const CreatureRules* DungeonGame::creature(const Actor& actor) const
	{
		auto type = m_types.find(actor.id);
		if (type == m_types.end()) return nullptr;
		auto found = m_rules.creatures.find(type->second);
		return found != m_rules.creatures.end() ? &found->second : nullptr;
	}

	int DungeonGame::monsters_left() const
	{
		int left = 0;
		for (const auto& actor : actors())
		{
			if (actor->kind == ActorKind::Character && faction_of(*actor) == monsters && is_alive(*actor)) ++left;
		}
		return left;
	}

	// every hero is down (a server waiting for players has not lost)
	bool DungeonGame::lost() const
	{
		bool any = false;
		for (const Actor* hero : players())
		{
			if (!hero) continue;
			any = true;
			if (!is_down(*hero)) return false;
		}
		return any;
	}

	bool DungeonGame::is_alive(const Actor& actor) const
	{
		const HealthComponent* health = actor.get<HealthComponent>();
		return !actor.removed && health && !health->is_dead();
	}

	float DungeonGame::hit_radius(const Actor& actor) const
	{
		const CreatureRules* rules = creature(actor);
		return rules ? rules->radius : 0.3f;
	}

	DamageResult DungeonGame::apply_damage(Actor& target, const DamageInfo& hit)
	{
		DamageResult result;
		HealthComponent* health = target.get<HealthComponent>();
		if (!health || health->is_dead()) return result;
		result.taken = health->damage(hit.amount);
		result.hit = result.taken > 0.0f;
		result.killed = result.hit && health->is_dead();
		if (!result.killed || faction_of(target) != monsters) return result;
		// a monster defeated: the hero learns from it, and it drops what it had
		++kills;
		const CreatureRules* rules = creature(target);
		if (!rules) return result;
		// the hero that struck it learns from it (or the first, for a hit from nobody)
		Actor* hero = static_cast<Actor*>(find(hit.source));
		if (!hero || !hero->get<ProgressionComponent>()) hero = player();
		if (hero)
		{
			if (ProgressionComponent* growth = hero->get<ProgressionComponent>())
			{
				if (growth->gain_experience(rules->experience) > 0) say("LEVEL " + std::to_string(growth->level()));
			}
		}
		drop_loot(rules->loot, target.position);
		if (rules->boss)
		{
			m_bossDown = true;
			LOG_INFO("[Dungeon] the boss is down after %.1f s", timeMs / 1000.0);
		}
		return result;
	}

	void DungeonGame::drop_loot(const std::string& tableName, const b2Vec2& at, int rolls, float scatter)
	{
		auto table = m_rules.loot.find(tableName);
		if (table == m_rules.loot.end()) return;
		Pakal::RandomStream& random = m_random["loot"];
		auto near = [&]() { return at + b2Vec2(random.uniform(-scatter, scatter), random.uniform(-scatter, scatter)); };
		for (int roll = 0; roll < rolls; ++roll)
		{
			const int gold = table->second.roll_gold(random);
			const std::string item = table->second.roll_item(random);
			if (gold > 0) add_actor(ActorKind::Pickup, "gold").add<LootComponent>(gold, std::string()).actor().position = near();
			if (!item.empty()) add_actor(ActorKind::Pickup, item).add<LootComponent>(0, item).actor().position = near();
		}
	}

	void DungeonGame::open_chest(Actor& chest)
	{
		// the loot lands in front of it, away from the wall it may stand by
		drop_loot(m_rules.chestLoot, chest.position, m_rules.chestRolls, 0.7f);
		++chestsOpened;
		say("A CHEST");
	}

	std::vector<uint8_t> DungeonGame::gear_command(GearCommand command, const std::string& what)
	{
		Pakal::ByteWriter out;
		out.u8(static_cast<uint8_t>(command));
		out.string(what);
		return out.data();
	}

	void DungeonGame::apply_gear_command(Actor& hero, Pakal::ByteReader& in)
	{
		const GearCommand command = static_cast<GearCommand>(in.u8());
		const std::string what = in.string();
		EquipmentComponent* gear = hero.get<EquipmentComponent>();
		if (!in.ok() || !gear || is_down(hero)) return;
		if (command == GearCommand::Wear) gear->equip(what);
		else if (command == GearCommand::TakeOff) gear->unequip(what);
	}

	// ChestComponent -------------------------------------------------------------------------------

	void ChestComponent::update(TopDownGame& game, float dt)
	{
		(void)dt;
		if (opened) return;
		Actor* hero = game.nearest_player(actor().position, true);
		if (!hero || (hero->position - actor().position).Length() > 1.1f) return;
		opened = true;
		static_cast<DungeonGame&>(game).open_chest(actor());
	}

	CharacterViewState ChestComponent::view_state() const
	{
		CharacterViewState state;
		state.dead = opened;
		return state;
	}

	void DungeonGame::entity_removed(Actor& actor)
	{
		m_types.erase(actor.id);
		TopDownGame::entity_removed(actor);
	}

	// MonsterBrainComponent ------------------------------------------------------------------------

	b2Vec2 follow_path(TopDownGame& game, const b2Vec2& from, const b2Vec2& goal, std::vector<b2Vec2>& path, size_t& next, float& repath, float dt)
	{
		repath -= dt;
		if (repath <= 0.0f || next >= path.size())
		{
			repath = 0.5f;
			path.clear();
			next = 0;
			game.nav().find_path(from, goal, path);
		}
		while (next < path.size() && (path[next] - from).Length() < 0.35f) ++next;
		b2Vec2 way = (next < path.size() ? path[next] : goal) - from;
		if (way.Normalize() < 0.05f) return b2Vec2(0.0f, 0.0f);
		return way;
	}

	void MonsterBrainComponent::update(TopDownGame& game, float dt)
	{
		CharacterBodyComponent* body = actor().get<CharacterBodyComponent>();
		AbilityComponent* skills = actor().get<AbilityComponent>();
		if (!body || !skills) return;
		if (!game.is_alive(actor()))
		{
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			body->set_enabled(false);   // a body on the floor: nobody bumps into it
			return;
		}
		const Actor* target = game.nearest_player(actor().position, true);
		if (!target)
		{
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		b2Vec2 to = target->position - actor().position;
		const float distance = to.Length();
		const bool inSight = sees(game, actor().position, target->position);
		if (!m_awake)
		{
			m_awake = distance <= m_rules.aggro && inSight;
			if (!m_awake)
			{
				body->move(b2Vec2(0.0f, 0.0f), 0.0f);
				return;
			}
		}
		m_wait = std::max(0.0f, m_wait - dt);
		if (skills->busy()) return;   // the ability holds the body
		skills->face(to);
		const float reach = m_rules.attackRange + game.hit_radius(*target);
		Pakal::RandomStream& random = game.random("sim");
		// ranged: from afar, in sight, with its projectile; too close, it steps back
		const AbilityRules* shot = nullptr;
		for (const auto& ability : skills->abilities())
		{
			if (ability.kind == AbilityKind::Projectile) shot = &ability;
		}
		if (shot && m_rules.shootRange > 0.0f && inSight && distance <= m_rules.shootRange)
		{
			if (distance < m_rules.attackRange)
			{
				b2Vec2 away = -to;
				away.Normalize();
				body->move(away, m_rules.runSpeed);
				return;
			}
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			if (m_wait <= 0.0f && skills->use(shot->name, to)) m_wait = random.uniform(m_rules.waitMin, m_rules.waitMax);
			return;
		}
		if (!shot && distance <= reach && inSight)
		{
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			if (m_wait > 0.0f) return;
			// one of its blows that is ready, at random
			std::vector<const AbilityRules*> choices;
			for (const auto& ability : skills->abilities())
			{
				if (ability.kind != AbilityKind::Projectile && skills->cooldown_left(ability.name) <= 0.0f) choices.push_back(&ability);
			}
			if (!choices.empty() && skills->use(choices[static_cast<size_t>(pick(random, static_cast<int>(choices.size())))]->name, to))
			{
				m_wait = random.uniform(m_rules.waitMin, m_rules.waitMax);
			}
			return;
		}
		body->move(follow_path(game, actor().position, target->position, m_path, m_next, m_repath, dt), m_rules.runSpeed);
	}

	// DungeonBot -----------------------------------------------------------------------------------

	PlayerInput DungeonBot::think(DungeonGame& game, float dt, size_t index)
	{
		PlayerInput command;
		Actor* hero = game.player(index);
		const AbilityComponent* skills = hero ? hero->get<AbilityComponent>() : nullptr;
		if (!hero || !skills || !game.is_alive(*hero)) return command;
		// it wears the most valuable item it has for each slot, as a player would from the inventory
		if (EquipmentComponent* gear = hero->get<EquipmentComponent>())
		{
			for (const auto& id : gear->bag())
			{
				const ItemRules* item = gear->item(id);
				auto worn = item ? gear->worn().find(item->slot) : gear->worn().end();
				const ItemRules* wearing = worn != gear->worn().end() ? gear->item(worn->second) : nullptr;
				if (item && !item->slot.empty() && (!wearing || item->value > wearing->value))
				{
					gear->equip(id);
					break;   // the bag changed
				}
			}
		}
		const auto& buttons = game.rules().buttons;
		auto press = [&](const std::string& name)
		{
			for (size_t i = 0; i < buttons.size(); ++i)
			{
				if (buttons[i] == name && ready(*hero, *skills, name))
				{
					command.pressed = 1u << i;
					return true;
				}
			}
			return false;
		};
		// low: a heal first
		const HealthComponent* health = hero->get<HealthComponent>();
		if (health && health->current() < 0.45f * health->maximum() && !skills->busy() && press("heal")) return command;
		// the nearest monster, and how many are close
		const Actor* nearest = nullptr;
		int close = 0;
		for (const auto& actor : game.actors())
		{
			if (actor->kind != ActorKind::Character || !game.hostile(*hero, *actor) || !game.is_alive(*actor)) continue;
			const float distance = (actor->position - hero->position).Length();
			if (distance < 2.5f) ++close;
			if (!nearest || distance < (nearest->position - hero->position).Length()) nearest = actor.get();
		}
		// loot in sight when no monster is near
		const Actor* loot = nullptr;
		for (const auto& actor : game.actors())
		{
			if (actor->kind == ActorKind::Pickup && !actor->removed && (actor->position - hero->position).Length() < 7.0f
				&& (!loot || (actor->position - hero->position).Length() < (loot->position - hero->position).Length()))
			{
				loot = actor.get();
			}
		}
		const float toNearest = nearest ? (nearest->position - hero->position).Length() : 1.0e9f;
		if (loot && toNearest > 5.0f)
		{
			command.move = follow_path(game, hero->position, loot->position, m_path, m_next, m_repath, dt);
			return command;
		}
		if (!nearest) return command;
		const bool inSight = sees(game, hero->position, nearest->position);
		command.hasAimPoint = true;
		command.aimPoint = nearest->position;
		if (!skills->busy())
		{
			if (close >= 3 && press("whirlwind")) return command;
			if (inSight && toNearest > 3.0f && toNearest < 9.0f && press("fireball")) return command;
		}
		if (toNearest <= 1.4f + game.hit_radius(*nearest) && inSight)
		{
			command.fire = true;   // the trigger: its first ability, the strike
			return command;
		}
		command.move = follow_path(game, hero->position, nearest->position, m_path, m_next, m_repath, dt);
		return command;
	}
}
