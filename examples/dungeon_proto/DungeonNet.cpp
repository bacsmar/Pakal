///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonNet.cpp
// Purpose: The dungeon played together over the network - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonNet.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "LogMgr.h"

#include <algorithm>
#include <chrono>

using namespace Pakal;
using namespace Pakal::TopDown::Net;

namespace Dungeon
{
	std::vector<uint8_t> game_info(uint32_t seed)
	{
		ByteWriter out;
		out.u32(seed);
		return out.data();
	}

	bool read_game_info(const std::vector<uint8_t>& data, uint32_t& seed)
	{
		ByteReader in(data);
		seed = in.u32();
		return in.ok();
	}

	void write_own(ByteWriter& out, const Actor& hero, const DungeonRules& rules)
	{
		const HealthComponent* health = hero.get<HealthComponent>();
		out.f32(health ? health->current() : 0.0f);
		out.f32(health ? health->maximum() : 1.0f);
		const ResourceComponent* pools = hero.get<ResourceComponent>();
		const ResourceComponent::Pool* mana = pools ? pools->pool("mana") : nullptr;
		out.f32(mana ? mana->current : 0.0f);
		out.f32(mana ? mana->maximum : 1.0f);
		const ProgressionComponent* growth = hero.get<ProgressionComponent>();
		out.varuint(growth ? static_cast<uint64_t>(growth->level()) : 1);
		out.varuint(growth ? static_cast<uint64_t>(growth->experience()) : 0);
		out.varuint(growth ? static_cast<uint64_t>(growth->experience_to_next()) : 1);
		const PurseComponent* purse = hero.get<PurseComponent>();
		out.varuint(purse ? static_cast<uint64_t>(purse->gold) : 0);
		const AbilityComponent* skills = hero.get<AbilityComponent>();
		out.u8(static_cast<uint8_t>(rules.buttons.size()));
		uint32_t unpaid = 0;
		for (size_t i = 0; i < rules.buttons.size(); ++i)
		{
			out.u8(static_cast<uint8_t>(std::lround((skills ? skills->cooldown_share(rules.buttons[i]) : 0.0f) * 255.0f)));
			for (const auto& ability : skills ? skills->abilities() : std::vector<AbilityRules>())
			{
				if (ability.name == rules.buttons[i] && ability.cost > 0.0f && (!mana || mana->current < ability.cost)) unpaid |= 1u << i;
			}
		}
		out.varuint(unpaid);
		const EquipmentComponent* gear = hero.get<EquipmentComponent>();
		out.u8(static_cast<uint8_t>(gear ? gear->worn().size() : 0));
		if (gear)
		{
			for (const auto& [slot, id] : gear->worn())
			{
				out.string(slot);
				out.string(id);
			}
		}
		out.u8(static_cast<uint8_t>(gear ? std::min<size_t>(gear->bag().size(), 255) : 0));
		if (gear)
		{
			for (size_t i = 0; i < gear->bag().size() && i < 255; ++i) out.string(gear->bag()[i]);
		}
	}

	bool read_own(const std::vector<uint8_t>& data, OwnState& own)
	{
		ByteReader in(data);
		own.health = in.f32();
		own.healthMax = in.f32();
		own.mana = in.f32();
		own.manaMax = in.f32();
		own.level = static_cast<int>(in.varuint());
		own.experience = static_cast<int>(in.varuint());
		own.experienceNext = static_cast<int>(in.varuint());
		own.gold = static_cast<int>(in.varuint());
		own.cooldowns.resize(in.u8());
		for (auto& share : own.cooldowns) share = static_cast<float>(in.u8()) / 255.0f;
		own.unpaid = static_cast<uint32_t>(in.varuint());
		own.worn.resize(in.u8());
		for (auto& [slot, id] : own.worn)
		{
			slot = in.string();
			id = in.string();
		}
		own.bag.resize(in.u8());
		for (auto& id : own.bag) id = in.string();
		return in.ok();
	}

	DungeonServer::DungeonServer(const DungeonRules& rules, uint32_t seed) : m_rules(rules), m_seed(seed)
	{
	}

	DungeonServer::~DungeonServer()
	{
		// the connections go first, then the game they looked at
		m_server.reset();
		m_game.reset();
	}

	bool DungeonServer::start(uint16_t port, size_t maxPlayers, int bots)
	{
		m_world = std::make_unique<PhysicsWorld2D>(b2Vec2(0.0f, 0.0f));
		m_nav = std::make_unique<NavGrid2D>(0.5f);
		m_game = std::make_unique<DungeonGame>(*m_world, *m_nav, m_rules, m_seed);
		m_game->start(bots, true);
		m_bots.resize(static_cast<size_t>(bots));
		NetServer::Hooks hooks;
		hooks.join = [this](uint32_t, const std::string& name) -> Actor*
		{
			Actor& hero = m_game->add_hero();
			LOG_INFO("[DungeonServer] %s is hero %zu", name.c_str(), m_game->players().size() - 1);
			return &hero;
		};
		// a hero whose player left goes from the dungeon
		hooks.leave = [](uint32_t, Actor& hero) { hero.removed = true; };
		hooks.own = [this](ByteWriter& out, const Actor& hero) { write_own(out, hero, m_rules); };
		// its player wears or takes off an item: the bag is the server's
		hooks.command = [](Actor& hero, ByteReader& in) { DungeonGame::apply_gear_command(hero, in); };
		m_server = std::make_unique<NetServer>(*m_game, hooks, game_info(m_seed), 60, 3);
		if (!m_server->listen(port, maxPlayers))
		{
			LOG_ERROR("[DungeonServer] cannot listen on port %u", static_cast<unsigned>(port));
			return false;
		}
		LOG_INFO("[DungeonServer] seed %u, port %u, %d bots, %d monsters", m_seed, static_cast<unsigned>(m_server->port()), bots, m_game->monsters_left());
		return true;
	}

	void DungeonServer::step()
	{
		m_server->poll();
		m_inputs.assign(m_game->players().size(), PlayerInput());
		for (size_t i = 0; i < m_bots.size() && i < m_inputs.size(); ++i) m_inputs[i] = m_bots[i].think(*m_game, kStep, i);
		m_server->fill_inputs(m_inputs);
		m_game->step(kStep, m_inputs);
		++m_step;
		m_server->end_step(m_step);
	}

	std::string DungeonServer::describe(const std::string& host) const
	{
		const size_t players = m_server ? m_server->clients() : 0;
		std::string line = host + "  -  " + std::to_string(players) + (players == 1 ? " player" : " players");
		if (m_game->won()) line += ", cleared";
		else if (m_game->lost()) line += ", every hero down";
		else line += ", " + std::to_string(m_game->monsters_left()) + " monsters left";
		return line;
	}

	bool DungeonHost::start(const DungeonRules& rules, uint32_t seed, uint16_t port, size_t maxPlayers, const std::string& name)
	{
		stop();
		m_rules = rules;
		m_stop = false;
		m_started = 0;
		m_thread = std::thread(&DungeonHost::run, this, seed, port, maxPlayers, name);
		while (m_started == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
		if (m_started < 0)
		{
			m_thread.join();
			return false;
		}
		return true;
	}

	void DungeonHost::stop()
	{
		m_stop = true;
		if (m_thread.joinable()) m_thread.join();
	}

	// The server's thread: it makes, steps and ends the server (a NetHost stays on the thread that uses it)
	void DungeonHost::run(uint32_t seed, uint16_t port, size_t maxPlayers, std::string name)
	{
		DungeonServer server(m_rules, seed);
		if (!server.start(port, maxPlayers, 0))
		{
			m_started = -1;
			return;
		}
		m_port = server.port();
		LanBeacon beacon;
		if (!beacon.open(kLanGame, m_port)) LOG_WARNING("[DungeonHost] other players will not find this game (the discovery port is taken)");
		m_started = 1;
		using Clock = std::chrono::steady_clock;
		const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(DungeonServer::kStep));
		auto next = Clock::now();
		while (!m_stop)
		{
			const auto now = Clock::now();
			if (now - next > std::chrono::milliseconds(250)) next = now;
			while (next <= now)
			{
				server.step();
				next += step;
			}
			beacon.set_info(server.describe(name));
			beacon.poll();
			std::this_thread::sleep_until(next);
		}
		LOG_INFO("[DungeonHost] closed after %.0f s", server.steps() * DungeonServer::kStep);
	}
}
