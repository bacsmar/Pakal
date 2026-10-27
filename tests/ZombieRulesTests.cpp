// Checks the game rules read from the converted level (gift table, drops, enemy values) and the
// mouse ray. Run with the path of a level.json (the test is skipped without it).
#include "../examples/zombies_module/GameRules.h"
#include "topdown/MouseAim.h"

#include <cmath>
#include <cstdio>
#include <fstream>

namespace
{
	int failures = 0;

	void check(bool ok, const char* what)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
		if (!ok) ++failures;
	}

	bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
}

int main(int argc, char** argv)
{
	// Mouse ray: the camera of the game looks at a point from (-12.5, 17.1, 12.5) away; the pixel at the
	// center of the view hits the floor right under the target
	{
		const Pakal::TopDown::Vec3 target{ 10.0f, 0.9f, -20.0f };
		const Pakal::TopDown::Vec3 eye{ target.x - 12.5f, target.y + 17.1f, target.z + 12.5f };
		float x = 0.0f, y = 0.0f;
		const bool hit = Pakal::TopDown::mouse_on_plane(eye, target, 22.0f, 639.5f, 359.5f, 1280.0f, 720.0f, 0.9f, x, y);
		check(hit && near(x, 10.0f) && near(y, 20.0f), "the center pixel aims at the target");
		// Right of the center is toward the south-east on the floor (screen right = +x, -y)
		Pakal::TopDown::mouse_on_plane(eye, target, 22.0f, 1000.0f, 359.5f, 1280.0f, 720.0f, 0.9f, x, y);
		check(x > 10.0f && y < 20.0f, "a pixel to the right aims south-east of the target");
		check(!Pakal::TopDown::mouse_on_plane(eye, target, 22.0f, 640.0f, 0.0f, 1280.0f, 720.0f, 1000.0f, x, y), "a ray that never reaches the plane misses");
	}

	// A building between the camera and the player: the segment crosses its box; one beside the line of sight does not
	{
		const Pakal::TopDown::Vec3 eye{ -12.5f, 17.0f, 12.5f }, player{ 0.0f, 1.3f, 0.0f };
		check(Zombies::segment_hits_box(eye, player, { -8.0f, 0.0f, 3.0f }, { -2.0f, 20.0f, 9.0f }), "a building on the line of sight is hit");
		check(!Zombies::segment_hits_box(eye, player, { 5.0f, 0.0f, 3.0f }, { 12.0f, 20.0f, 9.0f }), "a building beside it is not");
		check(!Zombies::segment_hits_box(eye, player, { -8.0f, 0.0f, 3.0f }, { -2.0f, 1.0f, 9.0f }), "a low wall under the line is not");
		check(Zombies::segment_hits_box(eye, player, { -20.0f, 10.0f, 5.0f }, { -5.0f, 25.0f, 20.0f }), "the camera inside a building counts");
	}

	// Gift table: chances are walked in order, the last entry takes what is left
	{
		Zombies::GameRules rules;
		rules.giftTable = { { 1, 0.5f }, { 2, 0.25f }, { 3, 0.25f } };
		check(rules.pick_gift(0.0f) == 1 && rules.pick_gift(0.49f) == 1, "gift: the first 50% is entry 1");
		check(rules.pick_gift(0.5f) == 2 && rules.pick_gift(0.74f) == 2, "gift: the next 25% is entry 2");
		check(rules.pick_gift(0.76f) == 3 && rules.pick_gift(0.99f) == 3, "gift: the rest is entry 3");
		check(rules.pick_gift(5.0f) == 3, "gift: the last entry wins when the chances run out");
		rules.randomPickups = { 7 };
		check(rules.pick_drop(true, {}, 0.5f, 0.2f, 0.9f) == 7, "drop: a random drop picks from the manager's list");
		check(rules.pick_drop(true, {}, 0.5f, 0.6f, 0.0f) == 0, "drop: nothing drops above the rate");
		check(rules.pick_drop(false, {}, 1.0f, 0.0f, 0.0f) == 0, "drop: a listed drop with no list gives nothing");
		check(rules.pick_drop(false, { 4, 5 }, 1.0f, 0.0f, 0.9f) == 5, "drop: a listed drop picks from its list");
	}

	// What the data leaves out is the original game's (the TopDown rules have no names of their own)
	{
		Zombies::GameRules rules;
		check(rules.gun.name == "Glock" && rules.gun.fireSound == "evt_weapon_dualglock_shoot"
			&& rules.gun.reloadSound == "sfx_weapon_dualglock_reload", "defaults: the dual Glock and its sounds");
		check(rules.player.hurtSound == "evt_mc_hurt", "defaults: the player's hurt sound");
		const auto rulesJson = nlohmann::ordered_json::parse(R"({"objects": {"20001": {"name": "Coin", "components": {
			"Pickup": {"Value": 5.0}}}}})");
		rules.read(rulesJson, nlohmann::ordered_json::array());
		const auto coin = rules.pickups.find(20001);
		check(coin != rules.pickups.end() && coin->second.type == "Money" && coin->second.sound == "sfx_pickup_zombucks",
			"defaults: a pickup without a type is money, with the coin's sound");
		check(rules.sound("propHit") == "evt_prop_impact_bullet", "defaults: the events' sounds are the original's cues");
	}

	// A firearm a consumable lends (FireArm: the Downtown missions add them to the original data)
	{
		const auto rulesJson = nlohmann::ordered_json::parse(R"({"objects": {"15102": {"name": "Shotgun", "components": {
			"Market": {"Functions": [{"value": 25000.0, "valueType": "TIMER"}]},
			"Pickup": {"PickupSound": "sfx_pickup"},
			"FireArm": {"FireRate": 700.0, "Magazine": 6, "Rounds": 36, "ReloadTime": 1400.0, "BulletDamage": 0.3, "Pellets": 7, "SpreadDegrees": 28.0}}}}})");
		Zombies::GameRules rules;
		rules.read(rulesJson, nlohmann::ordered_json::array());
		const auto found = rules.consumables.find(15102);
		check(found != rules.consumables.end() && found->second.firearm && !found->second.melee, "a FireArm object is a firearm consumable");
		if (found != rules.consumables.end())
		{
			const Zombies::GunRules& gun = found->second.gun;
			check(gun.pellets == 7 && near(gun.spreadDegrees, 28.0f) && gun.magazine == 6 && gun.rounds == 36 && near(gun.bulletDamage, 0.3f), "the shotgun fires 7 pellets in 28 degrees");
			check(near(found->second.timerMs, 25000.0f) && gun.name == "Shotgun" && !gun.dualMuzzles, "it lasts 25 s, has its own name and one muzzle");
		}
	}

	// The values of the original game, from a converted level
	if (argc > 1)
	{
		std::ifstream file(argv[1]);
		const auto root = nlohmann::ordered_json::parse(file, nullptr, false);
		check(root.is_object(), "level.json parses");
		if (root.is_object())
		{
			Zombies::GameRules rules;
			rules.read(root.value("rules", nlohmann::ordered_json::object()), root.value("destructibles", nlohmann::ordered_json::array()));
			check(rules.warnings.empty(), "no rule is missing");
			check(near(rules.gun.bulletDamage, 0.39f) && near(rules.gun.fireRateMs, 350.0f) && rules.gun.magazine == 20, "Glock: 0.39 damage, 350 ms, 20 rounds");
			check(near(rules.gun.bulletSpeed, 2000.0f) && near(rules.gun.reloadMs, 750.0f), "Glock: bullets at 2000 cm/s, 750 ms reload");
			check(near(rules.player.health, 80.0f) && near(rules.player.invincibleAfterHitMs, 250.0f), "player: 80 health, 250 ms invincible after a hit");
			const auto& business = rules.enemy("25001");
			check(near(business.speed, 200.0f) && near(business.health, 1.5f) && near(business.attackDamage, 3.0f), "ZombieBusiness: 200 cm/s, 1.5 health, 3 attack");
			check(near(business.berserkSpeed, 450.0f) && near(business.berserkHealth, 1.125f) && near(business.berserkAttackDamage, 4.5f), "ZombieBusiness berserk: 450 cm/s, 1.125 health, 4.5 attack");
			check(near(business.attackDelayMs, 500.0f) && near(business.attackRange, 250.0f) && !business.flash, "ZombieBusiness: melee 250 cm, 500 ms delay");
			const auto& paparazzi = rules.enemy("25009");
			check(paparazzi.flash && near(paparazzi.attackAngle, 50.0f) && near(paparazzi.attackTimeMs, 2250.0f) && near(paparazzi.attackDelayMs, 1250.0f), "Paparazzi: flash in 50 degrees, 2250 ms, hit at 1250 ms");
			check(rules.giftTable.size() == 15, "gift box: 15 entries");
			float total = 0.0f;
			for (const auto& entry : rules.giftTable) total += entry.chance;
			check(near(total, 1.45f), "gift box: the chances add to 1.45");
			check(rules.consumables.count(63) && rules.consumables.at(63).melee && near(rules.consumables.at(63).timerMs, 5000.0f), "Battle Axe: melee for 5 s");
			check(rules.consumables.count(15020) && near(rules.consumables.at(15020).addHealth, 150.0f), "MediumHealth adds 150");
			check(rules.pickups.count(25006) && near(rules.pickups.at(25006).value, 5.0f) && near(rules.pickups.at(25006).lifeTimeMs, 10000.0f), "money: 5 for 10 s");
			check(rules.randomPickups.size() == 1 && rules.randomPickups[0] == 25006, "random drops are money");
			int explosive = 0, breakable = 0;
			for (const auto& prop : rules.props)
			{
				if (prop.explosive && near(prop.blastRadius, 700.0f) && near(prop.blastDamage, 500.0f)) ++explosive;
				if (prop.health > 0.0f) ++breakable;
			}
			check(explosive == 19, "19 explosive cars: 700 cm blast, 500 damage");
			check(breakable > 40, "props that break are read");
		}
	}
	std::printf(failures == 0 ? "ALL PASSED\n" : "%d FAILED\n", failures);
	return failures == 0 ? 0 : 1;
}
