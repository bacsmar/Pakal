///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ReplicaGame.cpp
// Purpose: A client's copy of a server's world - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/net/ReplicaGame.h"

namespace Pakal::TopDown::Net
{
	void RemoteViewComponent::set(const ActorState& state)
	{
		m_state = state;
		m_action.name = state.action;
		m_action.seconds = state.actionSeconds;
		m_action.serial = state.actionSerial;
		m_action.loop = false;
	}

	CharacterViewState RemoteViewComponent::view_state() const
	{
		CharacterViewState view;
		view.dead = m_state.dead;
		view.attacking = m_state.attacking;
		view.attackDirection = m_state.facing;
		view.attackSeconds = m_state.actionSeconds;
		view.hurt = m_state.hurt;
		view.hurtClips = m_state.hurtClips;
		view.special = m_state.special;
		view.action = m_state.action.empty() ? nullptr : &m_action;
		view.facing = m_state.facing;
		return view;
	}

	ReplicaGame::ReplicaGame() : m_world(b2Vec2(0.0f, 0.0f)), m_nav(1.0f), m_random(1)
	{
	}

	ReplicaGame::~ReplicaGame()
	{
		remove_all();
	}

	void ReplicaGame::apply(const std::vector<ActorState>& actors, const std::function<void(Actor&)>& added)
	{
		for (auto& [id, seen] : m_seen) seen = false;
		for (const auto& state : actors)
		{
			auto found = m_replicas.find(state.id);
			Actor* replica = found != m_replicas.end() ? found->second : nullptr;
			const bool fresh = replica == nullptr;
			if (fresh)
			{
				replica = &add_actor(state.kind, state.name, state.faction);
				replica->add<RemoteViewComponent>();
				replica->position = replica->previousPosition = state.position;
				m_replicas[state.id] = replica;
				join_added();
			}
			replica->previousPosition = replica->position;
			replica->position = state.position;
			// the yaw only at first: a headless server has no models to turn its actors, the client's model turns its replica
			// (toward where it walks, or where it acts: RemoteViewComponent)
			if (fresh) replica->yaw = state.yaw;
			replica->get<RemoteViewComponent>()->set(state);
			m_seen[state.id] = true;
			if (fresh && added) added(*replica);
		}
	}

	Actor* ReplicaGame::replica(uintptr_t id) const
	{
		auto found = m_replicas.find(id);
		return found != m_replicas.end() ? found->second : nullptr;
	}

	const ActorState* ReplicaGame::state(const Actor& actor) const
	{
		const RemoteViewComponent* remote = actor.get<RemoteViewComponent>();
		return remote ? &remote->state() : nullptr;
	}

	std::vector<Actor*> ReplicaGame::missing() const
	{
		std::vector<Actor*> gone;
		for (const auto& [id, seen] : m_seen)
		{
			if (!seen)
			{
				if (Actor* replica = this->replica(id)) gone.push_back(replica);
			}
		}
		return gone;
	}

	void ReplicaGame::forget_missing()
	{
		for (auto it = m_seen.begin(); it != m_seen.end();)
		{
			if (it->second)
			{
				++it;
				continue;
			}
			if (Actor* replica = this->replica(it->first)) replica->removed = true;
			m_replicas.erase(it->first);
			it = m_seen.erase(it);
		}
		remove_removed();
	}

	bool ReplicaGame::is_alive(const Actor& actor) const
	{
		const ActorState* remote = state(actor);
		return !actor.removed && remote && !remote->dead;
	}
}
