///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownRules.h
// Purpose: The rules any top-down action game reads from its data: the gun and the melee weapons, the
//          player, pickups, consumables, the gift box, props that break, and the sounds of its events.
//          Plain data with no engine dependency. A game reads them its own way (GameRules::read) and
//          adds its own (the enemies).
//
// Units: as in the game data. Distances in centimeters, times in milliseconds, speeds in cm/s.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	// The starting gun (ProjectileWeapon + ProjectileBullet)
	struct GunRules
	{
		std::string name;              // the names and sounds come from the game (its data or its defaults)
		float fireRateMs = 350.0f;    // between shots
		int magazine = 20;
		int rounds = 1000;            // ammunition carried
		float reloadMs = 750.0f;
		float runSpeed = 475.0f;      // the player's speed with this weapon
		float fireSpeed = 350.0f;     // ... while firing
		bool dualMuzzles = true;      // shots alternate between two guns
		float bulletSpeed = 2000.0f;
		float bulletDamage = 0.39f;
		float bulletWidth = 50.0f;
		int pellets = 1;              // bullets per shot (a shotgun fires several: BulletsFired)
		float spreadDegrees = 0.0f;   // they spread evenly over this whole angle (twice MaxFireSpreadAngle)
		// Where in each fire cycle (FireRate long) a shot leaves, 0..1: the fire clip's TriggerTimes over its length. Two
		// pistols fired together shoot twice per cycle, one shot from each.
		std::vector<float> triggers{ 0.0f };
		float rangeCm = 1000.0f;      // a bullet is dropped after flying this far
		float piercing = 1.0f;        // PiercingFactor: each body hit takes this from the bullet's 1; it goes on while some is left
		bool knockback = false;       // its bullets knock enemies back (DamageType KNOCKBACK_HURT)
		float blastRadiusCm = 0.0f;   // its shots burst where they land (a grenade launcher), hurting everything this close...
		float blastDamage = 0.0f;     // ... this much
		// Where its bullets start (meters): ahead of the shooter, to each side for two guns, over its feet; and how long
		// the view shows a shot (the fire pose)
		float muzzleForward = 0.4f, muzzleSide = 0.15f, muzzleHeight = 1.0f;
		float fireClipSeconds = 0.35f;
		std::string fireSound;
		std::string reloadSound;
	};

	// How an enemy dies of a hit (the DeathType of the weapon): it falls, it is flung into the air, or nothing is left of it
	enum class DeathKind
	{
		Normal,
		Fling,  // FLING_DEATH
		Burst   // EXPLOSION_DEATH
	};

	// A melee weapon a consumable lends for a while (MeleeWeapon)
	struct MeleeRules
	{
		float reach = 230.0f;
		float attackRateMs = 300.0f;
		float damage = 1.0f;
		float runSpeed = 600.0f;
		std::string swingSound;
		// What it hits: a trapezoid ahead (its widths below), or a circle of the reach all around
		bool circle = false;
		float nearHalfWidth = 0.1f, farHalfWidth = 2.0f;   // the trapezoid's half widths at the player and at the reach (m)
		float height = 1.8f;           // it reaches this high from the feet (m)
		float clipSeconds = 0.3f;      // how long the view shows a swing
		DeathKind death = DeathKind::Normal;
	};

	struct PlayerRules
	{
		float health = 80.0f;
		float invincibleAfterHitMs = 250.0f;
		float delayAfterDeathMs = 2000.0f;
		std::string hurtSound;
	};

	// Something the player picks up from the floor (Pickup)
	struct PickupRules
	{
		std::string type;            // what the game makes of it (TopDownGame::take_pickup)
		float value = 5.0f;
		float radius = 200.0f;
		float lifeTimeMs = 10000.0f; // < 0: it stays
		std::string sound;
		int consumable = 0;          // a consumable it applies when picked up (health, a boost): props and enemies drop them
		std::string item;            // an item of the RPG kit (the game puts it in the taker's bag)
	};

	// What a consumable does (Market Functions, and MeleeWeapon for the melee ones)
	struct ConsumableRules
	{
		int id = 0;
		std::string name;
		std::string effect;   // an effect that is not a stat (Effect Type): "airstrike" calls a bombardment
		float addHealth = 0.0f;
		float speedFactor = 1.0f;
		float damageFactor = 1.0f;
		float defenceFactor = 1.0f;
		float timerMs = 0.0f;  // how long it lasts; 0 for an instant effect
		bool melee = false;
		MeleeRules meleeRules;
		bool firearm = false;  // lends a gun for the timer (FireArm: not in the original data, added by the Downtown missions)
		GunRules gun;
		std::string sound;
	};

	struct GiftEntry
	{
		int consumable = 0;
		float chance = 0.0f;
	};

	// A prop that breaks (Health, PickupSpawn, Explosive)
	struct PropRules
	{
		int id = -1;
		std::string name;
		float x = 0.0f, y = 0.0f;   // level units
		float health = -1.0f;        // <= 0: it cannot be destroyed
		bool randomDrop = false;
		std::vector<int> drops;
		float dropRate = 0.0f;
		bool explosive = false;
		float blastRadius = 0.0f;
		float blastDamage = 0.0f;
		std::string deathSound;
		// Effects as it wears down (Health DamageLevels): each starts when the share of health left reaches its rate
		struct DamageLevel
		{
			float rate;
			std::string effect;
		};
		std::vector<DamageLevel> damageLevels;
		std::string burningEffect;   // an explosive prop keeps this going once it blew up (Explosive DamagedEffect)
		// Not in the original's data (the level builder's defend missions): a barricade enemies break and the player
		// repairs by standing next to it (Barricade RepairTime: ms from nothing to whole), and the thing to defend
		// (Objective): enemies go for it
		bool repairable = false;
		float repairSeconds = 4.0f;
		float repairReach = 1.8f;    // a player this close to it (m) repairs it
		bool objective = false;
		// Gone when broken (Health ClearWhenBroken, ours: the original's Break Barrier, a board across a way): nothing stops
		// at it any more and the walking grid opens there; other props keep blocking as their broken model
		bool clearWhenBroken = false;
	};

	// How hard things shake the camera (CameraRig::shake_from powers): the original game's (CameraObject::ShakeCamera) unless the
	// game's data says otherwise (its rules' "shake")
	struct ShakeRules
	{
		float explosion = 15.0f;    // grenades, explosive props, cars
		float bombing = 50.0f;      // an air strike
		float enemyBlast = 20.0f;   // an enemy blowing up or its blast
		float healthPack = 20.0f;   // the player takes a health pack
		float hurt = 15.0f;         // the player is hit
	};

	// How the spawners pace what they put out
	struct SpawnRules
	{
		float intervalSeconds = 5.0f;   // a spawner waits this long after putting one out
		int moanEvery = 4;              // one in this many that appear moans ("enemyMoan"); 0: none
	};

	class TopDownRules
	{
	public:
		GunRules gun;
		ShakeRules shake;
		SpawnRules spawn;
		PlayerRules player;
		std::map<int, PickupRules> pickups;          // by game id
		std::vector<int> randomPickups;              // what a random drop can be (PickupManager)
		std::map<int, ConsumableRules> consumables;  // by game id
		std::vector<GiftEntry> giftTable;
		float giftRadius = 200.0f;
		std::vector<PropRules> props;
		std::vector<std::string> warnings;
		// The sounds the game plays for its events, by what they are ("propHit", "playerDown", "revive"...): the data's own
		// over the game's defaults. sound() gives "" for an event neither knows.
		std::map<std::string, std::string> sounds;
		std::map<std::string, std::string> defaultSounds;
		const std::string& sound(const std::string& event) const;

		// The gift box's pick (GiftBoxComponent): 'roll' in [0, 1) walks the table subtracting each chance;
		// the last entry wins when the chances run out. 0 when the table is empty.
		int pick_gift(float roll) const;

		// A dropped pickup (PickupSpawnComponent::SpawnPickup): 'roll' in [0, 1) decides whether anything
		// drops, 'pick' in [0, 1) which one. 0 when nothing drops.
		int pick_drop(bool random, const std::vector<int>& drops, float rate, float roll, float pick) const;
	};
}
