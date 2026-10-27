///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameRules.h
// Purpose: The original game's rules for a level, read from level.json ("rules" and "destructibles",
//          written by ZombiesConvert from the game object library). Plain data with no engine
//          dependency, so tests can check it.
//
// Units: as in the game data. Distances in centimeters, times in milliseconds, speeds in cm/s, and
//        health and damage in the game's own points (the player has 80).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TopDown.h"
#include "topdown/TopDownRules.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace Zombies
{
	// How an enemy attacks (its attack component)
	enum class EnemyAttack
	{
		Melee,     // MeleeAttack: a swing that lands DamageDelay into its clip
		Flash,     // FlashAttack: a paparazzi's camera flash in a cone
		Boomer,    // BoomerAttack: a blast around itself DamageDelay into its clip, and four shots in a cross
		Bomber,    // BomberAttack: the tourist blows itself up
		DogWalker, // DogWalkerAttack: backs off from the player and lets dogs loose
		Dog,       // DogAttack: runs at the player, pounces and bites once
		Constant,  // ConstantAttack: the zombinator windmills its arms, hurting all the time it is close
		Bear       // ZombieBear: asleep when it comes, swats on all fours, rears up and swings its chain
	};

	// One kind of enemy. Its speed, health, damage and xp below are the first row of the game's tables; the tables have a row
	// per difficulty level (the game variable difficultyLevel), see at().
	struct EnemyRules
	{
		std::string name;
		float speed = 200.0f;
		float health = 1.5f;
		float attackDamage = 3.0f;
		float berserkSpeed = 450.0f;
		float berserkHealth = 1.125f;
		float berserkAttackDamage = 4.5f;
		// The rows of the tables (MoveSpeeds, HealthValues, AttackDamages, BerserkSettings), when the data has them
		struct Row
		{
			float speed, health, attackDamage;
		};
		std::vector<float> speeds, healths, attackDamages;
		std::vector<Row> berserkRows;
		// The values for a difficulty level: each table's row at that index, or its last row when it has fewer
		// (EnemyComponent::GetMoveSpeed, GetDamage, GetDifficultyHealthModifier)
		Row at(int difficulty, bool berserk) const;
		float attackIntervalMinMs = 500.0f;
		float attackIntervalMaxMs = 750.0f;
		// The attack: melee (MeleeAttack) or a camera flash in a cone (FlashAttack), or one of the special ones
		EnemyAttack attack = EnemyAttack::Melee;
		bool flash = false;
		float attackRange = 250.0f;    // where it stops walking up (and the reach of a blast or of the windmill)
		float attackRadius = 0.0f;     // where an attack may start (AttackRadius of the special attacks; 0: attackRange)
		float shotSpeed = 1000.0f;     // Boomer: its shots (ProjectileBullet), cm/s...
		float shotDamage = 10.0f;
		float shotRangeCm = 1000.0f;
		float prepareMs = 300.0f;      // Bomber: the prepare clip before the explode clip, whose blast lands attackDelayMs into it
		float explodeClipMs = 0.0f;
		float backOffRadius = 350.0f;  // DogWalker: closer than this it backs off...
		float backwardSpeed = 500.0f;  // ... at this speed
		std::string spawnType = "25013"; // the dog it lets loose, one every spawnIntervalMs...
		float spawnIntervalMs = 1000.0f;
		float spawnDelayMs = 1000.0f;  // ... spawnDelayMs into its spawn clip
		int spawnActive = 1;           // dogs of its own alive at once at most
		int spawnTotal = -1;           // dogs in all (-1: no end)
		float spawnClipMs = 0.0f;      // DogWalker: the release clip; Dog: the clip it appears with
		float runSpeed = 0.0f;         // Dog and Constant: speed while attacking
		float pounceRadius = 100.0f;   // Dog: it pounces this close
		float pounceClipMs = 0.0f;
		float attackDelayMs = 500.0f;  // from the start of the swing to the hit
		float attackTimeMs = 0.0f;     // length of the whole attack (flash)
		float attackAngle = 0.0f;      // degrees of the flash cone, 0 for melee
		float collisionSize = 150.0f;
		float attackClipMs = 0.0f;     // length of its attack clip(s): the attack lasts that long (0: unknown)
		float hurtClipMs = 0.0f;       // length of its bullet-hurt clip: no new attack while it plays
		float mass = 1.0f;             // knockback lasts 2000/mass ms, from 2000/mass cm/s
		float offscreenSpeed = 0.0f;   // MoveSpeedOffScreen (0: the normal speed)
		float rotateSpeed = 360.0f;    // degrees per second it turns toward the player before attacking
		float slowFactor = 1.0f;       // a flash slows the player down to this (OtherDamage)...
		float slowMs = 0.0f;           // ... for this long (AttackTime)
		// ZombieBear: asleep (still, unhurt) for sleepMs when it comes, then on all fours it swats (MeleeAttack: attackRadius,
		// attackRange, attackDelayMs). Having lost rearShare of its health since it woke it rears up (rearClipMs, unhurt
		// meanwhile) and swings its chain (ChainAttack) from then on, taking abdomenMul the damage
		float sleepMs = 0.0f, wakeClipMs = 0.0f, rearClipMs = 0.0f;
		float rearShare = 0.5f, abdomenMul = 1.0f;
		float chainRadius = 600.0f, chainRange = 500.0f, chainDelayMs = 1000.0f, chainDamage = 45.0f, chainClipMs = 0.0f;
		float chainIntervalMinMs = 500.0f, chainIntervalMaxMs = 3000.0f;
		// Meters from the center where bullets hit it: half the game's collision size, kept between the walking body
		// (0.3 m) and 0.6 m so a crowd stays tellable apart
		float hit_radius() const { return std::min(0.6f, std::max(0.3f, collisionSize * 0.005f)); }
		float xp = 2.0f;
		bool fadeOut = true;
		bool showHealthBar = false;    // a bar over its head for a moment after each hit (ShowHealthOnDamage)
		float fadeOutMs = 1000.0f;
		std::string attackSound;
		std::string deathSound = "evt_zombie_die";
		std::string hurtSound;
		// Drops when it dies (PickupSpawn)
		bool randomDrop = false;
		std::vector<int> drops;
		float dropRate = 0.0f;
	};

	// The original game's rules: what TopDownRules has, read from its object library, and its enemies
	class GameRules : public TopDownRules
	{
	public:
		GameRules();

		// Reads the "rules" and "destructibles" sections. Anything missing keeps the defaults above
		// (the values of the Glock, the player and the ZombieBusiness); 'warnings' lists what was missing.
		void read(const nlohmann::ordered_json& rules, const nlohmann::ordered_json& destructibles);

		std::map<std::string, EnemyRules> enemies;   // by game id ("25001")
		const EnemyRules& enemy(const std::string& type) const;

	private:
		EnemyRules m_fallbackEnemy;
	};
}
