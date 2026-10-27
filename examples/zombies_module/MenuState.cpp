#include "MenuState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "ZombiesLevelState.h"
#include "CharacterViewer.h"
#include "bgfx/BgfxGraphicsSystem.h"

#include <sstream>

#include <cstdio>
#include <cstdlib>

namespace Zombies
{
	namespace
	{
		const float kSketchStrength = 0.85f;   // the sketch look when the option is on
		const unsigned kMenuDocument = 3;

		std::string escape(const std::string& text)
		{
			std::string out;
			for (char c : text)
			{
				if (c == '<') out += "&lt;";
				else if (c == '>') out += "&gt;";
				else if (c == '&') out += "&amp;";
				else out += c;
			}
			return out;
		}

		std::string money_text(int amount)
		{
			// 12500 -> "12,500"
			std::string digits = std::to_string(amount < 0 ? -amount : amount), out;
			for (size_t i = 0; i < digits.size(); ++i)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
				out += digits[i];
			}
			return (amount < 0 ? "-$" : "$") + out;
		}

		std::string stars_markup(int count)
		{
			std::string out;
			for (int i = 0; i < 3; ++i) out += i < count ? "<img src=\"images/star_on.png\"/>" : "<img src=\"images/star_off.png\"/>";
			return out;
		}
	}

	// --- Session ---

	std::string Session::resolve(const std::string& path)
	{
		const std::string& root = ResourceMgr.get_root_path();
		if (root.empty() || root == "." || (!path.empty() && (path[0] == '/' || path.find(':') != std::string::npos))) return path;
		return root + "/" + path;
	}

	namespace
	{
		std::string profile_path()
		{
			const char* custom = std::getenv("ZOMBIES_PROFILE"); // debug: another profile file
			return custom && *custom ? std::string(custom) : Session::resolve("save/profile.json");
		}
	}

	Session& Session::get()
	{
		static Session session;
		if (!session.campaignLoaded)
		{
			std::string error;
			session.campaignLoaded = session.campaign.load(resolve("Assets/game/campaign.json"), error);
			if (!session.campaignLoaded) { LOG_ERROR("[Menu] campaign: %s", error.c_str()); }
			if (session.profile.load(profile_path())) { LOG_INFO("[Menu] profile: %s, %d money", profile_path().c_str(), session.profile.money); }
			else { LOG_INFO("[Menu] new profile (%s)", profile_path().c_str()); }
		}
		return session;
	}

	void Session::save() const
	{
		if (!profile.save(profile_path())) { LOG_ERROR("[Menu] the profile was not saved to %s", profile_path().c_str()); }
	}

	void Session::apply_options(Pakal::Engine* engine) const
	{
		auto* graphics = engine ? dynamic_cast<Pakal::BgfxGraphicsSystem*>(engine->graphics_system()) : nullptr;
		if (!graphics) return;
		using Quality = Pakal::BgfxGraphicsSystem::Quality;
		graphics->set_quality(profile.quality < 0 ? Pakal::BgfxGraphicsSystem::default_quality() : static_cast<Quality>(profile.quality));
		graphics->set_fsr(profile.fsr);
		// the sketch look; ZOMBIES_SKETCH=<strength> to try it
		float sketch = profile.sketch ? kSketchStrength : 0.0f;
		if (const char* env = std::getenv("ZOMBIES_SKETCH")) sketch = static_cast<float>(std::atof(env));
		graphics->set_sketch(sketch);
	}

	std::string Session::options_markup() const
	{
		static const char* names[] = { "AUTO", "LOW", "MEDIUM", "HIGH", "ULTRA" };
		std::string markup = "<div class=\"option_row\"><div class=\"option_label\">QUALITY</div>";
		for (int q = -1; q <= 3; ++q)
		{
			markup += "<div id=\"opt_q" + std::to_string(q + 1) + "\" class=\"button small" + (profile.quality == q ? "" : " secondary")
				+ "\">" + names[q + 1] + "</div>";
		}
		markup += "</div><div class=\"option_row\"><div class=\"option_label\">FSR</div>";
		markup += "<div id=\"opt_fsr_on\" class=\"button small" + std::string(profile.fsr ? "" : " secondary") + "\">ON</div>";
		markup += "<div id=\"opt_fsr_off\" class=\"button small" + std::string(profile.fsr ? " secondary" : "") + "\">OFF</div></div>";
		markup += "<div class=\"option_row\"><div class=\"option_label\">LOOK</div>";
		markup += "<div id=\"opt_look_clean\" class=\"button small" + std::string(profile.sketch ? " secondary" : "") + "\">CLEAN</div>";
		markup += "<div id=\"opt_look_sketch\" class=\"button small" + std::string(profile.sketch ? "" : " secondary") + "\">SKETCH</div></div>";
		markup += "<div class=\"option_note\">FSR draws the picture smaller and rebuilds it to the screen: smoother, a little softer. "
			"SKETCH draws the town as an etching on old paper.</div>";
		return markup;
	}

	bool Session::handle_option(const std::string& element, Pakal::Engine* engine)
	{
		if (element.rfind("opt_q", 0) == 0) profile.quality = std::clamp(std::atoi(element.c_str() + 5) - 1, -1, 3);
		else if (element == "opt_fsr_on") profile.fsr = true;
		else if (element == "opt_fsr_off") profile.fsr = false;
		else if (element == "opt_look_clean") profile.sketch = false;
		else if (element == "opt_look_sketch") profile.sketch = true;
		else return false;
		apply_options(engine);
		save();
		return true;
	}

	// --- MenuState ---

	MenuState::MenuState(Screen screen) : BaseGameState("ZombiesMenu"), m_screen(screen)
	{
	}

	void MenuState::on_initialize(Pakal::Engine* engine)
	{
		Session::get();
		m_engine = engine;
		Session::get().apply_options_once(engine);   // the saved options
		if (const char* clicks = std::getenv("ZOMBIES_MENU_CLICKS"))
		{
			std::stringstream list(clicks);
			for (std::string id; std::getline(list, id, ',');) if (!id.empty()) m_scripted.push_back(id);
		}
		m_ui = engine->get_ui_manager();
		if (!m_ui || !m_ui->load_document(kMenuDocument, "Assets/ui/menu.rml"))
		{
			LOG_ERROR("[Menu] Assets/ui/menu.rml did not load");
			m_ui = nullptr;
			return;
		}
#if defined(__ANDROID__)
		m_ui->set_class(kMenuDocument, "menu", "touch", true);
#endif
		m_ui->show_document(kMenuDocument);
		m_clicks = m_ui->add_click_listener(kMenuDocument, [this](const std::string& element)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_pending.push_back(element);
		});
		show(m_screen);
	}

	void MenuState::on_terminate(Pakal::Engine* engine)
	{
		(void)engine;
		if (!m_ui) return;
		m_ui->remove_click_listener(m_clicks);
		m_ui->unload_document(kMenuDocument);
	}

	void MenuState::on_update(unsigned long dtMilliseconds)
	{
		m_scriptClock += static_cast<float>(dtMilliseconds) / 1000.0f;
		if (m_scriptClock >= 0.5f)
		{
			m_scriptClock = 0.0f;
			if (!m_scripted.empty())
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_pending.push_back(m_scripted.front());
				m_scripted.erase(m_scripted.begin());
			}
			else if (const char* shot = std::getenv("ZOMBIES_MENU_SHOT"); shot && *shot && !m_shotTaken)
			{
				m_shotTaken = true;
				auto* graphics = static_cast<Pakal::BgfxGraphicsSystem*>(m_engine->graphics_system());
				const std::string path = shot;
				graphics->execute_block([graphics, path]() { graphics->request_screenshot(path.c_str()); });
			}
		}
		std::vector<std::string> clicks;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			clicks.swap(m_pending);
		}
		for (const std::string& element : clicks)
		{
			// A mission starts: this state is destroyed, nothing after it
			if (element.rfind("mission_", 0) == 0)
			{
				Session& session = Session::get();
				const size_t index = static_cast<size_t>(std::atoi(element.c_str() + 8));
				if (index < session.campaign.missions.size() && session.profile.unlocked(session.campaign, index))
				{
					const MissionDef& mission = session.campaign.missions[index];
					LOG_INFO("[Menu] mission %s (%s)", mission.id.c_str(), mission.level.c_str());
					get_manager()->transition_to_state(new ZombiesLevelState(Session::resolve(mission.level), mission.id), true);
					return;
				}
				continue;
			}
			handle(element);
		}
	}

	void MenuState::show(Screen screen)
	{
		m_screen = screen;
		if (!m_ui) return;
		m_ui->set_class(kMenuDocument, "screen_main", "hidden", screen != Screen::Main);
		m_ui->set_class(kMenuDocument, "screen_missions", "hidden", screen != Screen::Missions);
		m_ui->set_class(kMenuDocument, "screen_shop", "hidden", screen != Screen::Shop);
		m_ui->set_class(kMenuDocument, "screen_heroes", "hidden", screen != Screen::Heroes);
		m_ui->set_class(kMenuDocument, "screen_options", "hidden", screen != Screen::Options);
		const Session& session = Session::get();
		m_ui->set_text(kMenuDocument, "money", money_text(session.profile.money));
		char stars[32];
		int total = 0;
		for (const auto& mission : session.campaign.missions) if (!mission.free) total += 3; // a free level has no stars
		std::snprintf(stars, sizeof(stars), "%d / %d", session.profile.total_stars(), total);
		m_ui->set_text(kMenuDocument, "stars_total", stars);
		if (screen == Screen::Missions) fill_missions();
		if (screen == Screen::Shop) fill_shop();
		if (screen == Screen::Heroes) fill_heroes();
		if (screen == Screen::Options) m_ui->set_markup(kMenuDocument, "options_box", session.options_markup());
	}

	void MenuState::fill_heroes()
	{
		const Session& session = Session::get();
		std::string markup;
		for (const HeroDef& hero : session.campaign.heroes)
		{
			const bool rescued = session.profile.heroes.count(hero.id) != 0;
			const bool playing = session.profile.hero == hero.id;
			markup += "<div id=\"hero_" + hero.id + "\" class=\"card hero" + (rescued ? "" : " locked") + "\">";
			markup += "<img class=\"hero_portrait\" src=\"images/hero_" + hero.id + ".png\"/>";
			markup += "<div class=\"card_title title\">" + escape(hero.title) + "</div>";
			markup += "<div class=\"card_brief\">" + escape(hero.brief) + "</div>";
			if (playing)
			{
				markup += "<div class=\"tag\">PLAYING</div>";
			}
			else if (rescued)
			{
				markup += "<div class=\"card_reward\">Tap to play as " + escape(hero.title) + "</div>";
			}
			else
			{
				const MissionDef* rescue = session.campaign.rescue_of(hero.id);
				markup += "<div class=\"card_reward\">" + (rescue ? "Side mission: " + escape(rescue->title) : std::string("Not rescued yet")) + "</div>";
			}
			markup += "</div>";
		}
		m_ui->set_markup(kMenuDocument, "hero_cards", markup);
	}

	void MenuState::fill_missions()
	{
		const Session& session = Session::get();
		std::string markup;
		for (size_t i = 0; i < session.campaign.missions.size(); ++i)
		{
			const MissionDef& mission = session.campaign.missions[i];
			const bool open = session.profile.unlocked(session.campaign, i);
			auto record = session.profile.missions.find(mission.id);
			const int stars = record != session.profile.missions.end() ? record->second.stars : 0;
			const bool won = record != session.profile.missions.end() && record->second.wins > 0;
			markup += "<div id=\"mission_" + std::to_string(i) + "\" class=\"card" + (open ? "" : " locked") + "\">";
			markup += "<div class=\"card_number title\">" + std::to_string(i + 1) + "</div>";
			markup += "<div class=\"card_title title\">" + escape(mission.title) + "</div>";
			markup += "<div class=\"card_brief\">" + escape(mission.brief) + "</div>";
			if (mission.side)
			{
				const HeroDef* hero = session.campaign.hero(mission.unlocks);
				markup += "<div class=\"card_side\">SIDE MISSION" + (hero ? " &middot; rescue " + escape(hero->title) : std::string()) + "</div>";
			}
			if (mission.free)
			{
				markup += "<div class=\"card_reward\">Always open: no stars, no reward</div>";
			}
			else if (open)
			{
				markup += "<div class=\"card_stars\">" + stars_markup(stars) + "</div>";
				markup += "<div class=\"card_reward\">" + std::string(won ? "Replay " : "Reward ")
					+ money_text(won ? mission.rewardReplay : mission.rewardFirst) + "</div>";
			}
			else
			{
				markup += "<div class=\"card_lock\"><img src=\"images/lock.png\"/></div><div class=\"card_reward\">Win the mission before</div>";
			}
			markup += "</div>";
		}
		m_ui->set_markup(kMenuDocument, "mission_cards", markup);
	}

	void MenuState::fill_shop()
	{
		const Session& session = Session::get();
		const Campaign& campaign = session.campaign;
		const Profile& profile = session.profile;
		std::string markup;
		for (const WeaponDef& weapon : campaign.weapons)
		{
			const bool owned = profile.weapons.count(weapon.id) != 0;
			const int level = profile.weapon_level(weapon.id);
			markup += "<div class=\"row\"><div class=\"row_title title\">" + escape(weapon.title) + "</div>";
			char info[96];
			std::snprintf(info, sizeof(info), "Damage +%.0f%%, fire rate +%.0f%% &nbsp; Level %d/%d",
				100.0f * campaign.damagePerLevel * level, 100.0f * campaign.fireRatePerLevel * level, level, campaign.upgrade_levels());
			markup += "<div class=\"row_info\">" + std::string(owned ? info : "Not yours yet") + "</div><div class=\"row_actions\">";
			if (!owned)
			{
				markup += "<div id=\"buy_" + weapon.id + "\" class=\"button small" + (profile.money < weapon.price ? " poor" : "") + "\">BUY "
					+ money_text(weapon.price) + "</div>";
			}
			else
			{
				markup += profile.equipped == weapon.id ? "<div class=\"tag\">IN HAND</div>"
					: "<div id=\"equip_" + weapon.id + "\" class=\"button small secondary\">EQUIP</div>";
				if (level < campaign.upgrade_levels())
				{
					const int price = campaign.upgrade_price(weapon, level + 1);
					markup += "<div id=\"up_" + weapon.id + "\" class=\"button small" + (profile.money < price ? " poor" : "") + "\">UPGRADE "
						+ money_text(price) + "</div>";
				}
				else
				{
					markup += "<div class=\"tag\">MAX</div>";
				}
			}
			markup += "</div></div>";
		}
		m_ui->set_markup(kMenuDocument, "shop_weapons", markup);

		markup.clear();
		{
			char info[96];
			std::snprintf(info, sizeof(info), "+%.0f health &nbsp; Level %d/%d", campaign.healthPerLevel * profile.healthLevel, profile.healthLevel,
				campaign.health_levels());
			markup += "<div class=\"row\"><div class=\"row_title title\">Toughness</div><div class=\"row_info\">" + std::string(info)
				+ "</div><div class=\"row_actions\">";
			if (profile.healthLevel < campaign.health_levels())
			{
				const int price = campaign.health_price(profile.healthLevel + 1);
				markup += "<div id=\"health_up\" class=\"button small" + std::string(profile.money < price ? " poor" : "") + "\">TRAIN "
					+ money_text(price) + "</div>";
			}
			else
			{
				markup += "<div class=\"tag\">MAX</div>";
			}
			markup += "</div></div>";
		}
		for (const BoostDef& boost : campaign.boosts)
		{
			markup += "<div class=\"row\"><div class=\"row_title title\">" + escape(boost.title) + "</div><div class=\"row_info\">Carried: "
				+ std::to_string(profile.boost_count(boost.id)) + "</div><div class=\"row_actions\"><div id=\"boost_" + boost.id + "\" class=\"button small"
				+ (profile.money < boost.price ? " poor" : "") + "\">BUY " + money_text(boost.price) + "</div></div></div>";
		}
		m_ui->set_markup(kMenuDocument, "shop_other", markup);
		m_ui->set_text(kMenuDocument, "shop_message", m_message);
	}

	void MenuState::handle(const std::string& element)
	{
		Session& session = Session::get();
		auto bought = [&](Purchase result)
		{
			m_message = purchase_text(result);
			if (result == Purchase::Ok) session.save();
			show(Screen::Shop);
		};
		if (element == "nav_play") show(Screen::Missions);
		else if (element == "nav_shop") { m_message.clear(); show(Screen::Shop); }
		else if (element == "nav_back" || element == "nav_back2" || element == "nav_back3" || element == "nav_back4") show(Screen::Main);
		else if (element == "nav_heroes") show(Screen::Heroes);
		// the character viewer and the zombie encyclopedia (this state is gone after it)
		else if (element == "nav_view_heroes") get_manager()->transition_to_state(new CharacterViewerState(CharacterViewerState::Mode::Heroes), true);
		else if (element == "nav_zombiepedia") get_manager()->transition_to_state(new CharacterViewerState(CharacterViewerState::Mode::Enemies), true);
		else if (element == "nav_options") show(Screen::Options);
		else if (session.handle_option(element, m_engine)) show(Screen::Options);
		else if (element.rfind("hero_", 0) == 0)
		{
			if (choose_hero(session.profile, element.substr(5))) session.save();
			show(Screen::Heroes);
		}
		else if (element.rfind("buy_", 0) == 0) bought(buy_weapon(session.campaign, session.profile, element.substr(4)));
		else if (element.rfind("up_", 0) == 0) bought(buy_upgrade(session.campaign, session.profile, element.substr(3)));
		else if (element == "health_up") bought(buy_health(session.campaign, session.profile));
		else if (element.rfind("boost_", 0) == 0) bought(buy_boost(session.campaign, session.profile, element.substr(6)));
		else if (element.rfind("equip_", 0) == 0)
		{
			if (equip(session.profile, element.substr(6))) session.save();
			m_message.clear();
			show(Screen::Shop);
		}
	}
}
