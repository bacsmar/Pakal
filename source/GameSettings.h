///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameSettings.h
// Purpose: The options every game has (docs/RELEASE_CHECKLIST.md: C-OPT, C-PAU-3), kept per device in one JSON file, and the
//          "about" data the options and credits show (docs/SETTINGS.md). The model of a model-view: each game draws the
//          screens in its own style and binds them to to_json() (IUIManager::create_data_model).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "Event.h"

#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	class Engine;

	class _PAKALExport GameSettings
	{
	public:
		// The options' names, as event_changed gives them and as to_json() keys them
		static constexpr const char* kMusic = "music";
		static constexpr const char* kSounds = "sounds";
		static constexpr const char* kLanguage = "language";
		static constexpr const char* kTelemetry = "telemetry";
		static constexpr const char* kQuality = "quality";
		static constexpr const char* kFsr = "fsr";
		static constexpr const char* kVibration = "vibration";

		struct Defaults
		{
			int music = 100;              // C-OPT-1: 0..100 in steps of 10 (a game's mix sets the default)
			int sounds = 80;              // C-OPT-2
			bool telemetry = false;       // C-OPT-6: nothing is sent until the player says yes (or the game's consent screen)
			bool vibration = true;        // C-OPT-5
		};

		// 'path': the file (a folder the game may write in: OSManager::data_folder() + "settings.json"). Read at once; a
		// missing or broken file gives the defaults
		explicit GameSettings(std::string path, const Defaults& defaults);
		explicit GameSettings(std::string path) : GameSettings(std::move(path), Defaults()) {}

		int music() const;                 // 0..100
		int sounds() const;                // 0..100
		// The language chosen; empty: the device's (C-OPT-3: the first start follows the device)
		std::string language() const;
		// What the game shows: the chosen language, else the device's when the game has it, else the first of 'available'
		std::string language_to_use(const std::string& device, const std::vector<std::string>& available) const;
		bool telemetry() const;
		std::string quality() const;       // "auto" or a tier the game knows ("low", "medium", "high", "ultra")
		std::string fsr() const;           // "auto", "on" or "off"
		bool vibration() const;

		// Each change is kept (saved at once, C-SAV-1) and told (event_changed with the option's name). Volumes go to the
		// nearest step of 10 within 0..100
		void set_music(int percent);
		void set_sounds(int percent);
		void set_language(const std::string& language);
		void set_telemetry(bool on);
		void set_quality(const std::string& quality);
		void set_fsr(const std::string& mode);
		void set_vibration(bool on);

		// A game's own option (a module's row: blood, the controls' layout...): its value as JSON text
		std::string extra(const std::string& name, const std::string& fallbackJson) const;
		void set_extra(const std::string& name, const std::string& json);

		// The volumes given to the sound manager now (C-PAU-3: the options and the pause panel change the same value at once).
		// The engine's systems take what they can; the language and the quality are the game's to apply
		void apply_sound(Engine& engine) const;

		// Every option as one JSON object, for a data model ({{music}}, {{language}}...)
		std::string to_json() const;
		// Back to the defaults (C-OPT-10's "reset" of the options; a game's progress is its own)
		void reset();

		Event<std::string> event_changed;

		struct About
		{
			std::string version, build, channel;   // from the build's identity (C-OPT-9: version + build number)
			std::string credits_file;              // C-OPT-7
			std::string privacy_url;               // C-OPT-8
		};
		// The "about" data: the version from the build's identity, and the game's credits file and privacy policy's address
		static About about(const Engine& engine, const std::string& creditsFile, const std::string& privacyUrl);
		static std::string about_json(const About& about);

	private:
		void changed(const char* name);
		bool save_locked() const;

		std::string m_path;
		Defaults m_defaults;
		mutable std::mutex m_mutex;
		struct State;
		State* m_state;
	public:
		~GameSettings();
		GameSettings(const GameSettings&) = delete;
		GameSettings& operator=(const GameSettings&) = delete;
	};
}
