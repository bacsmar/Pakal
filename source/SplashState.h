///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SplashState.h
// Purpose: The maker's logo when a game opens (docs/SPLASH.md; C-ID-1 in docs/RELEASE_CHECKLIST.md), as a
//          game state any game pushes first. The picture is the game's (its own or a sealed pack's); a game that names
//          none shows the engine's own logo (Assets/pakal/splash.png, copied next to every build).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "BaseGameState.h"
#include "Event.h"

#include <atomic>
#include <functional>
#include <string>

namespace Pakal
{
	class _PAKALExport SplashState final : public BaseGameState
	{
	public:
		struct Settings
		{
			std::string image;                  // the logo (a path of the game's resources); empty: the engine's (kEngineLogo)
			std::string background;             // the color around it: the picture's own edge, so no box shows on any aspect;
			                                    // empty: kEngineBackground with the engine's logo, else black
			// (a): the game has a loading screen of its own: the logo for fixed_seconds, never skipped, then the fade.
			// (b), when 'ready' is set: the logo covers the game's first load; it stays until ready() and at least
			// min_after_ready more, and any press skips it once the game is ready and it has shown min_after_ready
			float fixed_seconds = 1.5f;         // (a): 1 to 2 seconds
			std::function<bool()> ready;        // (b): the game is loaded (called on the game's thread)
			float min_after_ready = 1.0f;
			float fade_seconds = 0.8f;
			// After the fade: the game goes on to its title, menu or loading screen (transition_to_state from here is fine)
			std::function<void()> done;
			unsigned document_id = 0x5ffa5e;    // the interface document it uses
		};

		explicit SplashState(Settings settings);

		// The engine's own logo and the color it is shown on (the logo's dark green and cream both read on it)
		static constexpr const char* kEngineLogo = "Assets/pakal/splash.png";
		static constexpr const char* kEngineBackground = "#7a8a6a";

		// Bots and scripted captures leave the logo out (C-ID-1): PAKAL_NO_SPLASH=1 in the environment
		static bool skipped_by_environment();

	private:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override {}
		void on_deactivate(Engine* engine) override {}

		void listen(Engine* engine);
		void stop_listening();
		void finish();

		Settings m_settings;
		Engine* m_engine = nullptr;
		double m_shown = 0.0;        // seconds the logo has been visible
		double m_ready_at = -1.0;    // when the game said it was ready (b)
		double m_fade = -1.0;        // seconds into the fade; -1 before it
		bool m_finished = false;
		bool m_loaded = false;
		std::atomic<bool> m_pressed{ false };   // a key, a touch, a click or a pad button since the last update
		EventId m_key = 0, m_touch = 0, m_mouse = 0;
	};
}
