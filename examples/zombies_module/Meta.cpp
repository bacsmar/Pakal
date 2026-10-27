#include "Meta.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace Zombies
{
	using Json = nlohmann::json;

	// --- Campaign ---

	bool Campaign::read(const Json& json, std::string& error)
	{
		missions.clear();
		weapons.clear();
		boosts.clear();
		for (const auto& m : json.value("missions", Json::array()))
		{
			MissionDef mission;
			mission.id = m.value("id", std::string());
			mission.title = m.value("title", mission.id);
			mission.brief = m.value("brief", std::string());
			mission.level = m.value("level", std::string());
			const Json reward = m.value("reward", Json::object());
			mission.rewardFirst = reward.value("first", 0);
			mission.rewardReplay = reward.value("replay", 0);
			mission.free = m.value("free", false);
			mission.side = m.value("side", false);
			mission.unlocks = m.value("unlocks", std::string());
			for (const auto& s : m.value("stars", Json::array()))
			{
				mission.stars.push_back({ s.value("type", std::string("complete")), s.value("value", 0.0f) });
			}
			if (mission.id.empty() || mission.level.empty())
			{
				error = "a mission without id or level";
				return false;
			}
			missions.push_back(std::move(mission));
		}
		for (const auto& w : json.value("weapons", Json::array()))
		{
			weapons.push_back({ w.value("id", std::string()), w.value("title", w.value("id", std::string())), w.value("consumable", 0),
				w.value("price", 0), w.value("upgradeBase", 1000) });
		}
		heroes.clear();
		for (const auto& h : json.value("heroes", Json::array()))
		{
			heroes.push_back({ h.value("id", std::string()), h.value("title", h.value("id", std::string())), h.value("brief", std::string()) });
		}
		if (heroes.empty()) heroes.push_back({ "sam", "Sam", "" });
		for (const auto& b : json.value("boosts", Json::array()))
		{
			boosts.push_back({ b.value("id", std::string()), b.value("title", b.value("id", std::string())), b.value("consumable", 0), b.value("price", 0) });
		}
		const Json upgrades = json.value("upgrades", Json::object());
		damagePerLevel = upgrades.value("damagePerLevel", damagePerLevel);
		fireRatePerLevel = upgrades.value("fireRatePerLevel", fireRatePerLevel);
		if (upgrades.contains("priceFactors")) upgradePriceFactors = upgrades["priceFactors"].get<std::vector<float>>();
		const Json health = json.value("health", Json::object());
		healthPerLevel = health.value("perLevel", healthPerLevel);
		if (health.contains("prices")) healthPrices = health["prices"].get<std::vector<int>>();
		if (missions.empty())
		{
			error = "the campaign has no missions";
			return false;
		}
		return true;
	}

	bool Campaign::load(const std::string& path, std::string& error)
	{
		std::ifstream in(path);
		if (!in)
		{
			error = "cannot open " + path;
			return false;
		}
		const Json json = Json::parse(in, nullptr, false);
		if (json.is_discarded())
		{
			error = "bad JSON in " + path;
			return false;
		}
		return read(json, error);
	}

	int Campaign::mission_index(const std::string& id) const
	{
		for (size_t i = 0; i < missions.size(); ++i) if (missions[i].id == id) return static_cast<int>(i);
		return -1;
	}

	const MissionDef* Campaign::mission(const std::string& id) const
	{
		const int index = mission_index(id);
		return index >= 0 ? &missions[static_cast<size_t>(index)] : nullptr;
	}

	const WeaponDef* Campaign::weapon(const std::string& id) const
	{
		for (const auto& w : weapons) if (w.id == id) return &w;
		return nullptr;
	}

	const BoostDef* Campaign::boost(const std::string& id) const
	{
		for (const auto& b : boosts) if (b.id == id) return &b;
		return nullptr;
	}

	const HeroDef* Campaign::hero(const std::string& id) const
	{
		for (const auto& h : heroes) if (h.id == id) return &h;
		return nullptr;
	}

	const MissionDef* Campaign::rescue_of(const std::string& hero) const
	{
		for (const auto& m : missions) if (m.unlocks == hero) return &m;
		return nullptr;
	}

	int Campaign::upgrade_price(const WeaponDef& weapon, int level) const
	{
		if (level < 1 || level > upgrade_levels()) return 0;
		return static_cast<int>(weapon.upgradeBase * upgradePriceFactors[static_cast<size_t>(level - 1)] + 0.5f);
	}

	int Campaign::health_price(int level) const
	{
		if (level < 1 || level > health_levels()) return 0;
		return healthPrices[static_cast<size_t>(level - 1)];
	}

	// --- Profile ---

	Json Profile::to_json() const
	{
		Json json;
		json["version"] = 1;
		json["money"] = money;
		Json records = Json::object();
		for (const auto& [id, r] : missions)
		{
			records[id] = { { "wins", r.wins }, { "stars", r.stars }, { "bestKills", r.bestKills }, { "bestSeconds", r.bestSeconds } };
		}
		json["missions"] = records;
		json["weapons"] = Json(std::vector<std::string>(weapons.begin(), weapons.end()));
		json["equipped"] = equipped;
		json["weaponLevels"] = weaponLevels;
		json["healthLevel"] = healthLevel;
		json["boosts"] = boosts;
		json["heroes"] = Json(std::vector<std::string>(heroes.begin(), heroes.end()));
		json["hero"] = hero;
		json["options"] = { { "quality", quality }, { "fsr", fsr }, { "sketch", sketch } };
		return json;
	}

	void Profile::from_json(const Json& json)
	{
		*this = Profile();
		money = json.value("money", 0);
		const Json records = json.value("missions", Json::object()); // items() of a temporary would not outlive the loop
		for (const auto& [id, r] : records.items())
		{
			MissionRecord record;
			record.wins = r.value("wins", 0);
			record.stars = r.value("stars", 0);
			record.bestKills = r.value("bestKills", 0);
			record.bestSeconds = r.value("bestSeconds", 0.0f);
			missions[id] = record;
		}
		for (const auto& w : json.value("weapons", Json::array())) weapons.insert(w.get<std::string>());
		equipped = json.value("equipped", std::string("Glock"));
		if (!weapons.count(equipped)) equipped = "Glock";
		weaponLevels = json.value("weaponLevels", std::map<std::string, int>());
		healthLevel = json.value("healthLevel", 0);
		boosts = json.value("boosts", std::map<std::string, int>());
		for (const auto& h : json.value("heroes", Json::array())) heroes.insert(h.get<std::string>());
		hero = json.value("hero", std::string("sam"));
		if (!heroes.count(hero)) hero = "sam";
		const Json options = json.value("options", Json::object());
		quality = std::clamp(options.value("quality", -1), -1, 3);
		fsr = options.value("fsr", true);
		sketch = options.value("sketch", false);
	}

	bool Profile::load(const std::string& path)
	{
		*this = Profile();
		std::ifstream in(path);
		if (!in) return false;
		const Json json = Json::parse(in, nullptr, false);
		if (json.is_discarded() || !json.is_object()) return false;
		from_json(json);
		return true;
	}

	bool Profile::save(const std::string& path) const
	{
		std::error_code error;
		const std::filesystem::path target(path);
		if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), error);
		const std::string side = path + ".new";
		{
			std::ofstream out(side, std::ios::trunc);
			if (!out) return false;
			out << to_json().dump(1, '\t');
			if (!out) return false;
		}
		std::filesystem::rename(side, target, error);
		return !error;
	}

	bool Profile::unlocked(const Campaign& campaign, size_t index) const
	{
		if (index >= campaign.missions.size()) return false;
		if (campaign.missions[index].free) return true;
		// open when the mission before it (not counting the free ones, which are never won, nor the side ones, which nothing
		// waits for) was won; the first always
		size_t before = index;
		while (before > 0 && (campaign.missions[before - 1].free || campaign.missions[before - 1].side)) --before;
		if (before == 0) return true;
		auto found = missions.find(campaign.missions[before - 1].id);
		return found != missions.end() && found->second.wins > 0;
	}

	int Profile::total_stars() const
	{
		int total = 0;
		for (const auto& [id, r] : missions) total += r.stars;
		return total;
	}

	int Profile::weapon_level(const std::string& id) const
	{
		auto found = weaponLevels.find(id);
		return found != weaponLevels.end() ? found->second : 0;
	}

	int Profile::boost_count(const std::string& id) const
	{
		auto found = boosts.find(id);
		return found != boosts.end() ? found->second : 0;
	}

	// --- Missions ---

	std::array<bool, 3> earned_stars(const MissionDef& mission, const MissionResult& result)
	{
		std::array<bool, 3> stars{};
		for (size_t i = 0; i < mission.stars.size() && i < stars.size(); ++i)
		{
			const StarRule& rule = mission.stars[i];
			bool ok = result.won;
			if (rule.type == "health") ok = ok && result.healthShare >= rule.value;
			else if (rule.type == "time") ok = ok && result.seconds <= rule.value;
			else if (rule.type == "kills") ok = ok && result.kills >= static_cast<int>(rule.value);
			stars[i] = ok;
		}
		return stars;
	}

	MissionReward settle(const Campaign& campaign, Profile& profile, const std::string& missionId, const MissionResult& result)
	{
		MissionReward reward;
		reward.moneyPicked = std::max(0, result.moneyPicked);
		profile.money += reward.moneyPicked;
		const MissionDef* mission = campaign.mission(missionId);
		if (mission && result.won)
		{
			MissionRecord& record = profile.missions[missionId];
			reward.firstWin = record.wins == 0;
			reward.reward = reward.firstWin ? mission->rewardFirst : mission->rewardReplay;
			reward.stars = earned_stars(*mission, result);
			for (bool s : reward.stars) reward.starCount += s ? 1 : 0;
			profile.money += reward.reward;
			++record.wins;
			record.stars = std::max(record.stars, reward.starCount);
			record.bestKills = std::max(record.bestKills, result.kills);
			if (record.bestSeconds <= 0.0f || result.seconds < record.bestSeconds) record.bestSeconds = result.seconds;
			if (!mission->unlocks.empty() && profile.heroes.insert(mission->unlocks).second) reward.heroRescued = mission->unlocks;
		}
		reward.moneyAfter = profile.money;
		return reward;
	}

	// --- Shop ---

	const char* purchase_text(Purchase purchase)
	{
		switch (purchase)
		{
		case Purchase::Ok: return "Done";
		case Purchase::NotEnoughMoney: return "Not enough money";
		case Purchase::AlreadyOwned: return "Already yours";
		case Purchase::MaxLevel: return "Fully upgraded";
		case Purchase::NotOwned: return "Buy it first";
		default: return "Not for sale";
		}
	}

	namespace
	{
		Purchase pay(Profile& profile, int price)
		{
			if (profile.money < price) return Purchase::NotEnoughMoney;
			profile.money -= price;
			return Purchase::Ok;
		}
	}

	Purchase buy_weapon(const Campaign& campaign, Profile& profile, const std::string& id)
	{
		const WeaponDef* weapon = campaign.weapon(id);
		if (!weapon) return Purchase::Unknown;
		if (profile.weapons.count(id)) return Purchase::AlreadyOwned;
		const Purchase paid = pay(profile, weapon->price);
		if (paid == Purchase::Ok)
		{
			profile.weapons.insert(id);
			profile.equipped = id;   // a new gun goes straight into the hands
		}
		return paid;
	}

	Purchase buy_upgrade(const Campaign& campaign, Profile& profile, const std::string& id)
	{
		const WeaponDef* weapon = campaign.weapon(id);
		if (!weapon) return Purchase::Unknown;
		if (!profile.weapons.count(id)) return Purchase::NotOwned;
		const int next = profile.weapon_level(id) + 1;
		if (next > campaign.upgrade_levels()) return Purchase::MaxLevel;
		const Purchase paid = pay(profile, campaign.upgrade_price(*weapon, next));
		if (paid == Purchase::Ok) profile.weaponLevels[id] = next;
		return paid;
	}

	Purchase buy_health(const Campaign& campaign, Profile& profile)
	{
		const int next = profile.healthLevel + 1;
		if (next > campaign.health_levels()) return Purchase::MaxLevel;
		const Purchase paid = pay(profile, campaign.health_price(next));
		if (paid == Purchase::Ok) profile.healthLevel = next;
		return paid;
	}

	Purchase buy_boost(const Campaign& campaign, Profile& profile, const std::string& id)
	{
		const BoostDef* boost = campaign.boost(id);
		if (!boost) return Purchase::Unknown;
		const Purchase paid = pay(profile, boost->price);
		if (paid == Purchase::Ok) ++profile.boosts[id];
		return paid;
	}

	bool equip(Profile& profile, const std::string& id)
	{
		if (!profile.weapons.count(id)) return false;
		profile.equipped = id;
		return true;
	}

	bool choose_hero(Profile& profile, const std::string& hero)
	{
		if (!profile.heroes.count(hero)) return false;
		profile.hero = hero;
		return true;
	}

	void apply_loadout(GameRules& rules, const Campaign& campaign, const Profile& profile)
	{
		rules.player.health += campaign.healthPerLevel * static_cast<float>(profile.healthLevel);
		const WeaponDef* weapon = campaign.weapon(profile.equipped);
		if (!weapon) return;
		GunRules gun = rules.gun;
		if (weapon->consumable != 0)
		{
			// a gun the levels lend from the chest, carried from the start with the level gun's ammunition
			auto lent = rules.consumables.find(weapon->consumable);
			if (lent != rules.consumables.end() && lent->second.firearm)
			{
				gun = lent->second.gun;
				gun.rounds = std::max(gun.rounds, rules.gun.rounds);
			}
		}
		const int level = profile.weapon_level(weapon->id);
		gun.bulletDamage *= 1.0f + campaign.damagePerLevel * static_cast<float>(level);
		gun.fireRateMs /= 1.0f + campaign.fireRatePerLevel * static_cast<float>(level);
		rules.gun = gun;
	}
}
