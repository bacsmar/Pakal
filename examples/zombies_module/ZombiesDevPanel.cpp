///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesDevPanel.cpp
// Purpose: The level's development panel (Dear ImGui, F1): frame rate, levels, the player, enemies,
//          graphics, and the level's editor (TopDown's LevelEditPanel) with its live reload. It is a tool
//          that looks into everything the level has, so it stays the level's own (members of
//          ZombiesLevelState) in a file of its own.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesLevelState.h"
#include "Components.h"

#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <dear-imgui/imgui.h>

#include "LogMgr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>

using namespace Pakal;

namespace Zombies
{
	// The development panel: frame rate, levels, player, enemies, graphics. Runs while the graphics system draws, so it only
	// leaves requests for the next update (changing the level destroys this state).
	void ZombiesLevelState::dev_panel()
	{
		std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
		if (!m_game) return;
		// At the right edge, under the pause button
		const float width = ImGui::GetFontSize() * 24.0f;
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - width - 12.0f, ImGui::GetFontSize() * 4.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Zombies (F1)"))
		{
			ImGui::End();
			return;
		}
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::Text("%.0f FPS  %.1f ms   %s", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f,
			BgfxGraphicsSystem::quality_name(m_graphics->get_quality()));
		ImGui::Text("zombies %d   kills %d   $%d   %.0f s", m_game->alive_enemies(), m_game->kills, m_game->money, m_game->timeMs / 1000.0);

		if (ImGui::CollapsingHeader("Level", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_devLevels.empty())
			{
				// The levels are the folders next to this one that have a level.json
				std::error_code error;
				const std::filesystem::path root = std::filesystem::path(m_levelPath).parent_path().parent_path();
				for (const auto& entry : std::filesystem::directory_iterator(root, error))
				{
					if (std::filesystem::exists(entry.path() / "level.json", error)) m_devLevels.push_back(entry.path().filename().string());
				}
				std::sort(m_devLevels.begin(), m_devLevels.end());
				const std::string current = std::filesystem::path(m_levelPath).parent_path().filename().string();
				for (size_t i = 0; i < m_devLevels.size(); ++i) if (m_devLevels[i] == current) m_devLevelIndex = static_cast<int>(i);
			}
			const char* preview = m_devLevelIndex >= 0 ? m_devLevels[m_devLevelIndex].c_str() : "";
			if (ImGui::BeginCombo("##level", preview))
			{
				for (size_t i = 0; i < m_devLevels.size(); ++i)
				{
					if (ImGui::Selectable(m_devLevels[i].c_str(), static_cast<int>(i) == m_devLevelIndex)) m_devLevelIndex = static_cast<int>(i);
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load") && m_devLevelIndex >= 0)
			{
				m_devLevelRequest = (std::filesystem::path(m_levelPath).parent_path().parent_path() / m_devLevels[m_devLevelIndex] / "level.json").string();
			}
			if (ImGui::Button("Restart")) m_devRestartRequest = true;
			ImGui::SameLine();
			ImGui::Checkbox("Paused", &m_paused);
		}

		if (m_edit && ImGui::CollapsingHeader("Edit level", m_edit->editing() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
		{
			m_edit->draw();
		}

		Actor* player = m_game->find(m_playerId);
		if (player && ImGui::CollapsingHeader("Player"))
		{
			HealthComponent* health = player->get<HealthComponent>();
			// God mode: an endless defence, so every hit takes exactly nothing (boosts come and go on top of it)
			StatsComponent* stats = player->get<StatsComponent>();
			if (ImGui::Checkbox("God mode", &m_devGodMode) && stats)
			{
				if (m_devGodMode) m_devGodModifier = stats->stats.add_modifier(Stat::defence, 0.0f, std::numeric_limits<float>::infinity());
				else stats->stats.remove_modifier(m_devGodModifier);
			}
			if (health && ImGui::Button("Heal")) health->heal(health->maximum());
			ImGui::SameLine();
			if (ImGui::Button("+$1000")) m_game->money += 1000;
			ImGui::TextUnformatted("Consumables:");
			for (const auto& [id, rules] : m_rules.consumables)
			{
				char label[96];
				std::snprintf(label, sizeof(label), "%s##c%d", rules.name.empty() ? std::to_string(id).c_str() : rules.name.c_str(), id);
				if (ImGui::SmallButton(label)) player->get<ConsumableComponent>()->apply(*m_game, rules);
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}

		if (player && ImGui::CollapsingHeader("Enemies"))
		{
			ImGui::TextUnformatted("Spawn 8 m from the player:");
			for (const auto& [type, rules] : m_rules.enemies)
			{
				char label[96];
				std::snprintf(label, sizeof(label), "%s##e%s", rules.name.empty() ? type.c_str() : rules.name.c_str(), type.c_str());
				if (ImGui::SmallButton(label))
				{
					const float angle = m_random["dev"].uniform(0.0f, 6.2831853f);
					spawn_enemy(type, player->position + 8.0f * b2Vec2(std::cos(angle), std::sin(angle)));
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}

		if (ImGui::CollapsingHeader("Graphics"))
		{
			int quality = static_cast<int>(m_graphics->get_quality());
			const char* names[] = { "Low", "Medium", "High", "Ultra" };
			if (ImGui::Combo("Quality", &quality, names, 4)) m_graphics->set_quality(static_cast<BgfxGraphicsSystem::Quality>(quality));
			// What the picture is made of, to compare looks and costs (the FPS line above)
			bool changed = ImGui::Checkbox("Post-process", &m_devGraphics.post);
			ImGui::BeginDisabled(!m_devGraphics.post);
			ImGui::SameLine();
			changed |= ImGui::Checkbox("SSAO", &m_devGraphics.ao);
			ImGui::SameLine();
			changed |= ImGui::Checkbox("Tilt-shift", &m_devGraphics.tilt);
			ImGui::EndDisabled();
			changed |= ImGui::Checkbox("Real-time shadows", &m_devGraphics.shadows);
			if (changed) apply_dev_graphics();
			ImGui::BeginDisabled(!m_graphics->has_lightmap());
			bool lightmap = m_graphics->get_lightmap_enabled();
			if (ImGui::Checkbox("Lightmap", &lightmap)) m_graphics->set_lightmap_enabled(lightmap);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!m_graphics->has_gi());
			bool gi = m_graphics->get_gi_enabled();
			if (ImGui::Checkbox("GI probes", &gi)) m_graphics->set_gi_enabled(gi);
			ImGui::EndDisabled();
			{
				// the performance governor (phones): what it has done, and a switch to hold the settings still while measuring
				bool governor = m_graphics->get_governor();
				if (ImGui::Checkbox("Governor", &governor)) m_graphics->set_governor(governor, 60);
				ImGui::TextUnformatted(m_graphics->governor_status().c_str());
			}
			if (m_graphics->can_upscale())
			{
				// What the picture is drawn at, and how it is scaled up to the screen
				static const float scales[] = { 1.0f, 0.77f, 0.67f, 0.59f, 0.5f };
				static const char* scaleNames[] = { "Native (100%)", "Ultra quality (77%)", "Quality (67%)", "Balanced (59%)", "Performance (50%)" };
				const float current = m_graphics->get_render_scale();
				int index = 0;
				for (int i = 1; i < 5; ++i) if (std::fabs(scales[i] - current) < std::fabs(scales[index] - current)) index = i;
				if (ImGui::Combo("Resolution", &index, scaleNames, 5)) m_graphics->set_render_scale(scales[index]);
				ImGui::Text("%ux%u -> %ux%u", m_graphics->get_width(), m_graphics->get_height(), m_graphics->get_output_width(),
					m_graphics->get_output_height());
				bool fsr = m_graphics->get_upscaler() == BgfxGraphicsSystem::Upscaler::Fsr;
				if (ImGui::Checkbox("FSR 1", &fsr)) m_graphics->set_upscaler(fsr ? BgfxGraphicsSystem::Upscaler::Fsr : BgfxGraphicsSystem::Upscaler::Bilinear);
				ImGui::SameLine();
				ImGui::BeginDisabled(!fsr);
				float sharpness = m_graphics->get_sharpness();
				if (ImGui::SliderFloat("Sharpening (stops less)", &sharpness, 0.0f, 2.0f, "%.2f")) m_graphics->set_sharpness(sharpness);
				ImGui::EndDisabled();
			}
			if (ImGui::SliderFloat("Plastic gloss", &m_gloss, 0.0f, 1.5f, "%.2f")) MeshComponent3D_Bgfx::set_style(m_gloss, kGlossShininess);
			ImGui::Checkbox("No fading buildings", &m_noFade);
		}
		ImGui::End();
	}

	// The level's editor each frame: the mouse on the floor picks and drags while editing (the game waits meanwhile), and
	// the level is built again when the editor asks or its file changes from outside (live reload: pakal-level, an agent).
	// A reload keeps the player where it is and the editor as it was.
	void ZombiesLevelState::update_editing()
	{
		if (!m_edit || !m_graphics) return;
		b2Vec2 mouse(0.0f, 0.0f);
		const bool aimed = m_controls->mouse_aim(*m_rig, static_cast<float>(m_graphics->get_width()), static_cast<float>(m_graphics->get_height()), mouse);
		m_edit->update(m_controls->mouse_left(), !m_graphics->get_debug_ui().wants_mouse(), aimed, mouse, *m_graphics);
		static auto lastFrame = std::chrono::steady_clock::now();
		const auto frame = std::chrono::steady_clock::now();
		const float dt = std::min(0.1f, std::chrono::duration<float>(frame - lastFrame).count());
		lastFrame = frame;
		if (m_edit->editing())
		{
			// the game waits; WASD moves the camera (as the player would walk), from where the player is
			if (!m_wasEditing)
			{
				const Actor* player = m_game->find(m_playerId);
				m_editFocus = player ? player->position : b2Vec2(0.0f, 0.0f);
			}
			m_paused = true;
			const PlayerInput walk = m_controls->read(*m_rig, static_cast<float>(m_graphics->get_width()), static_cast<float>(m_graphics->get_height()));
			m_editFocus += (12.0f * dt) * walk.move;
			update_camera();
		}
		else if (m_wasEditing)
		{
			m_paused = false;
		}
		m_wasEditing = m_edit->editing();

		bool reload = m_edit->take_reload();
		static auto lastCheck = std::chrono::steady_clock::now();
		const auto now = std::chrono::steady_clock::now();
		if (m_edit->live_reload() && now - lastCheck > std::chrono::milliseconds(500))
		{
			lastCheck = now;
			std::error_code error;
			const auto written = std::filesystem::last_write_time(m_levelPath, error);
			if (!error && written != m_levelWriteTime)
			{
				m_levelWriteTime = written;
				if (written != m_edit->saved_at())
				{
					LOG_INFO("[ZombiesLevel] '%s' changed: reloading it", m_levelPath.c_str());
					reload = true;
				}
			}
		}
		if (!reload) return;
		const Actor* player = m_game->find(m_playerId);
		m_carryEdit.keep = true;
		m_carryEdit.playerAt = player ? player->position : b2Vec2(0.0f, 0.0f);
		m_carryEdit.focus = m_editFocus;
		m_carryEdit.editing = m_edit->editing();
		m_carryEdit.liveReload = m_edit->live_reload();
		m_carryEdit.selected = m_edit->selected();
		m_transitionTo = m_levelPath;
		m_transitionMission = m_missionId;
	}

	// The level's picture settings minus what the dev panel turned off
	void ZombiesLevelState::apply_dev_graphics()
	{
		BgfxGraphicsSystem::PostSettings post = m_levelPost;
		post.enabled = post.enabled && m_devGraphics.post;
		if (!m_devGraphics.ao) post.aoStrength = 0.0f;
		if (!m_devGraphics.tilt) post.tiltBlur = 0.0f;
		m_graphics->set_post_process(post);
		BgfxGraphicsSystem::ShadowSettings shadows = m_levelShadows;
		shadows.enabled = shadows.enabled && m_devGraphics.shadows;
		m_graphics->set_shadows(shadows);
	}
}
