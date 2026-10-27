///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Meta.h
// Purpose: What lasts between missions: the campaign (missions in order, the shop's catalog, from game/campaign.json)
//          and the player's profile (money, missions done and their stars, weapons, upgrades, boosts), saved per device.
//          Pure logic: no window, no files except the profile's load and save.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "GameRules.h"

#include <nlohmann/json.hpp>

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Zombies
{
	// One star of a mission: "complete" (win it), "health" (end with at least this share of health), "time" (win within
	// these seconds), "kills" (at least this many)
	struct StarRule
	{
		std::string type;
		float value = 0.0f;
	};

	struct MissionDef
	{
		std::string id;
		std::string title;
		std::string brief;
		std::string level;          // the level.json, from the working folder
		int rewardFirst = 0;        // money for the first win
		int rewardReplay = 0;       // ... and for each later one
		std::vector<StarRule> stars;
		bool free = false;          // always open, outside the campaign's order (a level to look around in)
		bool side = false;          // a side mission: it opens with the one before, and the next ones do not wait for it
		std::string unlocks;        // the hero it rescues (HeroDef id): playable once it is won
	};

	// A survivor the player can play as (the kit's characters): the first is there from the start, the others are rescued
	struct HeroDef
	{
		std::string id;             // the level's visuals.heroes key (sam, lis, matt, shaun)
		std::string title;
		std::string brief;
	};

	struct WeaponDef
	{
		std::string id;             // the gun's name in the level rules (Glock, SMG, ...)
		std::string title;
		int consumable = 0;         // the level's consumable that lends it (0: the level's own gun)
		int price = 0;
		int upgradeBase = 0;        // the upgrade prices are this times the campaign's factors
	};

	struct BoostDef
	{
		std::string id;
		std::string title;
		int consumable = 0;         // the level's consumable it uses
		int price = 0;
	};

	struct Campaign
	{
		std::vector<MissionDef> missions;
		std::vector<WeaponDef> weapons;
		std::vector<BoostDef> boosts;
		std::vector<HeroDef> heroes;
		float damagePerLevel = 0.15f;      // an upgrade level adds this share of damage
		float fireRatePerLevel = 0.04f;    // ... and fires this much faster
		std::vector<float> upgradePriceFactors{ 0.25f, 0.5f, 0.8f, 1.2f, 1.8f }; // one per level
		float healthPerLevel = 10.0f;
		std::vector<int> healthPrices{ 300, 600, 1000, 1500, 2200 };

		bool read(const nlohmann::json& json, std::string& error);
		bool load(const std::string& path, std::string& error);

		int mission_index(const std::string& id) const;  // -1 if there is none
		const MissionDef* mission(const std::string& id) const;
		const WeaponDef* weapon(const std::string& id) const;
		const BoostDef* boost(const std::string& id) const;
		const HeroDef* hero(const std::string& id) const;
		// The mission that rescues a hero (nullptr: it is there from the start)
		const MissionDef* rescue_of(const std::string& hero) const;
		int upgrade_levels() const { return static_cast<int>(upgradePriceFactors.size()); }
		// The price to take a weapon from level-1 to 'level' (1..upgrade_levels()); 0 if there is no such level
		int upgrade_price(const WeaponDef& weapon, int level) const;
		int health_levels() const { return static_cast<int>(healthPrices.size()); }
		int health_price(int level) const;
	};

	struct MissionRecord
	{
		int wins = 0;
		int stars = 0;          // the most it got (0..3)
		int bestKills = 0;
		float bestSeconds = 0.0f; // the fastest win (0: none yet)
	};

	struct Profile
	{
		int money = 0;
		std::map<std::string, MissionRecord> missions;
		std::set<std::string> weapons{ "Glock" };
		std::string equipped = "Glock";
		std::map<std::string, int> weaponLevels;
		int healthLevel = 0;
		std::map<std::string, int> boosts;      // how many of each the player carries
		std::set<std::string> heroes{ "sam" };  // the ones rescued (playable)
		std::string hero = "sam";               // the one played
		// Options (the menu's and the pause panel's): the picture's quality (-1: the device's default) and FSR upscaling
		int quality = -1;
		bool fsr = true;
		bool sketch = false;   // the sketch look (an etching on warm paper)

		nlohmann::json to_json() const;
		void from_json(const nlohmann::json& json);
		bool load(const std::string& path);         // false (and a new profile) when there is none
		bool save(const std::string& path) const;   // written to a side file, then renamed over

		// The first mission is always open; each next one opens when the one before was won
		bool unlocked(const Campaign& campaign, size_t index) const;
		int total_stars() const;
		int weapon_level(const std::string& id) const;
		int boost_count(const std::string& id) const;
	};

	// How a mission went
	struct MissionResult
	{
		bool won = false;
		int kills = 0;
		int moneyPicked = 0;    // what the player picked up during it (kept even when it is lost)
		float healthShare = 0.0f;
		float seconds = 0.0f;
	};

	struct MissionReward
	{
		std::array<bool, 3> stars{};   // which of the mission's stars were earned this time
		int starCount = 0;
		int reward = 0;                // the mission's reward (only for a win)
		bool firstWin = false;
		int moneyPicked = 0;
		int moneyAfter = 0;
		std::string heroRescued;       // a hero this win made playable
	};

	// Which stars a result earns
	std::array<bool, 3> earned_stars(const MissionDef& mission, const MissionResult& result);
	// Adds the money (what was picked up, plus the reward of a win) and the records to the profile
	MissionReward settle(const Campaign& campaign, Profile& profile, const std::string& missionId, const MissionResult& result);

	enum class Purchase { Ok, NotEnoughMoney, AlreadyOwned, MaxLevel, NotOwned, Unknown };
	const char* purchase_text(Purchase purchase);
	Purchase buy_weapon(const Campaign& campaign, Profile& profile, const std::string& weapon);
	Purchase buy_upgrade(const Campaign& campaign, Profile& profile, const std::string& weapon);
	Purchase buy_health(const Campaign& campaign, Profile& profile);
	Purchase buy_boost(const Campaign& campaign, Profile& profile, const std::string& boost);
	bool equip(Profile& profile, const std::string& weapon);
	bool choose_hero(Profile& profile, const std::string& hero);   // false if it is not rescued yet

	// The level's rules with the player's loadout: the equipped gun with its upgrades, and the extra health
	void apply_loadout(GameRules& rules, const Campaign& campaign, const Profile& profile);
}
