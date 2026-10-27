///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesHud.cpp
// Purpose: The Zombies HUD - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesHud.h"
#include "MenuState.h"

#include "IUIManager.h"
#include "bgfx/BgfxGraphicsSystem.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Zombies
{
	namespace
	{
		const unsigned kHudDocument = 1;

		// The objective line: the controls off show nothing, berserk says so, "SURVIVE" (or a script's key) is the
		// survival goal, anything else is a mission's own text as it is
		std::string objective_text(const HudView& view, const char* survive)
		{
			if (view.intro) return std::string();
			if (view.berserk) return "BERSERK: the zombies are faster and hit harder";
			if (view.objectiveKey == "SURVIVE" || view.objectiveKey.find('.') != std::string::npos) return survive;
			return view.objectiveKey;
		}
	}

	bool ZombiesHud::load(Pakal::IUIManager* ui)
	{
		m_ui = ui;
		if (!m_ui || !m_ui->load_document(kHudDocument, "Assets/ui/hud.rml"))
		{
			return false;
		}
#if defined(__ANDROID__)
		m_ui->set_class(kHudDocument, "hud", "touch", true);
#endif
		m_ui->show_document(kHudDocument);
		m_clickListener = m_ui->add_click_listener(kHudDocument, [this](const std::string& element)
		{
			std::lock_guard<std::mutex> lock(m_clickMutex);
			m_clicks.push_back(element);
		});
		m_loaded = true;
		m_ui->set_class(kHudDocument, "btn_menu", "hidden", false);
		return true;
	}

	void ZombiesHud::unload()
	{
		if (m_ui && m_loaded)
		{
			m_ui->remove_click_listener(m_clickListener);
			m_ui->unload_document(kHudDocument);
		}
		m_loaded = false;
	}

	std::vector<std::string> ZombiesHud::take_clicks()
	{
		std::vector<std::string> clicks;
		std::lock_guard<std::mutex> lock(m_clickMutex);
		clicks.swap(m_clicks);
		return clicks;
	}

	void ZombiesHud::refresh_boosts(const std::string& missionId)
	{
		if (!m_ui || !m_loaded) return;
		std::string markup;
		if (!missionId.empty())
		{
			const Session& session = Session::get();
			for (const BoostDef& boost : session.campaign.boosts)
			{
				const int count = session.profile.boost_count(boost.id);
				if (count <= 0) continue;
				markup += "<div id=\"useboost_" + boost.id + "\" class=\"boost\">" + boost.title + " <span class=\"count\">x" + std::to_string(count) + "</span></div>";
			}
		}
		m_ui->set_markup(kHudDocument, "boosts", markup);
		m_boostsShown = !markup.empty();
		m_ui->set_class(kHudDocument, "boosts", "hidden", !m_boostsShown);
	}

	void ZombiesHud::draw(Pakal::BgfxGraphicsSystem& graphics, const HudView& view)
	{
		if (m_loaded) update_document(view);
		else draw_text(graphics, view);
	}

	void ZombiesHud::update_document(const HudView& view)
	{
		const unsigned doc = kHudDocument;
		char text[160];

		const float hpMax = std::max(1.0f, view.healthMax);
		std::snprintf(text, sizeof(text), "%.0f%%", 100.0f * std::clamp(view.health / hpMax, 0.0f, 1.0f));
		m_ui->set_property(doc, "health_fill", "width", text);
		m_ui->set_class(doc, "health_fill", "low", view.health / hpMax < 0.3f);
		std::snprintf(text, sizeof(text), "%.0f", view.health);
		m_ui->set_text(doc, "health_text", text);
		m_ui->set_text(doc, "money_text", std::to_string(view.money));

		if (!view.meleeName.empty()) std::snprintf(text, sizeof(text), "%s", view.meleeName.c_str());
		else if (view.hasWeapon) std::snprintf(text, sizeof(text), "%s  %d/%d%s", view.gunName.c_str(), view.magazine, view.rounds,
			view.reloading ? "  reloading" : "");
		else text[0] = '\0';
		m_ui->set_text(doc, "weapon_text", text);

		if (view.killGoal > 0) std::snprintf(text, sizeof(text), "%d/%d", view.kills, view.killGoal);
		else std::snprintf(text, sizeof(text), "%d", view.kills);
		m_ui->set_text(doc, "kills_text", text);

		const std::string objective = objective_text(view, "Survive: kill zombies until the goal");
		m_ui->set_text(doc, "objective", objective);
		m_ui->set_class(doc, "objective_box", "hidden", objective.empty());

		m_ui->set_class(doc, "timer", "hidden", !view.timer);
		if (view.timer)
		{
			std::snprintf(text, sizeof(text), "%d:%02d", view.timerSeconds / 60, view.timerSeconds % 60);
			m_ui->set_text(doc, "timer", text);
		}
		m_ui->set_class(doc, "boost", "hidden", !view.boost);
		if (view.boost)
		{
			std::snprintf(text, sizeof(text), "%s  %.0f s", view.boostName.c_str(), std::ceil(view.boostSecondsLeft));
			m_ui->set_text(doc, "boost", text);
		}
		m_ui->set_class(doc, "dialog", "hidden", view.dialog.empty());
		if (!view.dialog.empty()) m_ui->set_text(doc, "dialog", view.dialog);

		// The panel over the game: paused, down, or the level won
		const bool panel = view.paused || view.down || view.complete || view.failed;
		m_ui->set_class(doc, "overlay", "hidden", !panel);
		m_ui->set_class(doc, "pause_button", "hidden", panel);
		m_ui->set_class(doc, "boosts", "hidden", panel || view.dead || !m_boostsShown);
		if (!panel)
		{
			return;
		}
		const bool won = view.complete && !view.paused;
		m_ui->set_text(doc, "panel_title", view.paused ? "PAUSED" : view.down ? "YOU ARE DOWN" : view.failed ? "MISSION FAILED"
			: view.mission ? "MISSION COMPLETE" : "LEVEL COMPLETE");
		std::snprintf(text, sizeof(text), "kills %d   picked up $%d", view.kills, view.money);
		if (view.down && !view.failed)
		{
			std::snprintf(text + std::strlen(text), sizeof(text) - std::strlen(text), "   you have $%d", view.moneyToSpend);
		}
		m_ui->set_text(doc, "panel_text", text);
		// down: a revive for money
		const bool revive = view.down && !view.failed && !view.paused;
		m_ui->set_class(doc, "btn_revive", "hidden", !revive);
		if (revive)
		{
			char price[48];
			std::snprintf(price, sizeof(price), "REVIVE $%d", view.revivePrice);
			m_ui->set_text(doc, "btn_revive", price);
			m_ui->set_class(doc, "btn_revive", "poor", view.moneyToSpend < view.revivePrice);
		}
		const bool results = view.mission && view.missionSettled && !view.paused;
		m_ui->set_class(doc, "panel_stars", "hidden", !(results && won));
		m_ui->set_class(doc, "panel_reward", "hidden", !results);
		if (results)
		{
			for (int i = 0; i < 3; ++i)
			{
				m_ui->set_attribute(doc, "star" + std::to_string(i + 1), "src", view.reward.stars[static_cast<size_t>(i)] ? "images/star_on.png" : "images/star_off.png");
			}
			if (!won) std::snprintf(text, sizeof(text), "You keep what you picked up &nbsp; Money $%d", view.reward.moneyAfter);
			else std::snprintf(text, sizeof(text), "Reward $%d%s &nbsp; Money $%d", view.reward.reward, view.reward.firstWin ? " (first win)" : "", view.reward.moneyAfter);
			if (won && !view.heroRescuedName.empty())
			{
				std::snprintf(text + std::strlen(text), sizeof(text) - std::strlen(text), " &nbsp; %s joins you: pick a hero in the menu", view.heroRescuedName.c_str());
			}
			m_ui->set_markup(doc, "panel_reward", text);
		}
		m_ui->set_class(doc, "btn_next", "hidden", !(won && view.mission && view.hasNext));
		m_ui->set_class(doc, "btn_resume", "hidden", !view.paused);
		// the options while paused (quality, FSR)
		m_ui->set_class(doc, "pause_options", "hidden", !view.paused);
		if (view.paused && !m_optionsShown)
		{
			m_ui->set_markup(doc, "pause_options", Session::get().options_markup());
			m_optionsShown = true;
		}
		if (!view.paused) m_optionsShown = false;
		m_ui->set_text(doc, "btn_restart", view.complete && !view.paused ? "PLAY AGAIN" : (view.down || view.failed) ? "TRY AGAIN" : "RESTART");
		if (view.failed && !view.paused) m_ui->set_text(doc, "panel_text", view.failText);
		m_ui->set_text(doc, "btn_menu", view.mission ? "MISSIONS" : "MENU");
	}

	void ZombiesHud::draw_text(Pakal::BgfxGraphicsSystem& graphics, const HudView& view)
	{
		// The text buffer keeps what was written, so each line is padded to wipe the previous one
		auto put_text = [&graphics](int column, int row, const char* text)
		{
			char padded[200];
			std::snprintf(padded, sizeof(padded), "%-150s", text);
			graphics.draw_text(column, row, padded);
		};
		char weaponText[96];
		if (!view.meleeName.empty()) std::snprintf(weaponText, sizeof(weaponText), "%s", view.meleeName.c_str());
		else if (view.hasWeapon) std::snprintf(weaponText, sizeof(weaponText), "%s %d/%d%s", view.gunName.c_str(), view.magazine, view.rounds,
			view.reloading ? " reloading" : "");
		else weaponText[0] = '\0';
		char line[256];
		char kills[32];
		if (view.killGoal > 0) std::snprintf(kills, sizeof(kills), "kills %d/%d", view.kills, view.killGoal);
		else std::snprintf(kills, sizeof(kills), "kills %d", view.kills);
		std::snprintf(line, sizeof(line), "%s   health %.0f/%.0f   $%d   %s   %s", view.levelName.c_str(),
			view.health, view.healthMax, view.money, weaponText, kills);
		put_text(1, 1, line);

		const std::string objective = objective_text(view, "SURVIVE: kill zombies until the goal");
		const std::string status = view.paused ? "PAUSED - P to resume, R to restart"
			: view.down ? "YOU ARE DOWN - R to restart"
			: view.dead ? ""
			: view.complete ? "LEVEL COMPLETE - R to play again"
			: view.intro ? "INTRO"
			: objective;
		if (view.timer && !view.paused)
		{
			// the countdown of StartTimer next to the objective
			std::snprintf(line, sizeof(line), "%s   TIME %d:%02d", status.c_str(), view.timerSeconds / 60, view.timerSeconds % 60);
			put_text(1, 2, line);
		}
		else
		{
			put_text(1, 2, status.c_str());
		}

		if (view.boost)
		{
			std::snprintf(line, sizeof(line), "%s: %.1f s", view.boostName.c_str(), view.boostSecondsLeft);
			put_text(1, 3, line);
		}
		else
		{
			put_text(1, 3, "");
		}
		put_text(1, 4, view.dialog.c_str());
		put_text(1, 6, "Move: WASD   Aim: mouse (or IJKL)   Fire: left button (or Space)   Pause: P   Restart: R   Colliders: F3");
	}
}
