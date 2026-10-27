///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameRules.cpp
// Purpose: The original game's rules for a level - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GameRules.h"

#include <algorithm>
#include <cstdlib>

namespace Zombies
{
	using Json = nlohmann::ordered_json;

	namespace
	{
		float number(const Json& object, const char* key, float fallback)
		{
			if (!object.is_object() || !object.contains(key)) return fallback;
			const Json& value = object[key];
			return value.is_number() ? value.get<float>() : fallback;
		}

		std::string text(const Json& object, const char* key, const std::string& fallback = std::string())
		{
			if (!object.is_object() || !object.contains(key) || !object[key].is_string()) return fallback;
			return object[key].get<std::string>();
		}

		bool flag(const Json& object, const char* key, bool fallback)
		{
			if (!object.is_object() || !object.contains(key) || !object[key].is_boolean()) return fallback;
			return object[key].get<bool>();
		}

		// First row of a table property ("HealthValues": [{"HealthValue": 1.5}, ...]): difficulty 0
		const Json& first_row(const Json& object, const char* key)
		{
			static const Json empty = Json::object();
			if (!object.is_object() || !object.contains(key) || !object[key].is_array() || object[key].empty()) return empty;
			return object[key][0];
		}

		std::vector<int> ids(const Json& rows, const char* key)
		{
			std::vector<int> out;
			if (!rows.is_array()) return out;
			for (const auto& row : rows)
			{
				if (row.is_object() && row.contains(key) && row[key].is_number()) out.push_back(row[key].get<int>());
			}
			return out;
		}

		// The sound of a Health component's damage effect ("DEFAULT_DEATH" -> "evt_zombie_die")
		std::string damage_sound(const Json& health, const char* damageType)
		{
			if (!health.contains("DamageEffects") || !health["DamageEffects"].is_array()) return std::string();
			for (const auto& effect : health["DamageEffects"])
			{
				if (text(effect, "DamageType") == damageType) return text(effect, "sound");
			}
			return std::string();
		}

		// The original game's values for what its data leaves out: the dual Glock (the numbers are TopDownRules'), and money
		// on the floor
		GunRules original_gun()
		{
			GunRules gun;
			gun.name = "Glock";
			gun.fireSound = "evt_weapon_dualglock_shoot";
			gun.reloadSound = "sfx_weapon_dualglock_reload";
			return gun;
		}

		PickupRules original_pickup()
		{
			PickupRules pickup;
			pickup.type = "Money";
			pickup.sound = "sfx_pickup_zombucks";
			return pickup;
		}

		const Json& components_of(const Json& objects, const std::string& id)
		{
			static const Json empty = Json::object();
			if (!objects.is_object() || !objects.contains(id)) return empty;
			const Json& object = objects[id];
			return object.contains("components") ? object["components"] : empty;
		}
	}

	void GameRules::read(const Json& rules, const Json& destructibles)
	{
		const Json objects = rules.value("objects", Json::object());
		// how hard things shake the camera, when the data says (else the original's)
		const Json shakes = rules.value("shake", Json::object());
		shake.explosion = shakes.value("explosion", shake.explosion);
		shake.bombing = shakes.value("bombing", shake.bombing);
		shake.enemyBlast = shakes.value("enemyBlast", shake.enemyBlast);
		shake.healthPack = shakes.value("healthPack", shake.healthPack);
		shake.hurt = shakes.value("hurt", shake.hurt);
		sounds.clear();
		if (rules.contains("sounds") && rules["sounds"].is_object())
		{
			for (auto it = rules["sounds"].begin(); it != rules["sounds"].end(); ++it)
			{
				if (it.value().is_string()) sounds[it.key()] = it.value().get<std::string>();
			}
		}

		// The starting gun
		const std::string gunId = std::to_string(rules.value("startingWeapon", 15001));
		const Json& gunComponents = components_of(objects, gunId);
		if (gunComponents.contains("ProjectileWeapon"))
		{
			const Json& weapon = gunComponents["ProjectileWeapon"];
			const Json bullet = gunComponents.value("ProjectileBullet", Json::object());
			gun.name = objects[gunId].value("name", gun.name);
			gun.fireRateMs = number(weapon, "FireRate", gun.fireRateMs);
			gun.magazine = static_cast<int>(number(weapon, "MagazineSize", static_cast<float>(gun.magazine)));
			gun.rounds = static_cast<int>(number(weapon, "Rounds", static_cast<float>(gun.rounds)));
			gun.reloadMs = number(weapon, "ReloadTime", gun.reloadMs);
			gun.runSpeed = number(weapon, "RunSpeed", gun.runSpeed);
			gun.fireSpeed = number(weapon, "FireSpeed", gun.fireSpeed);
			gun.dualMuzzles = flag(weapon, "DualMuzzles", gun.dualMuzzles);
			gun.fireSound = text(weapon, "FireSound", gun.fireSound);
			gun.reloadSound = text(weapon, "ReloadSound", gun.reloadSound);
			gun.bulletSpeed = number(bullet, "Speed", gun.bulletSpeed);
			gun.bulletDamage = number(bullet, "Damage", gun.bulletDamage);
			gun.bulletWidth = number(bullet, "BulletWidth", gun.bulletWidth);
			gun.piercing = number(bullet, "PiercingFactor", gun.piercing);
			gun.knockback = text(bullet, "DamageType") == "KNOCKBACK_HURT";
			gun.pellets = std::max(1, static_cast<int>(number(weapon, "BulletsFired", 1.0f)));
			gun.spreadDegrees = 2.0f * number(weapon, "MaxFireSpreadAngle", 0.0f);
			// The shots of a cycle: the fire clip's trigger times over the clip's length (annotated from the rigs as
			// ClipLengthsMs on the player object); without the length they are spread evenly over the cycle
			const Json clips = objects.contains(std::to_string(rules.value("player", 35001)))
				? objects[std::to_string(rules.value("player", 35001))].value("ClipLengthsMs", Json::object()) : Json::object();
			const float clipMs = number(clips, text(weapon, "FireAnimation").c_str(), 0.0f);
			const Json triggerTimes = weapon.value("TriggerTimes", Json::array());
			if (triggerTimes.is_array() && !triggerTimes.empty())
			{
				gun.triggers.clear();
				for (size_t i = 0; i < triggerTimes.size(); ++i)
				{
					const float at = clipMs > 0.0f ? number(triggerTimes[i], "Time", 0.0f) / clipMs : static_cast<float>(i) / static_cast<float>(triggerTimes.size());
					gun.triggers.push_back(std::min(0.999f, std::max(0.0f, at)));
				}
				std::sort(gun.triggers.begin(), gun.triggers.end());
			}
		}
		else
		{
			warnings.push_back("no starting weapon " + gunId);
		}

		// The player
		const Json& playerComponents = components_of(objects, std::to_string(rules.value("player", 35001)));
		if (playerComponents.contains("Health"))
		{
			const Json& health = playerComponents["Health"];
			player.health = number(health, "HealthValue", player.health);
			player.delayAfterDeathMs = number(health, "DelayAfterDeath", player.delayAfterDeathMs);
			const std::string hurt = damage_sound(health, "DEFAULT_HURT");
			if (!hurt.empty()) player.hurtSound = hurt;
		}
		else
		{
			warnings.push_back("no player");
		}
		if (playerComponents.contains("Player"))
		{
			player.invincibleAfterHitMs = number(playerComponents["Player"], "InvincibilityAfterHitTime", player.invincibleAfterHitMs);
		}

		// Enemies, pickups and consumables, by what their components say they are
		for (auto it = objects.begin(); it != objects.end(); ++it)
		{
			const std::string id = it.key();
			const std::string name = it.value().value("name", std::string());
			const Json& c = components_of(objects, id);

			if (c.contains("Enemy"))
			{
				const Json& enemy = c["Enemy"];
				EnemyRules e;
				e.name = name;
				e.speed = number(first_row(enemy, "MoveSpeeds"), "MoveSpeed", e.speed);
				e.health = number(first_row(enemy, "HealthValues"), "HealthValue", e.health);
				e.attackDamage = number(first_row(enemy, "AttackDamages"), "AttackDamage", e.attackDamage);
				e.xp = number(first_row(enemy, "XpValues"), "XpValue", e.xp);
				auto rows = [&enemy](const char* table, const char* key)
				{
					std::vector<float> out;
					const Json values = enemy.value(table, Json::array());
					if (!values.is_array()) return out;
					for (const auto& row : values) out.push_back(number(row, key, 0.0f));
					return out;
				};
				e.speeds = rows("MoveSpeeds", "MoveSpeed");
				e.healths = rows("HealthValues", "HealthValue");
				e.attackDamages = rows("AttackDamages", "AttackDamage");
				const Json berserkRows = enemy.value("BerserkSettings", Json::array());
				if (berserkRows.is_array())
				{
					for (const auto& row : berserkRows)
					{
						e.berserkRows.push_back({ number(row, "MoveSpeed", e.speed), number(row, "HealthValue", e.health), number(row, "AttackDamage", e.attackDamage) });
					}
				}
				const Json& berserk = first_row(enemy, "BerserkSettings");
				e.berserkSpeed = number(berserk, "MoveSpeed", e.speed);
				e.berserkHealth = number(berserk, "HealthValue", e.health);
				e.berserkAttackDamage = number(berserk, "AttackDamage", e.attackDamage);
				e.attackIntervalMinMs = number(enemy, "AttackIntervalMin", e.attackIntervalMinMs);
				e.attackIntervalMaxMs = number(enemy, "AttackIntervalMax", e.attackIntervalMaxMs);
				e.attackSound = text(enemy, "AttackSound");
				e.rotateSpeed = number(enemy, "RotateSpeed", e.rotateSpeed);
				e.offscreenSpeed = number(enemy, "MoveSpeedOffScreen", e.offscreenSpeed);
				if (c.contains("BuiltinCollision2DPolygon")) e.mass = std::max(0.01f, number(c["BuiltinCollision2DPolygon"], "Mass", e.mass));
				// Clip lengths (annotated from the rigs: annotate_clips.py) time the attack and the hurt stun
				const Json clips = it.value().value("ClipLengthsMs", Json::object());
				auto clipMs = [&clips](const std::string& clip) { return clip.empty() ? 0.0f : number(clips, clip.c_str(), 0.0f); };
				if (c.contains("FlashAttack"))
				{
					const Json& flash = c["FlashAttack"];
					e.flash = true;
					e.attack = EnemyAttack::Flash;
					e.attackRange = number(flash, "AttackRange", e.attackRange);
					e.attackDelayMs = number(flash, "DamageDelay", e.attackDelayMs);
					e.attackTimeMs = number(flash, "AttackTime", e.attackTimeMs);
					e.attackAngle = number(flash, "AttackRangeAngle", e.attackAngle);
					e.attackClipMs = clipMs(text(flash, "Animation"));
					e.slowFactor = number(flash, "OtherDamage", e.slowFactor);
					e.slowMs = number(flash, "AttackTime", e.slowMs);
				}
				else if (c.contains("MeleeAttack"))
				{
					const Json& melee = c["MeleeAttack"];
					e.attackRange = number(melee, "AttackRange", e.attackRange);
					e.attackDelayMs = number(melee, "DamageDelay", e.attackDelayMs);
					// the swings take turns: their average length
					float total = 0.0f;
					int count = 0;
					for (const auto& swing : melee.value("AttackAnimations", Json::array()))
					{
						const float ms = clipMs(text(swing, "Animation"));
						if (ms > 0.0f) { total += ms; ++count; }
					}
					if (count > 0) e.attackClipMs = total / count;
				}
				else if (c.contains("BoomerAttack"))
				{
					const Json& boomer = c["BoomerAttack"];
					e.attack = EnemyAttack::Boomer;
					e.attackRange = number(boomer, "AttackRange", e.attackRange);
					e.attackRadius = number(boomer, "AttackRadius", e.attackRadius);
					e.attackDelayMs = number(boomer, "DamageDelay", e.attackDelayMs);
					e.attackClipMs = clipMs(text(boomer, "Animation"));
					if (c.contains("ProjectileBullet"))
					{
						e.shotSpeed = number(c["ProjectileBullet"], "Speed", e.shotSpeed);
						e.shotDamage = number(c["ProjectileBullet"], "Damage", e.shotDamage);
					}
				}
				else if (c.contains("BomberAttack"))
				{
					const Json& bomber = c["BomberAttack"];
					e.attack = EnemyAttack::Bomber;
					e.attackRange = number(bomber, "AttackRange", e.attackRange);
					e.attackRadius = number(bomber, "AttackRadius", e.attackRadius);
					e.attackDelayMs = number(bomber, "DamageDelay", e.attackDelayMs);
					e.prepareMs = number(bomber, "PrepareTime", e.prepareMs);
					e.explodeClipMs = clipMs(text(bomber, "ExplodeAnimation"));
				}
				else if (c.contains("DogWalkerAttack"))
				{
					const Json& walker = c["DogWalkerAttack"];
					e.attack = EnemyAttack::DogWalker;
					e.attackRadius = number(walker, "AttackRadius", 600.0f);
					e.attackRange = e.attackRadius; // it stops walking where it starts letting dogs loose
					e.backOffRadius = number(walker, "BackOffRadius", e.backOffRadius);
					e.backwardSpeed = number(walker, "BackwardSpeed", e.backwardSpeed);
					e.spawnType = std::to_string(static_cast<int>(number(walker, "SpawnEnemyType", 25013.0f)));
					e.spawnIntervalMs = number(walker, "SpawnInterval", e.spawnIntervalMs);
					e.spawnDelayMs = number(walker, "SpawnDelay", e.spawnDelayMs);
					e.spawnActive = static_cast<int>(number(walker, "SpawnActive", 1.0f));
					e.spawnTotal = static_cast<int>(number(walker, "SpawnTotal", -1.0f));
					e.spawnClipMs = clipMs(text(walker, "SpawnAnimation"));
				}
				else if (c.contains("DogAttack"))
				{
					const Json& dog = c["DogAttack"];
					e.attack = EnemyAttack::Dog;
					e.attackRange = 0.0f; // it never stops: it runs into the player
					e.pounceRadius = number(dog, "PounceRadius", e.pounceRadius);
					e.runSpeed = number(first_row(dog, "RunSpeeds"), "RunSpeed", e.speed);
					e.spawnClipMs = clipMs(text(dog, "SpawnAnimation"));
					e.pounceClipMs = clipMs(text(dog, "PounceAnimation"));
				}
				else if (c.contains("ConstantAttack"))
				{
					const Json& constant = c["ConstantAttack"];
					e.attack = EnemyAttack::Constant;
					e.attackRange = number(constant, "AttackRange", e.attackRange);
					e.attackRadius = number(constant, "AttackRadius", e.attackRadius);
					e.runSpeed = number(first_row(constant, "RunSpeeds"), "RunSpeed", e.speed);
					e.attackClipMs = clipMs(text(constant, "Animation"));
				}
				if (c.contains("ZombieBear"))
				{
					const Json& bear = c["ZombieBear"];
					e.attack = EnemyAttack::Bear;
					if (c.contains("MeleeAttack")) e.attackRadius = number(c["MeleeAttack"], "AttackRadius", e.attackRange);
					e.sleepMs = number(bear, "SleepTime", 5000.0f);
					e.wakeClipMs = clipMs(text(bear, "SleepOutAnimation"));
					e.rearClipMs = clipMs(text(bear, "StandAnimation"));
					e.rearShare = number(bear, "LostPerDamageToReared", e.rearShare);
					e.abdomenMul = number(bear, "AbdomenDamageMul", 1.0f);
					if (c.contains("ChainAttack"))
					{
						const Json& chain = c["ChainAttack"];
						e.chainRadius = number(chain, "AttackRadius", e.chainRadius);
						e.chainRange = number(chain, "AttackRange", e.chainRange);
						e.chainDelayMs = number(chain, "DamageDelay", e.chainDelayMs);
						e.chainDamage = number(chain, "Damage", e.chainDamage);
						e.chainIntervalMinMs = number(chain, "AttackIntervalMin", e.chainIntervalMinMs);
						e.chainIntervalMaxMs = number(chain, "AttackIntervalMax", e.chainIntervalMaxMs);
						e.chainClipMs = clipMs(text(chain, "Animation"));
					}
				}
				if (c.contains("BuiltinCollision2DPolygon"))
				{
					// "Size": "150.0000, 150.0000" (width, height)
					e.collisionSize = static_cast<float>(std::atof(text(c["BuiltinCollision2DPolygon"], "Size", "150").c_str()));
				}
				if (c.contains("Health"))
				{
					const Json& health = c["Health"];
					e.fadeOut = flag(health, "IsFadeOut", e.fadeOut);
					e.showHealthBar = flag(health, "ShowHealthOnDamage", e.showHealthBar);
					e.fadeOutMs = number(health, "FadeOutTime", e.fadeOutMs);
					const std::string death = damage_sound(health, "PROJECTILE_DEATH");
					if (!death.empty()) e.deathSound = death;
					e.hurtSound = damage_sound(health, "DEFAULT_HURT");
					for (const char* hurtType : { "PROJECTILE_HURT", "DEFAULT_HURT" })
					{
						for (const auto& effect : health.value("DamageEffects", Json::array()))
						{
							if (e.hurtClipMs <= 0.0f && text(effect, "DamageType") == hurtType) e.hurtClipMs = clipMs(text(effect, "Animation"));
						}
					}
				}
				if (c.contains("PickupSpawn"))
				{
					const Json& spawn = c["PickupSpawn"];
					e.randomDrop = flag(spawn, "Random", false);
					e.drops = ids(spawn.value("Pickups", Json::array()), "Pickup");
					e.dropRate = number(spawn, "SpawnRate", 0.0f);
				}
				enemies[id] = e;
			}

			if (c.contains("PickupManager"))
			{
				randomPickups = ids(c["PickupManager"].value("Pickups", Json::array()), "Pickup");
			}

			if (c.contains("GiftBox"))
			{
				giftRadius = number(c["GiftBox"], "Radius", giftRadius);
				giftTable.clear();
				const Json table = c["GiftBox"].value("Consumables", Json::array());
				for (const auto& row : table)
				{
					giftTable.push_back({ static_cast<int>(number(row, "Consumable", 0.0f)), number(row, "Chance", 0.0f) });
				}
			}

			if (c.contains("Pickup") && !c.contains("Market"))
			{
				const Json& pickup = c["Pickup"];
				PickupRules p = original_pickup();
				p.type = text(pickup, "PickupType", p.type);
				p.value = number(pickup, "Value", p.value);
				p.radius = number(pickup, "Radius", p.radius);
				p.lifeTimeMs = number(pickup, "LifeTime", p.lifeTimeMs);
				p.sound = text(pickup, "PickupSound", p.sound);
				pickups[std::atoi(id.c_str())] = p;
			}

			if (c.contains("Market") && c.contains("Pickup"))
			{
				// it can also lie on the floor: picked up, it does what using it does
				PickupRules dropped;
				dropped.type = "Consumable";
				dropped.consumable = std::atoi(id.c_str());
				dropped.value = 0.0f;
				dropped.radius = 150.0f;
				dropped.lifeTimeMs = 15000.0f;
				pickups.emplace(dropped.consumable, dropped);
				ConsumableRules r;
				r.gun = original_gun(); // a lent firearm sounds like the Glock unless its data says otherwise
				r.id = std::atoi(id.c_str());
				r.name = name;
				const Json functions = c["Market"].value("Functions", Json::array());
				if (functions.is_array())
				{
					for (const auto& f : functions)
					{
						const std::string type = text(f, "valueType");
						const float value = number(f, "value", 0.0f);
						if (type == "ADD_HP") r.addHealth = value;
						else if (type == "ADD_SPEED") r.speedFactor = value;
						else if (type == "ADD_DAMAGE") r.damageFactor = value;
						else if (type == "ADD_DEFENCE") r.defenceFactor = value;
						else if (type == "TIMER") r.timerMs = value;
					}
				}
				if (c.contains("Effect")) r.effect = text(c["Effect"], "Type");
				if (c.contains("MeleeWeapon"))
				{
					const Json& melee = c["MeleeWeapon"];
					r.melee = true;
					r.meleeRules.reach = number(melee, "Reach", r.meleeRules.reach);
					r.meleeRules.attackRateMs = number(melee, "AttackRate", r.meleeRules.attackRateMs);
					r.meleeRules.damage = number(melee, "Damage", r.meleeRules.damage);
					r.meleeRules.runSpeed = number(melee, "RunSpeed", r.meleeRules.runSpeed);
					r.meleeRules.swingSound = text(melee, "SwingSound");
					r.meleeRules.circle = name == "Battle Axe" || name == "BattleAxe";
					const std::string death = text(melee, "DeathType");
					r.meleeRules.death = death == "FLING_DEATH" ? DeathKind::Fling : (death == "EXPLOSION_DEATH" ? DeathKind::Burst : DeathKind::Normal);
				}
				if (c.contains("FireArm"))
				{
					const Json& fire = c["FireArm"];
					r.firearm = true;
					r.gun.name = name;
					r.gun.fireRateMs = number(fire, "FireRate", r.gun.fireRateMs);
					r.gun.magazine = static_cast<int>(number(fire, "Magazine", static_cast<float>(r.gun.magazine)));
					r.gun.rounds = static_cast<int>(number(fire, "Rounds", static_cast<float>(r.gun.rounds)));
					r.gun.reloadMs = number(fire, "ReloadTime", r.gun.reloadMs);
					r.gun.runSpeed = number(fire, "RunSpeed", r.gun.runSpeed);
					r.gun.fireSpeed = number(fire, "FireSpeed", r.gun.fireSpeed);
					r.gun.dualMuzzles = flag(fire, "DualMuzzles", false);
					r.gun.bulletSpeed = number(fire, "BulletSpeed", r.gun.bulletSpeed);
					r.gun.bulletDamage = number(fire, "BulletDamage", r.gun.bulletDamage);
					r.gun.pellets = std::max(1, static_cast<int>(number(fire, "Pellets", 1.0f)));
					r.gun.spreadDegrees = number(fire, "SpreadDegrees", 0.0f);
					r.gun.knockback = flag(fire, "Knockback", false);
					r.gun.fireSound = text(fire, "FireSound", r.gun.fireSound);
					r.gun.reloadSound = text(fire, "ReloadSound", r.gun.reloadSound);
				}
				r.sound = text(c["Pickup"], "PickupSound");
				consumables[r.id] = r;
			}
		}

		// Props that break
		for (const auto& node : destructibles)
		{
			const Json c = node.value("components", Json::object());
			if (c.contains("Idiot")) continue; // ambient walkers are actors, not props
			PropRules p;
			p.id = node.value("id", -1);
			p.name = node.value("name", std::string());
			p.x = node.value("x", 0.0f);
			p.y = node.value("y", 0.0f);
			const Json health = c.value("Health", Json::object());
			p.health = number(health, "HealthValue", -1.0f);
			p.deathSound = damage_sound(health, "DEFAULT_DEATH");
			const Json levels = health.value("DamageLevels", Json::array());
			if (levels.is_array())
			{
				for (const auto& level : levels)
				{
					const std::string effect = text(level, "DamageEffect");
					if (!effect.empty()) p.damageLevels.push_back({ number(level, "HealthRate", 0.0f), effect });
				}
			}
			if (c.contains("PickupSpawn"))
			{
				const Json& spawn = c["PickupSpawn"];
				p.randomDrop = flag(spawn, "Random", false);
				p.drops = ids(spawn.value("Pickups", Json::array()), "Pickup");
				p.dropRate = number(spawn, "SpawnRate", 0.0f);
			}
			if (c.contains("Barricade"))
			{
				p.repairable = true;
				p.repairSeconds = number(c["Barricade"], "RepairTime", 4000.0f) / 1000.0f;
			}
			p.objective = c.contains("Objective");
			if (c.contains("Explosive"))
			{
				p.explosive = true;
				p.blastRadius = number(c["Explosive"], "BlastRadius", 0.0f);
				p.blastDamage = number(c["Explosive"], "BlastDamage", 0.0f);
				p.burningEffect = text(c["Explosive"], "DamagedEffect");
			}
			props.push_back(p);
		}

		if (enemies.empty()) warnings.push_back("no enemies in the rules");
		if (giftTable.empty()) warnings.push_back("no gift box table");
	}

	EnemyRules::Row EnemyRules::at(int difficulty, bool berserk) const
	{
		const size_t index = static_cast<size_t>(std::max(0, difficulty));
		if (berserk)
		{
			if (berserkRows.empty()) return { berserkSpeed, berserkHealth, berserkAttackDamage };
			return berserkRows[std::min(index, berserkRows.size() - 1)];
		}
		auto pick = [index](const std::vector<float>& rows, float first) { return rows.empty() ? first : rows[std::min(index, rows.size() - 1)]; };
		return { pick(speeds, speed), pick(healths, health), pick(attackDamages, attackDamage) };
	}

	namespace
	{
		// The game's events and the original's cues for them (the rules' "sounds" can replace any)
		const std::map<std::string, std::string> kDefaultSounds = {
			{ "propHit", "evt_prop_impact_bullet" },      // a bullet or a swing hits a prop (and each knock of a repair)
			{ "carBlast", "sfx_car_blow_up" },            // an explosive prop (a car) blows up
			{ "strikeBlast", "sfx_vehicle_explosion" },   // each blast of an air strike
			{ "healthPack", "sfx_health_pack" },          // a health pack without a sound of its own
			{ "meleeHit", "evt_zombie_swing_impact" },    // a melee weapon hits a zombie
			{ "playerSlowed", "sfx_player_slowdown" },    // a paparazzi's flash slows the player
			{ "playerDown", "m_mc_down" },
			{ "revive", "sfx_revive" },
			{ "enemyMoan", "evt_enemie_moan" },           // every few zombies coming in
			{ "bearWake", "sfx_zombiebear_sleep_out" },
			{ "bearRear", "sfx_zombiebear_stand_up" },
			{ "pickup", "sfx_pickup_zombucks" },          // money, tokens, the film reel
		};
	}

	GameRules::GameRules()
	{
		defaultSounds = kDefaultSounds;
		gun = original_gun();
		player.hurtSound = "evt_mc_hurt";
	}

	const EnemyRules& GameRules::enemy(const std::string& type) const
	{
		auto found = enemies.find(type);
		return found != enemies.end() ? found->second : m_fallbackEnemy;
	}
}
