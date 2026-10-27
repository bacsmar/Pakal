// What lasts between missions (examples/zombies_module/Meta): the campaign's order, rewards, stars, the shop and the loadout
#include "Meta.h"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace Zombies;

int main(int argc, char** argv)
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		std::fflush(stdout);
		if (!ok) ++failures;
	};

	const nlohmann::json json = nlohmann::json::parse(R"({
		"missions": [
			{ "id": "a", "level": "a.json", "reward": { "first": 400, "replay": 80 },
			  "stars": [ { "type": "complete" }, { "type": "health", "value": 0.5 }, { "type": "time", "value": 300 } ] },
			{ "id": "b", "level": "b.json", "reward": { "first": 500, "replay": 100 },
			  "stars": [ { "type": "complete" }, { "type": "kills", "value": 80 } ] }
		],
		"weapons": [ { "id": "Glock", "price": 0, "upgradeBase": 400 }, { "id": "SMG", "consumable": 15101, "price": 600, "upgradeBase": 600 } ],
		"boosts": [ { "id": "health", "consumable": 15020, "price": 120 } ]
	})");
	Campaign campaign;
	std::string error;
	check(campaign.read(json, error) && campaign.missions.size() == 2 && campaign.weapons.size() == 2, "the campaign reads its missions and weapons");

	// The campaign file of the game reads too
	if (argc > 1)
	{
		Campaign game;
		check(game.load(argv[1], error) && game.missions.size() >= 3, "game/campaign.json reads");
	}

	Profile profile;
	check(profile.unlocked(campaign, 0) && !profile.unlocked(campaign, 1), "only the first mission is open at the start");

	// A loss keeps what was picked up, gives no reward and opens nothing
	MissionResult lost;
	lost.moneyPicked = 35;
	MissionReward r = settle(campaign, profile, "a", lost);
	check(profile.money == 35 && r.reward == 0 && !profile.unlocked(campaign, 1), "a lost mission keeps the money picked up, nothing else");

	// A win: the first reward, the stars it met, the next mission opens
	MissionResult won;
	won.won = true;
	won.kills = 60;
	won.moneyPicked = 40;
	won.healthShare = 0.7f;
	won.seconds = 350.0f;
	r = settle(campaign, profile, "a", won);
	check(r.firstWin && r.reward == 400 && profile.money == 35 + 40 + 400, "the first win pays the first reward");
	check(r.stars[0] && r.stars[1] && !r.stars[2] && r.starCount == 2 && profile.missions["a"].stars == 2, "stars: won, with health, not in time");
	check(profile.unlocked(campaign, 1), "winning a mission opens the next");
	won.seconds = 200.0f;
	won.healthShare = 0.2f;
	r = settle(campaign, profile, "a", won);
	check(!r.firstWin && r.reward == 80 && r.starCount == 2 && profile.missions["a"].stars == 2 && std::fabs(profile.missions["a"].bestSeconds - 200.0f) < 0.01f,
		"a replay pays less and the best stars and time stay");

	// The shop
	profile.money = 1000;
	check(buy_weapon(campaign, profile, "SMG") == Purchase::Ok && profile.money == 400 && profile.equipped == "SMG", "buying a gun equips it");
	check(buy_weapon(campaign, profile, "SMG") == Purchase::AlreadyOwned, "a gun is bought once");
	check(buy_upgrade(campaign, profile, "SMG") == Purchase::Ok && profile.weapon_level("SMG") == 1 && profile.money == 250, "an upgrade costs base x factor");
	profile.money = 0;
	check(buy_upgrade(campaign, profile, "SMG") == Purchase::NotEnoughMoney && profile.weapon_level("SMG") == 1, "no money, no upgrade");
	profile.money = 500;
	check(buy_boost(campaign, profile, "health") == Purchase::Ok && profile.boost_count("health") == 1, "a boost is carried");
	check(buy_health(campaign, profile) == Purchase::Ok && profile.healthLevel == 1, "more health");

	// The loadout: the SMG lent by the level becomes the gun, upgraded; the health goes up
	GameRules rules;
	rules.gun.rounds = 1000;
	ConsumableRules smg;
	smg.id = 15101;
	smg.firearm = true;
	smg.gun.name = "SMG";
	smg.gun.bulletDamage = 1.0f;
	smg.gun.fireRateMs = 100.0f;
	smg.gun.rounds = 200;
	rules.consumables[15101] = smg;
	const float health = rules.player.health;
	apply_loadout(rules, campaign, profile);
	check(rules.gun.name == "SMG" && std::fabs(rules.gun.bulletDamage - 1.15f) < 1e-4f && rules.gun.rounds == 1000
		&& std::fabs(rules.gun.fireRateMs - 100.0f / 1.04f) < 1e-3f, "the equipped gun with its upgrade");
	check(std::fabs(rules.player.health - (health + 10.0f)) < 1e-4f, "the bought health");

	// Saved and read back
	const std::string path = (std::filesystem::temp_directory_path() / "pakal_meta_test" / "profile.json").string();
	Profile back;
	check(profile.save(path) && back.load(path) && back.money == profile.money && back.equipped == "SMG" && back.weapon_level("SMG") == 1
		&& back.missions["a"].wins == 2 && back.boost_count("health") == 1, "the profile is saved and read back");
	std::filesystem::remove_all(std::filesystem::path(path).parent_path());

	// Side missions and heroes: a side mission opens with the one before it and the next does not wait for it; winning it
	// rescues its hero, who can then be played
	{
		const nlohmann::json withSide = nlohmann::json::parse(R"({
			"missions": [
				{ "id": "a", "level": "a.json" },
				{ "id": "s", "level": "s.json", "side": true, "unlocks": "lis" },
				{ "id": "b", "level": "b.json" } ],
			"heroes": [ { "id": "sam", "title": "Sam" }, { "id": "lis", "title": "Lis" } ] })");
		Campaign sides;
		std::string sideError;
		check(sides.read(withSide, sideError) && sides.heroes.size() == 2 && sides.missions[1].side && sides.rescue_of("lis") == &sides.missions[1],
			"the campaign reads side missions and heroes");
		Profile player;
		check(!player.unlocked(sides, 1) && !player.unlocked(sides, 2), "a side mission waits for the one before it");
		MissionResult win;
		win.won = true;
		settle(sides, player, "a", win);
		check(player.unlocked(sides, 1) && player.unlocked(sides, 2), "winning it opens the side mission and the next one");
		check(!choose_hero(player, "lis") && player.hero == "sam", "a hero not rescued cannot be played");
		const MissionReward rescued = settle(sides, player, "s", win);
		check(rescued.heroRescued == "lis" && choose_hero(player, "lis") && player.hero == "lis", "the side mission rescues its hero");
		Profile reread;
		reread.from_json(player.to_json());
		check(reread.heroes.count("lis") && reread.hero == "lis", "the heroes are saved");
	}
	return failures ? 1 : 0;
}
