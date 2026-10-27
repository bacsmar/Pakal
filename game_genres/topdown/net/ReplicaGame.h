///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ReplicaGame.h
// Purpose: A client's copy of a server's world, to draw it: one actor per actor of the snapshots (same
//          kind, name and faction), placed where the snapshot puts it. Each one tells its model how it
//          looks (RemoteViewComponent: dead, attacking, its action, hurt) as the server's did, so the
//          views of a local game draw it unchanged. It simulates nothing.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "topdown/net/NetProtocol.h"

#include <functional>
#include <map>
#include <memory>

namespace Pakal::TopDown::Net
{
	// How a remote actor looks, from its snapshot
	class RemoteViewComponent : public Component, public CharacterViewSource
	{
	public:
		void set(const ActorState& state);
		const ActorState& state() const { return m_state; }
		CharacterViewState view_state() const override;

	private:
		ActorState m_state;
		CharacterAction m_action;
	};

	class ReplicaGame : public TopDownGame
	{
	public:
		ReplicaGame();
		~ReplicaGame() override;

		// The actors as the snapshot has them: new ones made (then 'added' is called), the others placed. One no longer
		// there is left where it was last (a game that removes things calls forget()).
		void apply(const std::vector<ActorState>& actors, const std::function<void(Actor&)>& added = {});
		// The actor that stands for the server's actor 'id'
		Actor* replica(uintptr_t id) const;
		const ActorState* state(const Actor& actor) const;
		// Takes out the replicas the last apply() did not see (call where their models may go)
		void forget_missing();
		// The replicas the last apply() did not see
		std::vector<Actor*> missing() const;

		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const TopDownRules& rules() const override { return m_rules; }
		bool is_alive(const Actor& actor) const override;
		float hit_radius(const Actor&) const override { return 0.4f; }
		float random01() override { return m_random["sim"].uniform(); }
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }

	protected:
		DamageResult apply_damage(Actor&, const DamageInfo&) override { return DamageResult(); }

	private:
		Pakal::PhysicsWorld2D m_world;
		Pakal::NavGrid2D m_nav;
		TopDownRules m_rules;
		Pakal::RandomStreams m_random;
		std::map<uintptr_t, Actor*> m_replicas;   // by the server's id
		std::map<uintptr_t, bool> m_seen;         // in the last apply()
	};
}
