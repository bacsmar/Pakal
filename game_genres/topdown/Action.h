///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Action.h
// Purpose: Something a character does over time: it winds up, is active, recovers, and then cannot be
//          done again until its cooldown is over (a swing, a reload, a punch, a spell). The timeline
//          counts the phases down step by step and says when each one begins; what happens in each
//          one (a hitbox, the rounds going in) is the owner's. A phase of no time passes at once.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <algorithm>
#include <functional>

namespace Pakal::TopDown
{
	enum class ActionPhase
	{
		Idle,       // not under way (it may still be cooling down)
		Windup,
		Active,
		Recovery,
	};

	// How long each phase lasts (seconds); data of the action (a weapon's, a move's)
	struct ActionTiming
	{
		float windup = 0.0f;
		float active = 0.0f;
		float recovery = 0.0f;
		float cooldown = 0.0f;   // from the end of the recovery until it can start again
	};

	class ActionTimeline
	{
	public:
		// Called as each phase begins, and with Idle when the action is over
		std::function<void(ActionPhase)> onPhase;

		// Starts the action, unless one is under way or cooling down, or 'pay' (its cost: rounds, stamina...) says it
		// cannot be paid. Phases of no time pass at once, so a swing with no windup is active before this returns.
		bool start(const ActionTiming& timing, const std::function<bool()>& pay = {})
		{
			if (!ready() || (pay && !pay()))
			{
				return false;
			}
			m_timing = timing;
			enter(ActionPhase::Windup, timing.windup);
			return true;
		}

		// One step: the cooldown and the phase count down; a phase whose time is over gives way to the next
		void update(float dt)
		{
			m_cooldown = std::max(0.0f, m_cooldown - dt);
			if (m_phase == ActionPhase::Idle)
			{
				return;
			}
			m_left -= dt;
			if (m_left <= 0.0f)
			{
				next();
			}
		}

		// Stops it where it is: back to idle, with no cooldown and no more phases
		void cancel()
		{
			m_phase = ActionPhase::Idle;
			m_left = 0.0f;
		}

		ActionPhase phase() const { return m_phase; }
		bool busy() const { return m_phase != ActionPhase::Idle; }
		bool ready() const { return m_phase == ActionPhase::Idle && m_cooldown <= 0.0f; }
		float phase_left() const { return m_left; }      // seconds left of this phase
		float cooldown_left() const { return m_cooldown; }

	private:
		void enter(ActionPhase phase, float seconds)
		{
			m_phase = phase;
			m_left = seconds;
			if (onPhase) onPhase(phase);
			if (m_phase == phase && m_left <= 0.0f)
			{
				next();   // a phase of no time (unless the handler cancelled it)
			}
		}

		void next()
		{
			switch (m_phase)
			{
			case ActionPhase::Windup: enter(ActionPhase::Active, m_timing.active); break;
			case ActionPhase::Active: enter(ActionPhase::Recovery, m_timing.recovery); break;
			case ActionPhase::Recovery:
				m_phase = ActionPhase::Idle;
				m_left = 0.0f;
				m_cooldown = m_timing.cooldown;
				if (onPhase) onPhase(ActionPhase::Idle);
				break;
			case ActionPhase::Idle: break;
			}
		}

		ActionTiming m_timing;
		ActionPhase m_phase = ActionPhase::Idle;
		float m_left = 0.0f;
		float m_cooldown = 0.0f;
	};
}
