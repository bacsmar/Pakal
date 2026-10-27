///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: FighterComponent.h
// Purpose: The Brawler kit: a character that fights hand to hand with moves given as data (MoveRules:
//          the phases of an ActionTimeline, a hitbox that is out during the active phase, and what a
//          hit does: damage, hitstun, a push, a launch into the air). It takes hits the same way:
//          stunned, pushed away, flung up (HeightComponent). It is its model's CharacterViewSource and
//          the player's PlayerWeapon (the trigger is its first move).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Action.h"
#include "topdown/TopDownComponents.h"

#include <string>
#include <vector>

namespace Pakal::TopDown
{
	// One move of a fighter (a jab, a kick, a jump kick): data
	struct MoveRules
	{
		std::string name;           // also the look's action clip (CharacterViewComponent::Clips::actions)
		ActionTiming timing;        // the hitbox is out during the active phase
		float damage = 10.0f;
		float reach = 0.9f;         // the hitbox's center, this far ahead of the fighter (meters)
		float radius = 0.5f;        // ... and its radius
		float low = 0.2f, high = 1.7f; // the heights it covers over the fighter's feet (Height.h)
		float hitstun = 0.35f;      // seconds the one hit cannot act
		float push = 0.0f;          // meters per second the one hit is pushed away, slowing down over the hitstun
		float launch = 0.0f;        // vertical speed the one hit is flung up with (a launcher)
		float lunge = 0.0f;         // meters per second the fighter steps forward while the move is under way
		bool air = false;           // only in the air (a jump kick); the others only on the floor
		bool jump = false;          // the move leaves the floor (with 'launch' as its own vertical speed)
		bool evade = false;         // while it is under way nothing hits the fighter (a dodge); with no damage it strikes nothing
		std::string next;           // a combo: the move that follows when the same button is pressed again before this one ends
	};

	class FighterComponent : public Component, public PlayerWeapon, public CharacterViewSource
	{
	public:
		// 'runSpeed': meters per second when it walks
		FighterComponent(std::vector<MoveRules> moves, float runSpeed);

		// Starts a move by name; nothing while another one is under way, stunned, down or the move cannot be done now
		bool perform(const std::string& move);
		// A button pressed: in this step's update (after the control has turned it), the first of these moves that can be
		// done then. Pressed again during a move of a combo it started, it queues the combo's next move (done as the move
		// recovers).
		void request(const std::vector<std::string>& moves) { m_requested = moves; }
		// Held: it guards (no moves, no walking); a hit from the front does 'blockFactor' of its damage and does not stun
		void set_blocking(bool on)
		{
			if (on && !m_blocking) ++m_guard.serial;   // the guard's clip starts again
			m_blocking = on;
		}
		bool blocking() const { return m_blocking && !m_action.busy() && m_stun <= 0.0f; }
		float blockFactor = 0.2f;
		const MoveRules* find(const std::string& move) const;
		// PlayerWeapon: the trigger does its first move
		void set_trigger(bool pulled) override;
		// Walking speed; 0 while a move is under way or stunned (the move's lunge moves it instead)
		float run_speed() const override;

		// Hit: stunned for 'seconds' (a move in progress stops) and pushed at 'push' meters per second
		void stagger(float seconds, const b2Vec2& push);
		bool stunned() const { return m_stun > 0.0f; }
		// In a move that evades (a dodge): hits miss it
		bool evading() const { return m_move && m_move->evade && m_action.busy(); }
		bool busy() const { return m_action.busy(); }
		const MoveRules* move() const { return m_move; }
		ActionPhase phase() const { return m_action.phase(); }
		// Which way it faces: its control's facing (the player), else the one its brain sets
		b2Vec2 facing() const;
		void face(const b2Vec2& direction);
		// How many fighters each of its moves has hit so far (tests, a combo counter)
		int hits() const { return m_hits; }

		CharacterViewState view_state() const override;
		void update(TopDownGame& game, float dt) override;

	private:
		void strike(TopDownGame& game);   // the hitbox, while the move is active

		std::vector<MoveRules> m_moves;
		float m_runSpeed;
		ActionTimeline m_action;
		const MoveRules* m_move = nullptr;
		std::vector<uintptr_t> m_struck;   // who this move has hit (each one once)
		bool m_trigger = false;
		std::vector<std::string> m_requested;
		std::string m_chainRoot;            // the move a button started (a combo goes on while that button is pressed)
		const MoveRules* m_queued = nullptr;   // the combo's next move, once its button was pressed again
		bool m_blocking = false;
		CharacterAction m_guard{ "block", 0.0f, true, 1 << 20 };   // the guard's clip, for the model (serials apart from the moves')
		float m_stun = 0.0f;
		float m_stunTotal = 0.0f;
		b2Vec2 m_push = b2Vec2(0.0f, 0.0f);
		b2Vec2 m_facing = b2Vec2(1.0f, 0.0f);
		int m_hits = 0;
		int m_hurtClips = 0;
		float m_hurtFlash = 0.0f;
		CharacterAction m_view;            // the move's clip, for the model
	};
}
