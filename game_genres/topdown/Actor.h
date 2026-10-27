///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Actor.h
// Purpose: TopDown's actors are the engine's synchronous entities (SyncEntity): a thing in the level
//          (the player, an enemy, a prop, a pickup) whose components run once per fixed step, in the
//          order they were added. TopDown's components get the game (TopDownGame, the SimWorld the
//          actors live in) instead of the bare world.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "SyncEntity.h"

namespace Pakal::TopDown
{
	class TopDownGame;

	using Actor = Pakal::SyncEntity;

	// What an actor is (its SyncEntity::kind). Whose side a character is on is its faction (Factions.h); what sort of
	// character it is comes from the game's data (Zombies: its EnemyRules by type).
	struct ActorKind
	{
		enum : int
		{
			Character,   // the player, an enemy, someone the player escorts
			Prop,
			Pickup,
			Missile,     // something thrown that flies on its own (the RPG kit's projectiles)
		};
	};

	class Component : public Pakal::SyncComponent
	{
	public:
		Actor& actor() const { return entity(); }

		// Called once per fixed step, in the order the components were added
		virtual void update(TopDownGame& game, float dt) { (void)game; (void)dt; }

	private:
		// The world of a TopDown actor is always a TopDownGame
		void update(Pakal::SimWorld& world, float dt) final;
	};
}
