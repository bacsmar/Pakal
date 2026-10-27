///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombieScript.h
// Purpose: The level scripts of the Zombies game: the original game's script functions
//          (SetEnemySpawn, ShowAutoDialog, FadeOut, ...) on the engine's ScriptHost (tasks as Lua
//          coroutines, Wait on the game clock, game variables), forwarded to the level through Host.
//          Functions the level does not implement yet are logged and do nothing.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "scripting/ScriptHost.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Zombies
{
	class ZombieScript
	{
	public:
		// What the level does when a script asks for it
		struct Host
		{
			std::function<void(int enemyType, int alive, float cooldownMs)> setEnemySpawn;
			std::function<void()> killAllZombies;
			std::function<void()> done;        // Done(): the objective is complete
			std::function<void()> doneKillAll;
			std::function<void(const std::string& text, float milliseconds)> showDialog;
			std::function<void(const std::string& key)> setObjectiveText;
			std::function<void()> removeObjectiveText;
			std::function<void(bool enabled)> enableControls;
			std::function<void(int objectId)> warpPlayer;                  // WarpPlayer(id): to a named point
			std::function<void(int objectId, bool shown)> showObject;      // ShowObject / HideObject: visibility only
			std::function<void(int objectId, bool enabled)> enableObject;  // EnableObject / DisableObject: logical state
			std::function<void(const std::vector<int>& ids, bool enabled)> enableRandomObject; // EnableRandomObject(id1, ..., bool, count)
			std::function<void(bool on)> enableBerserk;
			std::function<void(const std::vector<int>& ids)> reelObject;  // ReelObject(id1, ...): a film reel hides in one of them
			std::function<void(float milliseconds, float power)> shakeCamera;
			std::function<void(int fromId, const std::string& fromOffset, int toId, const std::string& toOffset, float seconds)> cameraPan;
			std::function<void()> cameraReturnToPlayer;
			// FadeOut / FadeIn: the screen goes to (toColor true) or comes back from the color "AARRGGBB" in 'milliseconds'
			std::function<void(bool toColor, float milliseconds, const std::string& colorArgb)> fade;
			std::function<void(const std::string& text, float milliseconds)> showClapper; // the scene's title card
			std::function<void(const std::string& name)> playMusic;                       // "" returns to the level's music
			// GetObjectHealth(id): what is left of a prop that breaks (0 once broken, -1 when there is no such prop)
			std::function<float(int objectId)> getObjectHealth;
			// StartTimer(ms) / ShowTimer(ms): a countdown on the screen; the game variable "timerMs" says what is left.
			// StopTimer / HideTimer remove it.
			std::function<void(float milliseconds)> startTimer;
			std::function<void()> stopTimer;
			std::function<void(int pickupId, int pointId)> dropPickup;   // DropPickup(pickup, point): a pickup at a named point/anchor
			std::function<void(const std::string& message)> fail;      // Fail(text): the mission is lost
			std::function<bool(int pointId, float radiusCm)> isPlayerNear; // IsPlayerNear(point, radius): the player within it
			std::function<void(int pointId, bool on)> showMarker;          // ShowMarker(point, on): a beacon the player goes to
			// StartEscort(hero, start, point1, ...): a survivor (a hero of the level's visuals) at 'start' walks the points in
			// order with the player; the game variables escortState (0 waiting, 1 walking, 2 arrived, 3 dead) and escortHealth
			// (0-100) tell how it goes
			std::function<void(const std::string& hero, const std::vector<int>& points)> startEscort;
			std::function<void(const std::string& message)> log; // called for functions the level does not implement
		};

		explicit ZombieScript(Host host);
		~ZombieScript();
		ZombieScript(const ZombieScript&) = delete;
		ZombieScript& operator=(const ZombieScript&) = delete;

		// Starts a task from a Lua chunk. A looping task runs its chunk again each time it finishes
		// (the Run hook of an objective). Returns false when the chunk does not compile.
		bool start(const std::string& name, const std::string& code, bool loop) { return m_runtime.start(name, code, loop); }

		// Resumes every task that is due at game time 'clockMs' (milliseconds since the level started)
		void update(double clockMs) { m_runtime.update(clockMs); }

		// Game variables shared with the scripts (GetGameVariable / SetGameVariable)
		double get_variable(const std::string& name) const { return m_runtime.get_variable(name); }
		void set_variable(const std::string& name, double value) { m_runtime.set_variable(name, value); }

		// EnableSkip(true) lets the player cut a cutscene short: skip() makes every Wait pass at once until EnableSkip(false)
		bool skippable() const { return m_runtime.skippable(); }
		bool skipping() const { return m_runtime.skipping(); }
		void skip() { m_runtime.skip(); }

		bool is_running(const std::string& name) const { return m_runtime.is_running(name); }
		// Ends every task with this name (its coroutine is dropped; a suspended Wait is never resumed)
		void stop(const std::string& name) { m_runtime.stop(name); }
		size_t task_count() const { return m_runtime.task_count(); }

	private:
		void bind_functions();

		Host m_host;
		Pakal::ScriptHost m_runtime;
	};
}
