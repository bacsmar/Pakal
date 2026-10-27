///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesHud.h
// Purpose: The Zombies HUD: health, money, the weapon, kills, the objective, the timer, the boost, the
//          dialog, the row of boosts, and the panel over the game (paused, down, complete, failed, with
//          the revive, the reward and the next mission). The RmlUi document Assets/ui/hud.rml, or
//          debug text without it. It shows a HudView the level fills each frame, and hands the clicks
//          back; what a button does is the level's.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Meta.h"

#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class IUIManager;
}

namespace Zombies
{
	// What the HUD shows this frame
	struct HudView
	{
		std::string levelName;
		float health = 0.0f, healthMax = 1.0f;
		bool dead = false;            // the player's health is out...
		bool down = false;            // ... and long enough for the down panel
		int money = 0;                // picked up in this level
		std::string meleeName;        // a melee weapon a consumable lends (shown instead of the gun)
		std::string gunName;
		int magazine = 0, rounds = 0;
		bool reloading = false;
		bool hasWeapon = false;
		int kills = 0, killGoal = 0;  // 0: no goal
		bool intro = false;           // the controls are off (a scripted intro)
		bool berserk = false;
		std::string objectiveKey;     // SetObjectiveText ("SURVIVE", a mission's own text...)
		bool timer = false;           // a countdown runs...
		int timerSeconds = 0;         // ... with this left
		bool boost = false;           // a consumable is active
		std::string boostName;
		float boostSecondsLeft = 0.0f;
		std::string dialog;           // ShowAutoDialog, while it lasts
		bool paused = false, complete = false, failed = false;
		std::string failText;
		bool mission = false;         // a mission of the campaign...
		bool missionSettled = false;  // ... whose result is settled (reward, stars)
		MissionReward reward;
		std::string heroRescuedName;  // a hero that joins with this win
		int revivePrice = 0, moneyToSpend = 0;
		bool hasNext = false;         // the next mission is open
	};

	class ZombiesHud
	{
	public:
		// Loads the document (false: it is not there, and the HUD is debug text)
		bool load(Pakal::IUIManager* ui);
		void unload();
		bool loaded() const { return m_loaded; }

		// The elements clicked since the last call (they arrive on the drawing thread)
		std::vector<std::string> take_clicks();
		// The row of boosts the player carries (only in a mission)
		void refresh_boosts(const std::string& missionId);
		// The options of the pause panel changed: they are filled again
		void options_changed() { m_optionsShown = false; }

		void draw(Pakal::BgfxGraphicsSystem& graphics, const HudView& view);

	private:
		void update_document(const HudView& view);
		void draw_text(Pakal::BgfxGraphicsSystem& graphics, const HudView& view);

		Pakal::IUIManager* m_ui = nullptr;
		bool m_loaded = false;
		unsigned m_clickListener = 0;
		std::mutex m_clickMutex;
		std::vector<std::string> m_clicks;
		bool m_boostsShown = false;
		bool m_optionsShown = false;   // the pause panel's options rows are up to date
	};
}
