///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesScripting.cpp
// Purpose: The level's side of its scripts: what each of the original game's script functions does on
//          this level (ZombieScript::Host: spawns, objectives, dialogs, fades, camera pans, markers,
//          escorts, timers...), the objectives run in the scene's order, and the game variables the
//          scripts read. They act on everything the level has, so they are the level's own (members of
//          ZombiesLevelState) in a file of their own.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesLevelState.h"
#include "Components.h"
#include "topdown/SpawnSystem.h"
#include "topdown/view/AudioCues.h"
#include "topdown/view/PropMesh.h"
#include "topdown/view/RenderSpace.h"

#include "LogMgr.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>

using namespace Pakal;

namespace Zombies
{
	namespace
	{
		const int kReelPickupId = 9001; // the film reel of ReelObject: not in the original data
		const int kTokenPickupId = 9002; // a token of a "collect" mission: it stays until taken
		const float kEscortHealth = 150.0f;      // the survivor of an escort mission (a zombie hits ~11)

		// The texts of the scripts are keys of the game's string tables, which are not part of the data: the ones the levels use
		// have their words here, any other key shows its middle part ("A.Some_Title.text" -> "Some Title")
		std::string readable_text(const std::string& key)
		{
			static const std::map<std::string, std::string> words = {
				{ "INTROTXT.S01T01_Intro.text", "HOLLYWOOD - the dead walk the boulevard" },
				{ "SCENE_START.ACT01_SCENE01_START.text", "ACT 01 - SCENE 01" },
				{ "BERSERK.Berserk_01.text", "BERSERK!" },
				{ "OBJECTIVES.SURVIVAL_01.text", "SURVIVE" },
			};
			const auto found = words.find(key);
			if (found != words.end()) return found->second;
			const size_t first = key.find('.');
			const size_t last = key.rfind('.');
			if (first == std::string::npos || last == first || key.compare(last, std::string::npos, ".text") != 0) return key;
			std::string middle = key.substr(first + 1, last - first - 1);
			std::replace(middle.begin(), middle.end(), '_', ' ');
			return middle;
		}
	}

	bool ZombiesLevelState::point_position(int id, b2Vec2& at) const
	{
		auto point = m_warpPoints.find(id);
		if (point != m_warpPoints.end()) { at = point->second; return true; }
		auto anchor = m_cameraAnchors.find(id);
		if (anchor != m_cameraAnchors.end()) { at = b2Vec2(anchor->second[0], -anchor->second[2]); return true; }
		return false;
	}

	// The level's objectives (level.json "script"), run in the scene's order. Each objective runs its
	// OnActivate once, then its Run hook every frame until Done() or DoneKillAll() ends it. This follows
	// the game's objective flow (ObjectiveComponent.cpp: a finished objective starts the next one).
	void ZombiesLevelState::start_objective_scripts(const LevelDocument& doc)
	{
		ZombieScript::Host host;
		host.setEnemySpawn = [this](int enemyType, int alive, float cooldownMs)
		{
			m_spawns->set_order(std::to_string(enemyType), alive, cooldownMs);
		};
		host.killAllZombies = [this]() { m_game->kill_all_enemies(); };
		host.doneKillAll = [this]()
		{
			m_levelComplete = true;
			m_objectiveDone = true;
			LOG_INFO("[ZombiesLevel] LEVEL COMPLETE: %d kills, $%d", m_game->kills, m_game->money);
		};
		host.done = [this]() { m_objectiveDone = true; };
		host.showDialog = [this](const std::string& text, float milliseconds)
		{
			m_dialog = readable_text(text);
			m_dialogUntilMs = m_game->timeMs + milliseconds;
		};
		host.setObjectiveText = [this](const std::string& key) { m_objectiveKey = key; };
		host.removeObjectiveText = [this]() { m_objectiveKey.clear(); };
		host.getObjectHealth = [this](int id) -> float
		{
			auto found = m_actorsByGameId.find(id);
			if (found == m_actorsByGameId.end()) return -1.0f;
			for (uintptr_t actorId : found->second)
			{
				const Actor* actor = m_game->find(actorId);
				if (const DestructibleComponent* prop = actor ? actor->get<DestructibleComponent>() : nullptr) return prop->health();
			}
			return -1.0f;
		};
		host.startTimer = [this](float ms) { m_timerEndMs = m_game->timeMs + ms; };
		host.dropPickup = [this](int pickupId, int pointId)
		{
			if (pickupId == kTokenPickupId && !m_rules.pickups.count(kTokenPickupId))
			{
				PickupRules token;
				token.type = "Token";
				token.value = 1.0f;
				token.radius = 130.0f;
				token.lifeTimeMs = -1.0f;
				token.sound = m_rules.sound("pickup");
				m_rules.pickups[kTokenPickupId] = token;
			}
			b2Vec2 at;
			if (!point_position(pointId, at)) { LOG_WARNING("[ZombiesLevel] DropPickup: no point %d", pointId); return; }
			m_game->drop_pickup(pickupId, at);
		};
		host.isPlayerNear = [this](int pointId, float radiusCm)
		{
			b2Vec2 at;
			const Actor* player = m_game->find(m_playerId);
			return player && point_position(pointId, at) && (player->position - at).Length() <= meters(radiusCm);
		};
		host.showMarker = [this](int pointId, bool on)
		{
			m_markers.erase(std::remove(m_markers.begin(), m_markers.end(), pointId), m_markers.end());
			if (on) m_markers.push_back(pointId);
		};
		host.startEscort = [this](const std::string& hero, const std::vector<int>& points)
		{
			b2Vec2 start;
			if (points.empty() || !point_position(points[0], start)) { LOG_WARNING("[ZombiesLevel] StartEscort: no start point"); return; }
			std::vector<b2Vec2> route;
			for (size_t i = 1; i < points.size(); ++i)
			{
				b2Vec2 at;
				if (point_position(points[i], at)) route.push_back(at);
			}
			Actor& ally = m_game->add_actor(ActorKind::Character, "Survivor", m_game->survivors);
			ally.position = start;
			ally.add<HealthComponent>(kEscortHealth, 0.3f);
			ally.add<EscortComponent>(route);
			ally.add<CharacterBodyComponent>(*m_game, start, kPlayerRadius);
			auto model = m_heroModels.find(hero);
			const std::string file = model != m_heroModels.end() ? model->second : m_playerModel;
			if (!file.empty()) ally.add<CharacterViewComponent>(create_instance(file), m_playerLook);
			m_game->escorted = ally.id;
			LOG_INFO("[ZombiesLevel] escorting %s along %zu points", hero.c_str(), route.size());
		};
		host.fail = [this](const std::string& text)
		{
			m_levelFailed = true;
			m_failText = text;
			LOG_INFO("[ZombiesLevel] mission failed: %s", text.c_str());
		};
		host.stopTimer = [this]() { m_timerEndMs = -1.0; };
		host.enableControls = [this](bool enabled)
		{
			if (Actor* player = m_game->find(m_playerId))
			{
				if (auto* control = player->get<PlayerControlComponent>()) control->enabled = enabled;
			}
		};
		host.warpPlayer = [this](int id) { warp_player(id); };
		host.showObject = [this](int id, bool shown) { show_object(id, shown); };
		host.enableObject = [this](int id, bool enabled) { enable_object(id, enabled); };
		// EnableRandomObject(ids..., enable, count): one of the listed objects, picked at random
		host.enableRandomObject = [this](const std::vector<int>& ids, bool enabled)
		{
			if (ids.empty()) return;
			const size_t pick = std::min(ids.size() - 1, static_cast<size_t>(m_game->random01() * static_cast<float>(ids.size())));
			show_object(ids[pick], enabled);
			enable_object(ids[pick], enabled);
		};
		// ReelObject: the film reel is inside one of the listed props (a breakable one), and gives money when it breaks
		host.reelObject = [this](const std::vector<int>& ids)
		{
			std::vector<uintptr_t> candidates;
			for (int id : ids)
			{
				const auto found = m_actorsByGameId.find(id);
				if (found == m_actorsByGameId.end()) continue;
				for (uintptr_t actorId : found->second)
				{
					Actor* prop = m_game->find(actorId);
					if (prop && prop->get<DestructibleComponent>()) candidates.push_back(actorId);
				}
			}
			if (candidates.empty()) return;
			const size_t pick = std::min(candidates.size() - 1, static_cast<size_t>(m_game->random01() * static_cast<float>(candidates.size())));
			if (Actor* prop = m_game->find(candidates[pick]))
			{
				// the reel is worth more than a coin and stays until it is picked up
				PickupRules reel;
				reel.type = "Money";
				reel.value = 50.0f;
				reel.radius = 200.0f;
				reel.lifeTimeMs = -1.0f;
				reel.sound = m_rules.sound("pickup");
				m_rules.pickups[kReelPickupId] = reel;
				prop->get<DestructibleComponent>()->set_hidden_pickup(kReelPickupId);
				LOG_INFO("[ZombiesLevel] a film reel hides in %s", prop->name.c_str());
			}
		};
		host.enableBerserk = [this](bool on)
		{
			m_game->set_berserk(on);
			LOG_INFO("[ZombiesLevel] berserk %s", on ? "on" : "off");
		};
		host.cameraPan = [this](int fromId, const std::string& fromOffset, int toId, const std::string& toOffset, float seconds)
		{
			start_camera_pan(fromId, fromOffset, toId, toOffset, seconds);
		};
		host.cameraReturnToPlayer = [this]() { m_rig->pan().active = false; };
		host.fade = [this](bool toColor, float milliseconds, const std::string& argb)
		{
			// "AARRGGBB": FadeOut covers the screen with the color; FadeIn uncovers it (from the color of the last FadeOut, so
			// a cutscene does not flash another color)
			if (toColor)
			{
				const unsigned long value = std::strtoul(argb.c_str(), nullptr, 16);
				m_fade.color[0] = static_cast<float>((value >> 16) & 0xff) / 255.0f;
				m_fade.color[1] = static_cast<float>((value >> 8) & 0xff) / 255.0f;
				m_fade.color[2] = static_cast<float>(value & 0xff) / 255.0f;
			}
			m_fade.from = m_graphics ? m_graphics->get_screen_fade() : 0.0f;
			m_fade.to = toColor ? 1.0f : 0.0f;
			m_fade.startMs = m_game->timeMs;
			m_fade.durationMs = milliseconds;
		};
		host.showClapper = [this](const std::string& text, float milliseconds)
		{
			m_dialog = readable_text(text);
			m_dialogUntilMs = m_game->timeMs + milliseconds;
		};
		host.playMusic = [this](const std::string& name)
		{
			if (m_audio && !name.empty()) m_audio->play(name, 0.6f);
		};
		host.shakeCamera = [this](float milliseconds, float power)
		{
			// ShakeCamera(ms, power) of the scripts: power is the jitter in centimeters
			m_rig->shake(power, milliseconds);
		};
		host.log = [](const std::string& message)
		{
			// Each missing function is reported once, not on every pass of Run
			static std::set<std::string> reported;
			if (reported.insert(message).second)
			{
				LOG_INFO("[ZombieScript] %s", message.c_str());
			}
		};
		m_script = std::make_unique<ZombieScript>(std::move(host));

		// Named points (WarpPlayer targets): legacy id -> position in meters
		const Json points = doc.script.value("points", Json::object());
		for (auto it = points.begin(); it != points.end(); ++it)
		{
			m_warpPoints[std::atoi(it.key().c_str())] = b2Vec2(doc.meters(it.value()[0].get<double>()), doc.meters(it.value()[1].get<double>()));
		}
		const Json anchors = doc.script.value("anchors", Json::object());
		for (auto it = anchors.begin(); it != anchors.end(); ++it)
		{
			const auto& p = it.value();
			m_cameraAnchors[std::atoi(it.key().c_str())] = { doc.meters(p[0].get<double>()), doc.meters(p[2].get<double>()), -doc.meters(p[1].get<double>()) };
		}
		m_objectiveHooks = doc.script.value("objectives", Json::object());
		for (const auto& id : doc.script.value("order", Json::array()))
		{
			if (id.is_string()) m_objectiveOrder.push_back(id.get<std::string>());
		}
		if (!m_objectiveOrder.empty())
		{
			begin_objective(0);
		}
	}

	void ZombiesLevelState::start_camera_pan(int fromId, const std::string& fromOffset, int toId, const std::string& toOffset, float seconds)
	{
		if (!m_cameraAnchors.count(fromId) || !m_cameraAnchors.count(toId))
		{
			LOG_ERROR("[ZombiesLevel] camera pan missing anchors %d/%d; regenerate the level", fromId, toId);
			return;
		}
		auto eyeAt = [](const CameraPan::Point& target, const std::string& offset, CameraPan::Point& eye)
		{
			std::istringstream input(offset);
			float x, y, z;
			if (!(input >> x >> y >> z)) return false;
			eye = { target[0] + x * 0.01f, target[1] + z * 0.01f, target[2] - y * 0.01f };
			return true;
		};
		CameraPan pan;
		pan.fromTarget = m_cameraAnchors.at(fromId);
		pan.toTarget = m_cameraAnchors.at(toId);
		if (!eyeAt(pan.fromTarget, fromOffset, pan.fromEye) || !eyeAt(pan.toTarget, toOffset, pan.toEye))
		{
			LOG_ERROR("[ZombiesLevel] invalid camera offset");
			return;
		}
		pan.startedMs = m_game->timeMs;
		pan.durationMs = std::max(0.0f, seconds) * 1000.0;
		pan.active = true;
		m_rig->pan() = pan;
	}

	// ShowObject / HideObject and EnableObject / DisableObject: every prop with that game id. Enabling
	// the gift box also arms it.
	void ZombiesLevelState::show_object(int id, bool shown)
	{
		auto found = m_actorsByGameId.find(id);
		if (found == m_actorsByGameId.end()) return;
		for (uintptr_t actorId : found->second)
		{
			if (Actor* actor = m_game->find(actorId))
			{
				if (auto* prop = actor->get<PropComponent>()) prop->show(shown);
			}
		}
	}

	void ZombiesLevelState::enable_object(int id, bool enabled)
	{
		auto found = m_actorsByGameId.find(id);
		if (found == m_actorsByGameId.end()) return;
		for (uintptr_t actorId : found->second)
		{
			Actor* actor = m_game->find(actorId);
			if (!actor) continue;
			if (auto* prop = actor->get<PropComponent>()) prop->enable(enabled);
			if (auto* gift = actor->get<GiftBoxComponent>())
			{
				gift->arm(enabled);
				LOG_INFO("[ZombiesLevel] gift box %d %s", id, gift->armed() ? "ready" : "off");
			}
		}
	}

	// WarpPlayer(id): to the named point of the level (level.json script.points, from the game's Spawnpoint)
	void ZombiesLevelState::warp_player(int id)
	{
		Actor* player = m_game->find(m_playerId);
		auto point = m_warpPoints.find(id);
		if (point == m_warpPoints.end() || !player)
		{
			LOG_INFO("[ZombiesLevel] WarpPlayer(%d): no such point", id);
			return;
		}
		b2Vec2 at = point->second;
		static const char* start = std::getenv("ZOMBIES_START"); // debug: warps go to this point "x,y" (meters) instead
		if (start)
		{
			std::sscanf(start, "%f,%f", &at.x, &at.y);
		}
		if (auto* body = player->get<CharacterBodyComponent>()) body->teleport(at);
	}

	void ZombiesLevelState::begin_objective(size_t index)
	{
		m_objectiveIndex = index;
		m_objectiveDone = false;
		m_runStarted = false;
		m_objectiveRunCode.clear();
		const std::string id = m_objectiveOrder[index];
		LOG_INFO("[ZombiesLevel] objective %s (%zu of %zu)", id.c_str(), index + 1, m_objectiveOrder.size());
		if (!m_objectiveHooks.contains(id))
		{
			m_objectiveDone = true; // nothing to run for it
			return;
		}
		const Json& hooks = m_objectiveHooks[id];
		const std::string onActivate = hooks.value("onActivate", std::string());
		if (!onActivate.empty())
		{
			m_script->start("OnActivate", onActivate, false);
		}
		m_objectiveRunCode = hooks.value("run", std::string());
	}

	// Runs the ending objective's onDeactivate hook once (a Wait in it is not relaunched)
	void ZombiesLevelState::run_deactivate_hook()
	{
		if (!m_script || m_objectiveIndex >= m_objectiveOrder.size())
		{
			return;
		}
		const std::string id = m_objectiveOrder[m_objectiveIndex];
		if (!m_objectiveHooks.contains(id))
		{
			return;
		}
		const std::string code = m_objectiveHooks[id].value("onDeactivate", std::string());
		if (!code.empty() && !m_script->is_running("OnDeactivate"))
		{
			m_script->start("OnDeactivate", code, false);
		}
	}

	void ZombiesLevelState::update_script(float dt)
	{
		(void)dt;
		if (!m_script)
		{
			return;
		}
		// The game variables the scripts read: alive and killed zombies
		m_script->set_variable("enemyCount", m_game->alive_enemies());
		m_script->set_variable("killCount", m_game->kills);
		m_script->set_variable("pickupCount", m_game->tokens);
		if (const Actor* ally = m_game->escorted ? m_game->find(m_game->escorted) : nullptr)
		{
			const EscortComponent* escort = ally->get<EscortComponent>();
			const HealthComponent* health = ally->get<HealthComponent>();
			m_script->set_variable("escortState", escort ? static_cast<double>(escort->state()) : 0.0);
			m_script->set_variable("escortHealth", health ? std::floor(100.0 * health->current() / kEscortHealth + 0.5) : 0.0);
		}
		m_script->set_variable("timerMs", m_timerEndMs < 0.0 ? 0.0 : std::max(0.0, m_timerEndMs - m_game->timeMs));
		m_game->difficulty = static_cast<int>(m_script->get_variable("difficultyLevel"));
		// A cutscene the script allows to skip (EnableSkip): any press of the fire button or space cuts it short
		if (m_script->skippable() && (m_controls->mouse_left() || m_controls->key_down(Key::Space) || m_controls->key_down(Key::Return)))
		{
			m_script->skip();
			m_dialogUntilMs = 0.0;
		}
		m_script->update(m_game->timeMs);

		// The objective's Run hook starts once its OnActivate has finished
		if (!m_objectiveDone && !m_runStarted && !m_objectiveRunCode.empty() && !m_script->is_running("OnActivate"))
		{
			m_runStarted = true;
			m_script->start("Run", m_objectiveRunCode, true);
		}

		// Done: this objective ends (its onDeactivate hook runs) and the next one of the scene starts
		if (m_objectiveDone)
		{
			m_script->stop("Run");
			run_deactivate_hook();
			if (m_objectiveIndex + 1 < m_objectiveOrder.size())
			{
				begin_objective(m_objectiveIndex + 1);
			}
		}
	}
}
