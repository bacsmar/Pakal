///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterViewer.h
// Purpose: The character viewer and the zombie encyclopedia: one character at a time on a turntable,
//          lit as in the levels, with its clips, its weapons and the other hand reaching for the gun
//          (two-bone IK, drawn as lines with how far the hand is from the grip). Heroes come from the
//          campaign, enemies (with their numbers) from the first level's rules; both look as that
//          level says (visuals). Its panels are an RmlUi document (ui/viewer.rml) over the model, to
//          use with a finger as with the keys. To pick a hero and to check that every model looks right.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "GameRules.h"
#include "topdown/view/CharacterViewComponent.h"
#include "topdown/view/InputMapper.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class CameraComponent3D_Bgfx;
	class Engine;
	class IUIManager;
	class MeshComponent3D_Bgfx;
}

namespace Zombies
{
	class CharacterViewerState : public Pakal::BaseGameState
	{
	public:
		enum class Mode { Heroes, Enemies };
		explicit CharacterViewerState(Mode mode);
		~CharacterViewerState() override;

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override;
		void on_deactivate(Pakal::Engine* engine) override;

	private:
		using Look = Pakal::TopDown::CharacterViewComponent::Look;
		using Held = Pakal::TopDown::CharacterViewComponent::Held;

		// One character of the list: a hero, or a type of enemy
		struct Entry
		{
			std::string id, title;
			std::string text;                 // a hero's story
			std::string status;               // a hero: playing, rescued, or the mission that rescues it
			bool rescued = false, playing = false;
			float health = 0.0f, speed = 0.0f, damage = 0.0f;   // an enemy's numbers (speed in meters per second)
			std::string attack;
			std::vector<std::string> models;  // its model and its variants (files)
			Look look;
		};
		// What a hero can hold: nothing, its gun, or a weapon a consumable lends
		struct Weapon
		{
			std::string name;
			bool hasHeld = false;
			Held held;
		};

		bool load();
		void show_entry();              // graphics thread: the model of the entry and variant chosen
		void show_weapon();             // graphics thread: the weapon chosen in its hand, and the reach
		void play_clip();
		// A button of the panels or a key: "v_next", "v_clip_prev"... Returns false when this state is gone (back).
		bool act(const std::string& action);
		bool pressed(Pakal::Key key);   // down now, not in the last frame
		void update_camera(float dt);
		void draw_reach();
		void fill_panels();             // everything but the reach's distance
		void fill_reach();

		Mode m_mode;
		Pakal::Engine* m_engine = nullptr;
		Pakal::BgfxGraphicsSystem* m_graphics = nullptr;
		Pakal::IUIManager* m_ui = nullptr;
		unsigned m_clicks = 0;
		std::mutex m_mutex;
		std::vector<std::string> m_pending;   // buttons pressed (the interface's thread)
		std::string m_levelDir;
		GameRules m_rules;
		std::vector<Entry> m_entries;
		float m_most[3] = { 1.0f, 1.0f, 1.0f };   // the highest health, speed and damage of the enemies (the bars' full)
		std::vector<Weapon> m_weapons;
		std::unique_ptr<Pakal::TopDown::InputMapper> m_keys;
		std::map<Pakal::Key, bool> m_held;
		std::unique_ptr<Pakal::CameraComponent3D_Bgfx> m_camera;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_floor;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_model;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_weaponMesh;
		size_t m_entry = 0, m_variant = 0, m_weapon = 1;
		int m_clip = -1;                // -1: the look's idle clip
		float m_speed = 1.0f;           // the clip's playback
		bool m_paused = false;
		bool m_reach = true;            // the other hand on the gun
		bool m_lines = true;            // the arm's chain and the grip drawn
		float m_yaw = 0.6f, m_pitch = 0.18f, m_distance = 2.4f;   // the camera: around, above, and how far (in heights)
		bool m_spin = true;
		bool m_loadTried = false, m_loaded = false;
		double m_seconds = 0.0;
		bool m_shot = false;
		float m_handOff = -1.0f;        // centimeters from the hand to the grip (-1: no grip)
		std::string m_reachShown;       // what the panel says about it now
		float m_scriptClock = 0.0f;
		// ZOMBIES_VIEWER_SHOTS=<folder>: a capture of every weapon with each clip of ZOMBIES_VIEWER_SHOT_CLIPS (to check them)
		struct Shots
		{
			std::string folder;
			std::vector<std::string> clips;
			size_t weapon = 0, clip = 0;
			float wait = 0.0f;
			bool posed = false;
		} m_shots;
		void take_shots(float dt);
		std::vector<std::string> m_scripted;   // ZOMBIES_VIEWER_CLICKS: buttons pressed one by one (to try it)
	};
}
