///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditorState.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/editor/LevelEditorState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "LogMgr.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "topdown/LevelCameraPath.h"
#include "topdown/editor/EditorMath.h"
#include "topdown/view/CameraRig.h"
#include "topdown/view/SceneLook.h"

#include <dear-imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace Pakal::TopDown
{
	using Pakal::Json;
	using Pakal::LevelObject;

	namespace
	{
		const ImVec4 kOverridden(0.45f, 0.75f, 1.0f, 1.0f);
		const float kNear = 0.3f, kFar = 3000.0f;
		const float kIntroSeconds = 8.0f;   // a path without "seconds" is previewed this long (the game gives it the script's)

		double tidy(double value)
		{
			const double rounded = std::round(value * 10000.0) / 10000.0;
			return std::fabs(rounded) < 1e-9 ? 0.0 : rounded;
		}

		std::string text_of(const Json& value)
		{
			return value.is_string() ? value.get<std::string>() : (value.is_null() ? std::string() : value.dump());
		}
	}

	LevelEditorState::LevelEditorState(std::string levelPath, PlayFactory play, SchemaKinds kinds)
		: BaseGameState("LevelEditor"), m_path(std::move(levelPath)), m_play(std::move(play)), m_kinds(std::move(kinds))
	{
	}

	LevelEditorState::~LevelEditorState() = default;

	void LevelEditorState::on_initialize(Pakal::Engine* engine)
	{
		m_engine = engine;
		m_selfTest = std::getenv("PAKAL_EDITOR_SELFTEST") != nullptr;
		ComponentSchema::add_topdown_kinds(m_schema);
		if (m_kinds) m_kinds(m_schema);
		read_recent();
		if (m_path.empty())
		{
			show_opener();   // no level given: the editor starts choosing one
		}
		else if (!reload_document())
		{
			LOG_ERROR("[LevelEditor] %s", m_status.c_str());
			show_opener();
		}
		else
		{
			remember_recent(m_path);
		}
		LevelRuntime::Settings settings;
		settings.nearPlane = kNear;
		settings.farPlane = kFar;
		settings.input.ignoreKeys = true;
		m_fovY = settings.camera.fovY;
		m_runtime = std::make_unique<LevelRuntime>(*engine);
		if (!m_runtime->begin(settings))
		{
			LOG_ERROR("[LevelEditor] no graphics system");
			m_runtime.reset();
			return;
		}
		m_runtime->on_graphics([this]() { open_view(); });
		auto& ui = m_runtime->graphics().get_debug_ui();
		ui.set_visible(true);
		m_panelId = ui.add_panel([this]() { panel(); });
		LOG_INFO("[LevelEditor] editing %s: %zu objects, %zu prefabs", m_path.c_str(), m_document ? m_document->objects.size() : size_t(0),
			m_prefabs.names().size());
	}

	void LevelEditorState::on_terminate(Pakal::Engine* engine)
	{
		(void)engine;
		if (!m_runtime) return;
		m_runtime->graphics().get_debug_ui().remove_panel(m_panelId);
		m_live.close();
		if (m_testAsker.joinable()) m_testAsker.join();
		m_runtime->on_graphics([this]() { close_view(); });
		m_runtime->end();
		m_runtime.reset();
	}

	void LevelEditorState::on_update(unsigned long dtMilliseconds)
	{
		(void)dtMilliseconds;
		if (!m_runtime || !m_playAsked.exchange(false) || !m_play) return;
		{
			std::lock_guard<std::recursive_mutex> lock(m_mutex);
			if (!save()) return;
		}
		// not under the lock: the panel takes it on the graphics thread, which this waits for
		m_runtime->on_graphics([this]() { close_view(); });
		Pakal::BaseGameState* game = m_play(m_path);
		if (!game)
		{
			m_runtime->on_graphics([this]() { open_view(); });
			return;
		}
		m_playing = true;
		get_manager()->push_state(game, true);
	}

	void LevelEditorState::on_activate(Pakal::Engine* engine)
	{
		(void)engine;
		if (!m_playing.exchange(false) || !m_runtime) return;
		// back from Play: the game let its picture go; the editor's comes back as it was
		m_runtime->on_graphics([this]() { open_view(); });
		m_runtime->graphics().get_debug_ui().set_visible(true);
		m_status = "stopped";
		m_testBackFrame = m_testFrame;
	}

	void LevelEditorState::on_deactivate(Pakal::Engine* engine)
	{
		(void)engine;
	}

	// --- the document ---

	bool LevelEditorState::reload_document()
	{
		auto document = std::make_unique<Pakal::LevelDocument>();
		std::string error;
		Pakal::LevelLayerEditing layer;
		if (!layer.open(m_path, *document, error))
		{
			m_status = error;
			return false;
		}
		m_layer = std::move(layer);
		const int given = document->assign_keys();
		m_document = std::move(document);
		m_editor = std::make_unique<Pakal::LevelEditor>(*m_document);
		m_prefabs.open(m_path, *m_document);
		m_editor->set_prefabs(&m_prefabs);
		const std::filesystem::path file(m_path);
		m_historyFolder = (file.parent_path() / (file.stem().string() + ".history")).string();
		if (!m_editor->load_history(m_historyFolder, error)) m_status = error;
		std::error_code code;
		m_savedAt = std::filesystem::last_write_time(m_path, code);
		if (m_layer.layered()) m_baseSavedAt = std::filesystem::last_write_time(m_layer.base_path(), code);
		m_dirty = given > 0;
		if (given > 0) m_status = std::to_string(given) + " objects got a key (saved with the level)";
		if (!m_layer.layering().missing.empty())
		{
			m_status = std::to_string(m_layer.layering().missing.size()) + " keys this layer hides or adjusts are not in its base (see Level > Layer)";
		}
		// what is selected and no longer exists goes
		m_selection.erase(std::remove_if(m_selection.begin(), m_selection.end(), [&](const std::string& key) { return m_document->find(key) < 0; }),
			m_selection.end());
		return true;
	}

	bool LevelEditorState::apply(const Json& operation)
	{
		if (!m_editor) return false;
		const std::string error = m_editor->apply(operation);
		if (!error.empty())
		{
			m_status = error;
			return false;
		}
		m_dirty = true;
		m_status = operation.value("op", std::string());
		if (m_viewOpen)
		{
			m_objects.update(*m_document, nullptr);
			apply_dimming();
		}
		return true;
	}

	void LevelEditorState::undo(bool redo)
	{
		if (!m_editor) return;
		const std::string what = redo ? m_editor->redo() : m_editor->undo();
		if (what.empty()) return;
		m_dirty = true;
		m_status = std::string(redo ? "redone: " : "undone: ") + what;
		if (m_viewOpen)
		{
			m_objects.update(*m_document, nullptr);
			apply_dimming();
		}
	}

	bool LevelEditorState::save()
	{
		if (!m_document) return false;
		std::string error;
		const bool written = m_layer.layered() ? m_layer.save(m_path, *m_document, error) : m_document->save(m_path, error);
		if (!written || !m_editor->save_history(m_historyFolder, error))
		{
			m_status = "not saved: " + error;
			return false;
		}
		std::error_code code;
		m_savedAt = std::filesystem::last_write_time(m_path, code);
		m_dirty = false;
		m_status = m_layer.layered() ? "saved (the layer)" : "saved";
		return true;
	}

	int LevelEditorState::selected_index() const
	{
		return m_document && !m_selection.empty() ? m_document->find(m_selection.front()) : -1;
	}

	LevelObject* LevelEditorState::selected_object()
	{
		const int index = selected_index();
		return index >= 0 ? &m_document->objects[static_cast<size_t>(index)] : nullptr;
	}

	// --- the picture ---

	void LevelEditorState::open_view()
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (!m_runtime || !m_document) return;
		m_runtime->graphics().set_active_camera(&m_runtime->camera());
		m_runtime->apply_look(SceneLook::from_json(m_document->lighting, m_document->extra.value("post", Json::object())));
		m_objects.setup(Pakal::LevelDocument::directory_of(m_path), m_document->unitsPerMeter, &m_prefabs, &m_schema,
			[this](const std::string& file) { return m_runtime->model(file); });
		m_objects.build(*m_document, nullptr);
		apply_dimming();
		if (!m_viewOpen && m_distance == 30.0f && !m_document->objects.empty())
		{
			// the first time: the whole level in sight
			double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
			for (const auto& o : m_document->objects)
			{
				x0 = std::min(x0, o.x); y0 = std::min(y0, o.y); x1 = std::max(x1, o.x); y1 = std::max(y1, o.y);
			}
			const double u = m_document->unitsPerMeter;
			m_target[0] = static_cast<float>((x0 + x1) * 0.5 / u);
			m_target[2] = static_cast<float>(-(y0 + y1) * 0.5 / u);
			m_distance = std::clamp(static_cast<float>(std::max(x1 - x0, y1 - y0) / u) * 0.9f, 10.0f, 400.0f);
		}
		m_viewOpen = true;
		place_camera();
	}

	void LevelEditorState::close_view()
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		m_objects.clear(nullptr);
		if (m_runtime) m_runtime->graphics().clear_active_camera(&m_runtime->camera());
		m_viewOpen = false;
	}

	void LevelEditorState::place_camera()
	{
		if (!m_runtime) return;
		auto& camera = m_runtime->camera();
		auto& graphics = m_runtime->graphics();
		camera.set_viewport(0, 0, static_cast<int>(graphics.get_width()), static_cast<int>(graphics.get_height()));
		CameraPan::Point eye = free_eye(), look{ m_target[0], m_target[1], m_target[2] };
		float fovY = m_fovY;
		camera_view(eye, look, fovY);
		camera.set_perspective(fovY, kNear, kFar);
		camera.set_position(eye[0], eye[1], eye[2]);
		camera.look_at(look[0], look[1], look[2]);
		camera.update(0.0f);
	}

	CameraPan::Point LevelEditorState::free_eye() const
	{
		const float c = std::cos(m_pitch);
		return { m_target[0] + m_distance * c * std::sin(m_yaw), m_target[1] + m_distance * std::sin(m_pitch), m_target[2] + m_distance * c * std::cos(m_yaw) };
	}

	Json LevelEditorState::level_point(const CameraPan::Point& point) const
	{
		const double u = m_document->unitsPerMeter;
		auto round = [](double v) { return tidy(std::round(v * 10.0) / 10.0); };
		return Json::array({ round(point[0] * u), round(-point[2] * u), round(point[1] * u) });
	}

	void LevelEditorState::focus_selection()
	{
		const LevelObject* object = selected_object();
		if (!object) return;
		const double u = m_document->unitsPerMeter;
		m_target[0] = static_cast<float>(object->x / u);
		m_target[1] = static_cast<float>(object->z / u);
		m_target[2] = static_cast<float>(-object->y / u);
		m_distance = std::min(m_distance, 15.0f);
	}

	void LevelEditorState::check_files()
	{
		m_checkTimer -= ImGui::GetIO().DeltaTime;
		if (m_checkTimer > 0.0f) return;
		m_checkTimer = 0.5f;
		std::error_code code;
		auto written = std::filesystem::last_write_time(m_path, code);
		if (m_layer.layered())
		{
			std::error_code baseCode;
			const auto base = std::filesystem::last_write_time(m_layer.base_path(), baseCode);
			if (!baseCode && base != m_baseSavedAt) written = std::filesystem::file_time_type::max();   // read again as if the level changed
		}
		if (!code && written != m_savedAt)
		{
			if (m_dirty)
			{
				m_status = "the level changed outside: saving here overwrites it";
				m_savedAt = written;
			}
			else if (reload_document())
			{
				m_objects.update(*m_document, nullptr);
				apply_dimming();
				m_status = "reloaded: the level changed outside";
			}
		}
		if (!m_prefabs.refresh().empty())
		{
			m_objects.update(*m_document, nullptr);
			apply_dimming();
			m_status = "prefabs reloaded";
		}
	}

	// --- the panel ---

	void LevelEditorState::panel()
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		if (m_selfTest) self_test();
		serve_live();
		if (m_playing)
		{
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 60.0f, 4.0f), ImGuiCond_Always);
			ImGui::Begin("##play", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
			if (ImGui::Button("Stop (F5)") || ImGui::IsKeyPressed(ImGuiKey_F5, false)) m_engine->game_state_manager()->request_pop();
			ImGui::End();
			return;
		}
		if (!m_document || !m_viewOpen || m_opener)
		{
			draw_opener();
			if (!m_document || !m_viewOpen) return;
		}
		check_files();
		if (m_newLayer) draw_new_layer();
		draw_menu();
		draw_hierarchy();
		draw_inspector();
		draw_gizmo();
		viewport_input();
		draw_selection();
		draw_camera_path();
		if (m_previewing) m_previewSeconds += ImGui::GetIO().DeltaTime;
		place_camera();
	}

	void LevelEditorState::draw_menu()
	{
		if (!ImGui::BeginMainMenuBar()) return;
		if (ImGui::BeginMenu("Level"))
		{
			if (ImGui::MenuItem("Open...", "Ctrl+O")) show_opener();
			if (ImGui::MenuItem("Save", "Ctrl+S")) save();
			if (ImGui::MenuItem("Reload from disk", nullptr, false, !m_dirty) && reload_document())
			{
				m_objects.update(*m_document, nullptr);
				apply_dimming();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("New layer over this level..."))
			{
				// next to the level's folder: <folder>_layer/level.json
				const std::filesystem::path folder = std::filesystem::path(m_path).parent_path();
				const std::string suggested = ((folder.parent_path() / (folder.filename().string() + "_layer")) / "level.json").generic_string();
				std::snprintf(m_newLayerPath, sizeof(m_newLayerPath), "%s", suggested.c_str());
				m_newLayerError.clear();
				m_newLayer = true;
			}
			if (m_layer.layered())
			{
				ImGui::Separator();
				ImGui::TextDisabled("A layer over %s", m_layer.base_path().c_str());
				ImGui::Checkbox("Lock the base (a click does not pick it)", &m_lockBase);
				if (ImGui::Checkbox("Dim the base", &m_dimBase)) apply_dimming();
				for (const auto& key : m_layer.layering().missing) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "not in the base: %s", key.c_str());
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			// the agent's operations (live editing) say so
			const bool agents = !m_editor->done().empty() && m_editor->done().back().value("by", std::string()) == "agent";
			if (ImGui::MenuItem(agents ? "Undo (the agent's)" : "Undo", "Ctrl+Z", false, m_editor->can_undo())) undo(false);
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_editor->can_redo())) undo(true);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::RadioButton("Move (W)", m_gizmo == Gizmo::Move)) m_gizmo = Gizmo::Move;
		if (ImGui::RadioButton("Turn (E)", m_gizmo == Gizmo::Turn)) m_gizmo = Gizmo::Turn;
		if (ImGui::RadioButton("Scale (R)", m_gizmo == Gizmo::Scale)) m_gizmo = Gizmo::Scale;
		ImGui::Checkbox("Local", &m_local);
		ImGui::Checkbox("Snap", &m_snap);
		ImGui::Separator();
		if (m_play)
		{
			if (ImGui::Button("Play (F5)")) m_playAsked = true;
		}
		ImGui::Separator();
		ImGui::Text("%s%s%s", std::filesystem::path(m_path).parent_path().filename().string().c_str(), m_layer.layered() ? " (layer)" : "", m_dirty ? " *" : "");
		if (m_liveNoteSeconds > 0.0f) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", m_liveNote.c_str());
		else ImGui::TextDisabled("%s", m_status.c_str());
		ImGui::EndMainMenuBar();
	}

	void LevelEditorState::draw_hierarchy()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float top = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(ImVec2(0.0f, top), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300.0f, io.DisplaySize.y - top), ImGuiCond_FirstUseEver);
		ImGui::Begin("Level");
		if (ImGui::BeginTabBar("##tabs"))
		{
			if (ImGui::BeginTabItem("Objects"))
			{
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##filter", "filter (key, name, prefab, zone)", m_filter, sizeof(m_filter));
				std::vector<int> shown;
				const std::string filter = m_filter;
				for (size_t i = 0; i < m_document->objects.size(); ++i)
				{
					const LevelObject& o = m_document->objects[i];
					if (!filter.empty() && o.key.find(filter) == std::string::npos && o.name.find(filter) == std::string::npos
						&& o.prefab.find(filter) == std::string::npos && o.zone.find(filter) == std::string::npos) continue;
					shown.push_back(static_cast<int>(i));
				}
				ImGui::TextDisabled("%zu of %zu objects", shown.size(), m_document->objects.size());
				if (m_layer.layered()) draw_hidden();
				ImGui::BeginChild("##objects");
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(shown.size()));
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						const LevelObject& o = m_document->objects[static_cast<size_t>(shown[static_cast<size_t>(row)])];
						const bool selected = std::find(m_selection.begin(), m_selection.end(), o.key) != m_selection.end();
						const bool base = m_layer.is_base(o.key);
						if (base) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
						char label[256];
						std::snprintf(label, sizeof(label), "%s%s%s##%d", o.key.c_str(), o.prefab.empty() ? "" : "  <", o.prefab.empty() ? "" : (o.prefab + ">").c_str(), shown[static_cast<size_t>(row)]);
						if (ImGui::Selectable(label, selected))
						{
							if (io.KeyCtrl || io.KeyShift)
							{
								if (selected) m_selection.erase(std::find(m_selection.begin(), m_selection.end(), o.key));
								else m_selection.push_back(o.key);
							}
							else
							{
								m_selection = { o.key };
							}
						}
						if (base) ImGui::PopStyleColor();
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) focus_selection();
					}
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Prefabs"))
			{
				draw_prefabs();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Camera"))
			{
				draw_camera_tab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void LevelEditorState::draw_prefabs()
	{
		ImGui::TextDisabled("%s", m_prefabs.folder().c_str());
		if (!m_placing.empty())
		{
			ImGui::TextColored(kOverridden, "Click the floor to place %s (Esc: stop)", m_placing.c_str());
			if (ImGui::Button("Stop placing")) m_placing.clear();
		}
		for (const auto& name : m_prefabs.names())
		{
			int uses = 0;
			for (const auto& o : m_document->objects) uses += o.prefab == name ? 1 : 0;
			char label[160];
			std::snprintf(label, sizeof(label), "%s  (%d)", name.c_str(), uses);
			if (ImGui::Selectable(label, m_placing == name)) m_placing = m_placing == name ? std::string() : name;
			if (ImGui::IsItemHovered())
			{
				std::string error;
				const Json prefab = m_prefabs.prefab(name, error);
				ImGui::SetTooltip("%s", error.empty() ? prefab.dump(1).c_str() : error.c_str());
			}
		}
		if (m_prefabs.names().empty()) ImGui::TextWrapped("No prefabs yet: select an object and make one in the inspector.");
	}

	void LevelEditorState::draw_inspector()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float top = ImGui::GetFrameHeight();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 360.0f, top), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(360.0f, io.DisplaySize.y - top), ImGuiCond_FirstUseEver);
		ImGui::Begin("Inspector");
		LevelObject* selected = selected_object();
		if (!selected)
		{
			ImGui::TextWrapped("Click an object (or pick one in the list). Shift adds to the selection.");
			ImGui::End();
			return;
		}
		// the operations below may change the document: work on a copy of what is shown
		const LevelObject object = *selected;
		const std::string key = object.key;
		if (m_selection.size() > 1) ImGui::TextDisabled("%zu selected (the gizmo moves the first)", m_selection.size());
		auto text_field = [&](const char* label, const std::string& value, const char* field)
		{
			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
			if (ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (apply({ { "op", "set" }, { "object", key }, { "field", field }, { "value", std::string(buffer) } }) && std::string(field) == "key") m_selection.front() = buffer;
			}
		};
		text_field("Key", object.key, "key");
		text_field("Name", object.name, "name");
		text_field("Role", object.role, "role");
		if (ImGui::BeginCombo("Zone", object.zone.empty() ? "(none)" : object.zone.c_str()))
		{
			if (ImGui::Selectable("(none)", object.zone.empty())) apply({ { "op", "set" }, { "object", key }, { "field", "zone" }, { "value", "" } });
			for (const auto& zone : m_document->zones)
			{
				if (ImGui::Selectable(zone.name.c_str(), zone.name == object.zone)) apply({ { "op", "set" }, { "object", key }, { "field", "zone" }, { "value", zone.name } });
			}
			ImGui::EndCombo();
		}
		if (object.id >= 0) ImGui::TextDisabled("game id %d", object.id);
		if (m_layer.is_base(key))
		{
			ImGui::TextColored(kOverridden, "From the base: changes are saved in this layer (adjust)");
			if (ImGui::Button("Hide it in this layer")) apply({ { "op", "delete" }, { "object", key } });
		}

		ImGui::SeparatorText("Transform");
		const double u = m_document->unitsPerMeter;
		float position[3] = { static_cast<float>(object.x / u), static_cast<float>(object.y / u), static_cast<float>(object.z / u) };
		if (ImGui::InputFloat3("Position (m)", position, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			apply({ { "op", "place" }, { "object", key }, { "x", position[0] }, { "y", position[1] }, { "z", position[2] } });
		}
		if (object.has_basis)
		{
			ImGui::TextColored(kOverridden, "Its own matrix (basis): a tilt or a scale per axis");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("The gizmo keeps it. Rotation or Scale below replace it with angles and one scale.");
		}
		float rotation[3] = { static_cast<float>(object.rot[0]), static_cast<float>(object.rot[1]), static_cast<float>(object.rot[2]) };
		if (ImGui::InputFloat3("Rotation (deg)", rotation, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			apply({ { "op", "place" }, { "object", key }, { "rot", Json::array({ rotation[0], rotation[1], rotation[2] }) } });
		}
		float scale = static_cast<float>(object.scale);
		if (ImGui::InputFloat("Scale", &scale, 0.1f, 0.5f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue) && scale > 0.0f)
		{
			apply({ { "op", "place" }, { "object", key }, { "scale", scale } });
		}

		ImGui::SeparatorText("Prefab");
		if (ImGui::BeginCombo("Prefab", object.prefab.empty() ? "(none)" : object.prefab.c_str()))
		{
			if (ImGui::Selectable("(none)", object.prefab.empty())) apply({ { "op", "set" }, { "object", key }, { "field", "prefab" }, { "value", "" } });
			for (const auto& name : m_prefabs.names())
			{
				if (ImGui::Selectable(name.c_str(), name == object.prefab)) apply({ { "op", "set" }, { "object", key }, { "field", "prefab" }, { "value", name } });
			}
			ImGui::EndCombo();
		}
		if (object.prefab.empty()) ImGui::TextDisabled("model %s", object.mesh.c_str());
		if (!object.prefab.empty())
		{
			ImGui::BeginDisabled(object.overrides.empty());
			if (ImGui::Button("Apply to prefab")) apply({ { "op", "apply-to-prefab" }, { "object", key } });
			ImGui::SameLine();
			if (ImGui::Button("Revert all")) apply({ { "op", "revert" }, { "object", key } });
			ImGui::EndDisabled();
			if (!object.overrides.empty()) ImGui::TextColored(kOverridden, "overrides its prefab (in blue)");
		}
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputTextWithHint("##newprefab", "new prefab's name", m_newPrefab, sizeof(m_newPrefab));
		ImGui::SameLine();
		if (ImGui::Button("Make prefab") && m_newPrefab[0])
		{
			if (apply({ { "op", "make-prefab" }, { "object", key }, { "name", std::string(m_newPrefab) } })) m_newPrefab[0] = 0;
		}

		ImGui::SeparatorText("Components");
		std::string error;
		const Json resolved = m_prefabs.resolve(object, error);
		if (!error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s", error.c_str());
		else draw_components(*selected, resolved);
		ImGui::End();
	}

	void LevelEditorState::draw_components(LevelObject& object, const Json& resolved)
	{
		const std::string key = object.key;
		const Json overrides = object.overrides;
		const Json components = resolved.value("components", Json::object());
		std::string error;
		const Json base = object.prefab.empty() ? Json::object() : m_prefabs.prefab(object.prefab, error).value("components", Json::object());
		for (auto it = components.begin(); it != components.end(); ++it)
		{
			const std::string name = it.key();
			ImGui::PushID(name.c_str());
			const bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
			if (ImGui::SmallButton("Remove"))
			{
				// one the prefab has is removed by an override; one of its own goes with its override
				if (base.contains(name)) apply({ { "op", "override" }, { "object", key }, { "path", "/components/" + name }, { "value", nullptr } });
				else apply({ { "op", "revert" }, { "object", key }, { "path", "/components/" + name } });
				ImGui::PopID();
				return;   // the document changed under this loop
			}
			if (open)
			{
				const ComponentKind* kind = m_schema.find(name);
				if (!kind)
				{
					ImGui::TextDisabled("not in the schema: %s", it.value().dump().c_str());
				}
				else
				{
					if (!kind->help.empty()) ImGui::TextDisabled("%s", kind->help.c_str());
					const Json fields = m_schema.with_defaults(name, it.value());
					for (const auto& field : kind->fields)
					{
						const std::string path = "/components/" + name + "/" + field.name;
						const bool overridden = overrides.contains(Json::json_pointer(path));
						const Json current = fields.value(field.name, field.value);
						ImGui::PushID(field.name.c_str());
						if (overridden) ImGui::PushStyleColor(ImGuiCol_Text, kOverridden);
						Json changed;
						bool edited = false;
						using Type = FieldSchema::Type;
						switch (field.type)
						{
						case Type::Float:
						{
							float value = current.is_number() ? current.get<float>() : 0.0f;
							if (ImGui::InputFloat(field.name.c_str(), &value, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
							{
								if (field.max > field.min) value = std::clamp(value, static_cast<float>(field.min), static_cast<float>(field.max));
								changed = value;
								edited = true;
							}
							break;
						}
						case Type::Int:
						{
							int value = current.is_number() ? current.get<int>() : 0;
							if (ImGui::InputInt(field.name.c_str(), &value, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue)) { changed = value; edited = true; }
							break;
						}
						case Type::Bool:
						{
							bool value = current.is_boolean() && current.get<bool>();
							if (ImGui::Checkbox(field.name.c_str(), &value)) { changed = value; edited = true; }
							break;
						}
						case Type::Enum:
						{
							const std::string value = text_of(current);
							if (ImGui::BeginCombo(field.name.c_str(), value.c_str()))
							{
								for (const auto& option : field.options)
								{
									if (ImGui::Selectable(option.c_str(), option == value)) { changed = option; edited = true; }
								}
								ImGui::EndCombo();
							}
							break;
						}
						case Type::Color:
						case Type::Vec3:
						{
							float value[3] = { 0.0f, 0.0f, 0.0f };
							for (size_t i = 0; i < 3 && current.is_array() && i < current.size(); ++i) value[i] = current[i].get<float>();
							// a color applies as it changes (each change undoable); a vector when Enter is pressed
							const bool done = field.type == Type::Color ? ImGui::ColorEdit3(field.name.c_str(), value, ImGuiColorEditFlags_NoInputs)
								: ImGui::InputFloat3(field.name.c_str(), value, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
							if (done) { changed = Json::array({ value[0], value[1], value[2] }); edited = true; }
							break;
						}
						case Type::IntList:
						{
							std::string list;
							for (const auto& v : current.is_array() ? current : Json::array()) list += (list.empty() ? "" : ", ") + v.dump();
							char buffer[256];
							std::snprintf(buffer, sizeof(buffer), "%s", list.c_str());
							if (ImGui::InputText(field.name.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
							{
								changed = Json::array();
								for (const char* p = buffer; *p;)
								{
									char* end = nullptr;
									const long v = std::strtol(p, &end, 10);
									if (end == p) { ++p; continue; }
									changed.push_back(static_cast<int>(v));
									p = end;
								}
								edited = true;
							}
							break;
						}
						case Type::String:
						case Type::Asset:
						default:
						{
							char buffer[256];
							std::snprintf(buffer, sizeof(buffer), "%s", text_of(current).c_str());
							if (ImGui::InputText(field.name.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) { changed = std::string(buffer); edited = true; }
							break;
						}
						}
						if (overridden) ImGui::PopStyleColor();
						if (!field.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", field.help.c_str());
						if (overridden)
						{
							ImGui::SameLine();
							if (ImGui::SmallButton("revert"))
							{
								apply({ { "op", "revert" }, { "object", key }, { "path", path } });
								ImGui::PopID();
								ImGui::PopID();
								return;   // the document changed under this loop
							}
						}
						ImGui::PopID();
						if (edited)
						{
							apply({ { "op", "override" }, { "object", key }, { "path", path }, { "value", changed } });
							ImGui::PopID();
							return;   // the document changed under this loop
						}
					}
				}
			}
			ImGui::PopID();
		}
		// a component to add
		if (ImGui::BeginCombo("##add", "Add component"))
		{
			for (const auto& kind : m_schema.kinds())
			{
				if (components.contains(kind.name)) continue;
				if (ImGui::Selectable(kind.name.c_str()))
				{
					apply({ { "op", "override" }, { "object", key }, { "path", "/components/" + kind.name }, { "value", Json::object() } });
					break;
				}
			}
			ImGui::EndCombo();
		}
	}

	void LevelEditorState::draw_gizmo()
	{
		m_gizmoActive = false;
		if (m_shot >= 0 && m_selection.empty())
		{
			// a shot's point: moved, never turned or scaled
			if (m_lookThrough || m_previewing) return;
			Json path = camera_path();
			if (!path.is_object() || m_shot >= static_cast<int>(path.value("shots", Json::array()).size())) return;
			const char* which = m_shotLook ? "look" : "eye";
			const Json& p = path["shots"][static_cast<size_t>(m_shot)][which];
			const CameraPan::Point at = LevelCameraPath::to_render(p[0].get<double>(), p[1].get<double>(), p[2].get<double>(), m_document->unitsPerMeter);
			auto& camera = m_runtime->camera();
			const ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::BeginFrame();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
			Mat4 matrix{};
			matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
			matrix[12] = at[0];
			matrix[13] = at[1];
			matrix[14] = at[2];
			if (m_dragging) matrix = m_dragged;
			float snap[3] = { m_snapMove, m_snapMove, m_snapMove };
			if (ImGuizmo::Manipulate(camera.get_view_matrix(), camera.get_projection_matrix(), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, matrix.data(), nullptr,
				m_snap != io.KeyCtrl ? snap : nullptr))
			{
				m_dragged = matrix;
				m_dragging = true;
			}
			m_gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
			if (m_dragging && !ImGuizmo::IsUsing())
			{
				m_dragging = false;
				path["shots"][static_cast<size_t>(m_shot)][which] = level_point({ m_dragged[12], m_dragged[13], m_dragged[14] });
				set_camera_path(path);
			}
			return;
		}
		LevelObject* object = selected_object();
		LevelObjects::Instance* instance = object ? m_objects.find(object->key) : nullptr;
		if (!object || !instance || !instance->mesh) return;
		auto& camera = m_runtime->camera();
		const ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::BeginFrame();
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
		Mat4 matrix = level_placement(*object, m_document->unitsPerMeter);
		const ImGuizmo::OPERATION operation = m_gizmo == Gizmo::Move ? ImGuizmo::TRANSLATE : m_gizmo == Gizmo::Turn ? ImGuizmo::ROTATE : ImGuizmo::SCALEU;
		float snap[3];
		const float step = m_gizmo == Gizmo::Move ? m_snapMove : m_gizmo == Gizmo::Turn ? m_snapTurn : m_snapScale;
		snap[0] = snap[1] = snap[2] = step;
		const bool snapping = m_snap != io.KeyCtrl;   // Ctrl turns the snap over while dragging
		if (ImGuizmo::Manipulate(camera.get_view_matrix(), camera.get_projection_matrix(), operation, m_local ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
			matrix.data(), nullptr, snapping ? snap : nullptr))
		{
			instance->mesh->set_matrix(matrix.data());   // seen at once; the document changes on release
			m_dragged = matrix;
			m_dragging = true;
		}
		m_gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
		if (m_dragging && !ImGuizmo::IsUsing())
		{
			m_dragging = false;
			double position[3], rotation[3], scale = 1.0;
			EditorMath::decompose(m_dragged, m_document->unitsPerMeter, position, rotation, scale);
			const double u = m_document->unitsPerMeter;
			Json operation = { { "op", "place" }, { "object", object->key }, { "x", tidy(position[0] / u) }, { "y", tidy(position[1] / u) }, { "z", tidy(position[2] / u) } };
			if (object->has_basis)
			{
				// its own matrix stays one (a tilt or a scale per axis that angles and one scale cannot say)
				Json basis = Json::array();
				for (int column = 0; column < 3; ++column)
				{
					for (int row = 0; row < 3; ++row) basis.push_back(std::round(double(m_dragged[column * 4 + row]) * 1e6) / 1e6);
				}
				operation["basis"] = basis;
			}
			else
			{
				operation["rot"] = Json::array({ tidy(rotation[0]), tidy(rotation[1]), tidy(rotation[2]) });
				operation["scale"] = tidy(scale);
			}
			apply(operation);
		}
	}

	void LevelEditorState::draw_selection()
	{
		auto& graphics = m_runtime->graphics();
		for (size_t s = 0; s < m_selection.size(); ++s)
		{
			const int index = m_document->find(m_selection[s]);
			LevelObjects::Instance* instance = m_objects.find(m_selection[s]);
			if (index < 0 || !instance || !instance->mesh) continue;
			const Mat4 placement = level_placement(m_document->objects[static_cast<size_t>(index)], m_document->unitsPerMeter);
			float lo[3], hi[3];
			instance->mesh->get_bounds(lo, hi);
			tmath::vector3df corner[8];
			for (int c = 0; c < 8; ++c)
			{
				const auto p = EditorMath::transform(placement, (c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2], 1.0f);
				corner[c] = tmath::vector3df(p[0], p[1], p[2]);
			}
			const uint32_t color = s == 0 ? 0xff20d0ff : 0xffd0d040;
			static const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
			for (const auto& edge : edges) graphics.draw_line3d(corner[edge[0]], corner[edge[1]], color, false);
		}
	}

	void LevelEditorState::viewport_input()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard)
		{
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) show_opener();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo(false);
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) undo(true);
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
			{
				if (const LevelObject* object = selected_object())
				{
					const double u = m_document->unitsPerMeter;
					const size_t before = m_document->objects.size();
					if (apply({ { "op", "copy" }, { "object", object->key }, { "x", object->x / u + 1.0 }, { "y", object->y / u } }) && m_document->objects.size() > before)
					{
						m_selection = { m_document->objects.back().key };
					}
				}
			}
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.KeyCtrl)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_gizmo = Gizmo::Move;
				if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_gizmo = Gizmo::Turn;
				if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_gizmo = Gizmo::Scale;
				if (ImGui::IsKeyPressed(ImGuiKey_F, false)) focus_selection();
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			{
				for (const auto& key : std::vector<std::string>(m_selection)) apply({ { "op", "delete" }, { "object", key } });
				m_selection.clear();
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				if (m_lookThrough || m_previewing) stop_preview();
				else { m_placing.clear(); m_selection.clear(); m_shot = -1; }
			}
			if (m_play && ImGui::IsKeyPressed(ImGuiKey_F5, false)) m_playAsked = true;
		}

		// the camera: right drag turns it (and WASD flies), middle drag pans, the wheel zooms
		if (!io.WantCaptureMouse)
		{
			// moving the free camera leaves a shot looked through
			if ((ImGui::IsMouseDragging(ImGuiMouseButton_Right, 2.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 2.0f) || io.MouseWheel != 0.0f) && m_lookThrough)
				m_lookThrough = false;
			if (io.MouseWheel != 0.0f) m_distance = std::clamp(m_distance * std::pow(0.85f, io.MouseWheel), 1.0f, 1500.0f);
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
			{
				m_yaw -= io.MouseDelta.x * 0.005f;
				m_pitch = std::clamp(m_pitch + io.MouseDelta.y * 0.005f, 0.05f, 1.55f);
			}
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
			{
				const float speed = m_distance * 0.0015f;
				const float rightX = std::cos(m_yaw), rightZ = -std::sin(m_yaw), forwardX = -std::sin(m_yaw), forwardZ = -std::cos(m_yaw);
				m_target[0] += (-io.MouseDelta.x * rightX + io.MouseDelta.y * forwardX) * speed;
				m_target[2] += (-io.MouseDelta.x * rightZ + io.MouseDelta.y * forwardZ) * speed;
			}
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureKeyboard)
		{
			const float speed = m_distance * 0.8f * io.DeltaTime * (io.KeyShift ? 3.0f : 1.0f);
			const float rightX = std::cos(m_yaw), rightZ = -std::sin(m_yaw), forwardX = -std::sin(m_yaw), forwardZ = -std::cos(m_yaw);
			float f = 0.0f, r = 0.0f;
			if (ImGui::IsKeyDown(ImGuiKey_W)) f += 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_S)) f -= 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_D)) r += 1.0f;
			if (ImGui::IsKeyDown(ImGuiKey_A)) r -= 1.0f;
			m_target[0] += (forwardX * f + rightX * r) * speed;
			m_target[2] += (forwardZ * f + rightZ * r) * speed;
		}

		// a click: places the prefab being placed, or picks what is under it
		if (io.WantCaptureMouse || m_gizmoActive || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
		auto& camera = m_runtime->camera();
		EditorMath::Ray ray;
		if (!EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, ray)) return;
		if (!m_placing.empty())
		{
			float x = 0.0f, z = 0.0f;
			if (!EditorMath::hit_floor(ray, 0.0f, x, z)) return;
			const size_t before = m_document->objects.size();
			const float grid = m_snap ? m_snapMove : 0.0f;
			auto snapped = [grid](float v) { return grid > 0.0f ? std::round(v / grid) * grid : v; };
			if (apply({ { "op", "instantiate" }, { "prefab", m_placing }, { "x", tidy(snapped(x)) }, { "y", tidy(snapped(-z)) } }) && m_document->objects.size() > before)
			{
				m_selection = { m_document->objects.back().key };
			}
			if (!io.KeyShift) m_placing.clear();   // Shift keeps placing
			return;
		}
		if (!io.KeyShift && pick_shot(io.MousePos.x, io.MousePos.y))
		{
			m_selection.clear();
			return;
		}
		const std::string picked = pick_at(ray);
		if (picked.empty())
		{
			if (!io.KeyShift)
			{
				m_selection.clear();
				m_shot = -1;
			}
			return;
		}
		auto found = std::find(m_selection.begin(), m_selection.end(), picked);
		if (io.KeyShift)
		{
			if (found != m_selection.end()) m_selection.erase(found);
			else m_selection.push_back(picked);
		}
		else
		{
			m_selection = { picked };
		}
	}

	std::string LevelEditorState::pick_at(const EditorMath::Ray& ray) const
	{
		// of the boxes the ray goes through, the smallest across (its longest side: a flat decal on the floor is not small): a
		// block of scenery's box holds the props on it
		double smallest = 1e300;
		std::string picked;
		for (size_t i = 0; i < m_objects.instances().size() && i < m_document->objects.size(); ++i)
		{
			const auto& instance = m_objects.instances()[i];
			if (!instance.mesh) continue;
			if (m_lockBase && m_layer.is_base(instance.key)) continue;
			const LevelObject& object = m_document->objects[i];
			float lo[3], hi[3];
			instance.mesh->get_bounds(lo, hi);
			if (EditorMath::hit_box(ray, level_placement(object, m_document->unitsPerMeter), lo, hi) < 0.0f) continue;
			const double across = std::max({ double(hi[0] - lo[0]), double(hi[1] - lo[1]), double(hi[2] - lo[2]) }) * object.scale;
			if (across < smallest)
			{
				smallest = across;
				picked = instance.key;
			}
		}
		return picked;
	}

	// PAKAL_EDITOR_SELFTEST=1: the editor tries itself, frame by frame, without a mouse: it picks an object through the
	// point of the screen its middle is drawn at, moves it as a gizmo would (and undoes it), plays the level and stops, with
	// a capture at each stage (editor_<stage>.tga next to the game). The results go to the log ("[LevelEditor] self test").
	void LevelEditorState::self_test()
	{
		++m_testFrame;
		auto& graphics = m_runtime->graphics();
		auto report = [this](bool ok, const char* what)
		{
			if (!ok) ++m_testFailures;
			LOG_INFO("[LevelEditor] self test %s: %s", ok ? "PASS" : "FAIL", what);
		};
		if (m_playing)
		{
			if (m_testFrame == m_testPlayFrame + 300) graphics.request_screenshot("editor_play.tga");
			if (m_testFrame == m_testPlayFrame + 320) m_engine->game_state_manager()->request_pop();
			return;
		}
		if (m_testFrame == 30)
		{
			// the small object drawn nearest the middle of the view
			const ImGuiIO& io = ImGui::GetIO();
			auto& camera = m_runtime->camera();
			Mat4 view, projection;
			std::copy(camera.get_view_matrix(), camera.get_view_matrix() + 16, view.begin());
			std::copy(camera.get_projection_matrix(), camera.get_projection_matrix() + 16, projection.begin());
			const Mat4 viewProjection = mat4_mul(projection, view);
			float best = 1e30f, sx = 0.0f, sy = 0.0f;
			std::string expected;
			for (size_t i = 0; i < m_objects.instances().size(); ++i)
			{
				const auto& instance = m_objects.instances()[i];
				if (!instance.mesh) continue;
				float lo[3], hi[3];
				instance.mesh->get_bounds(lo, hi);
				const LevelObject& object = m_document->objects[i];
				if (std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] }) * object.scale > 3.0) continue;
				const Mat4 placement = level_placement(object, m_document->unitsPerMeter);
				const auto middle = EditorMath::transform(placement, (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f, 1.0f);
				const auto clip = EditorMath::transform(viewProjection, middle[0], middle[1], middle[2], 1.0f);
				if (clip[3] <= 0.0f) continue;
				const float x = (clip[0] / clip[3] * 0.5f + 0.5f) * io.DisplaySize.x, y = (0.5f - clip[1] / clip[3] * 0.5f) * io.DisplaySize.y;
				const float off = std::hypot(x - io.DisplaySize.x * 0.5f, y - io.DisplaySize.y * 0.5f);
				if (off < best) { best = off; sx = x; sy = y; expected = instance.key; }
			}
			EditorMath::Ray ray;
			const bool rayOk = EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), sx, sy, io.DisplaySize.x, io.DisplaySize.y, ray);
			const std::string picked = rayOk ? pick_at(ray) : std::string();
			LOG_INFO("[LevelEditor] self test: pointing at %s (%.0f, %.0f), picked %s", expected.c_str(), sx, sy, picked.c_str());
			report(!expected.empty() && picked == expected, "a click on an object picks it");
			m_selection = { picked.empty() ? expected : picked };
			focus_selection();
		}
		if (m_testFrame == 60) graphics.request_screenshot("editor_selected.tga");
		if (m_testFrame == 70)
		{
			LevelObject* object = selected_object();
			if (!object) { report(false, "something is selected"); return; }
			const std::string key = object->key;
			const double x0 = object->x, y0 = object->y, z0 = object->z, u = m_document->unitsPerMeter;
			const std::array<double, 3> rot0 = object->rot;
			const double scale0 = object->scale;
			// what the gizmo does when it is let go: its matrix taken apart into a place operation
			Mat4 moved = level_placement(*object, u);
			moved[12] += 2.0f;
			double position[3], rotation[3], scale = 1.0;
			EditorMath::decompose(moved, u, position, rotation, scale);
			apply({ { "op", "place" }, { "object", key }, { "x", tidy(position[0] / u) }, { "y", tidy(position[1] / u) }, { "z", tidy(position[2] / u) },
				{ "rot", Json::array({ tidy(rotation[0]), tidy(rotation[1]), tidy(rotation[2]) }) }, { "scale", tidy(scale) } });
			object = selected_object();
			// the same placement back from the numbers (an angle may come back as another that turns the same)
			const Mat4 again = level_placement(*object, u);
			float worst = 0.0f;
			for (int i = 0; i < 16; ++i) worst = std::max(worst, std::fabs(again[i] - moved[i]));
			report(object && std::fabs(object->x - (x0 + 2.0 * u)) < 0.5 && std::fabs(object->y - y0) < 0.5 && std::fabs(object->z - z0) < 0.5,
				"a gizmo's move becomes a place operation");
			report(worst < 0.01f && std::fabs(object->scale - scale0) < 0.001, "its rotation and scale stay the same");
			LOG_INFO("[LevelEditor] self test: rotation %.2f %.2f %.2f -> %.2f %.2f %.2f, matrix off by %.5f", rot0[0], rot0[1], rot0[2], object->rot[0],
				object->rot[1], object->rot[2], worst);
			const LevelObjects::Instance* instance = m_objects.find(key);
			report(instance && instance->mesh, "the moved object keeps its model");
			undo(false);
			object = selected_object();
			report(object && std::fabs(object->x - x0) < 0.01, "undo puts it back");
		}
		// live editing: another program moves the selected object in the open editor, and asks what it shows
		if (m_testFrame == 74 && selected_object())
		{
			m_testKey = selected_object()->key;
			m_testX = selected_object()->x;
			const std::string level = m_path, key = m_testKey;
			m_testAsker = std::thread([this, level, key]()
			{
				Json reply;
				std::string error;
				const Json move = { { "op", "move" }, { "object", key }, { "dx", 1.5 }, { "dy", 0.0 } };
				const bool asked = Pakal::LiveEditing::ask(level, { { "ops", Json::array({ move }) } }, reply, error, 5.0);
				m_testReply = asked ? reply : Json{ { "error", error } };
			});
		}
		if (m_testFrame == 86 && m_testAsker.joinable())
		{
			m_testAsker.join();
			const LevelObject* object = selected_object();
			LOG_INFO("[LevelEditor] self test: the live request's answer %s", m_testReply.dump().c_str());
			report(m_testReply.value("ok", false) && object && object->key == m_testKey && std::fabs(object->x - (m_testX + 1.5 * m_document->unitsPerMeter)) < 0.5,
				"another program's operation is done in the open editor");
			report(!m_editor->done().empty() && m_editor->done().back().value("by", std::string()) == "agent", "it is marked as the agent's in the history");
			const Json state = m_testReply.value("state", Json::object());
			report(!state.value("selection", Json::array()).empty() && state["selection"][0].value("key", std::string()) == m_testKey, "the answer says what is selected");
			undo(false);
			report(selected_object() && std::fabs(selected_object()->x - m_testX) < 0.01, "and it is undone with the editor's undo");
		}
		if (m_testFrame == 90 && m_play)
		{
			m_testPlayFrame = m_testFrame;
			m_playAsked = true;
		}
		if (m_testBackFrame > 0 && m_testFrame == m_testBackFrame + 60)
		{
			graphics.request_screenshot("editor_back.tga");
			report(m_viewOpen && !m_objects.instances().empty(), "Stop brings the editor back with its models");
		}
		// a layer over the level: made, a base object hidden and one moved, saved as the layer only, and removed after
		if (m_testBackFrame > 0 && m_testFrame == m_testBackFrame + 70)
		{
			const std::filesystem::path folder = std::filesystem::path(m_path).parent_path();
			m_testLayer = ((folder.parent_path() / (folder.filename().string() + "_selftest_layer")) / "level.json").generic_string();
			const size_t baseObjects = m_document->objects.size();
			std::string error;
			std::filesystem::remove_all(std::filesystem::path(m_testLayer).parent_path());
			report(Pakal::LevelLayerEditing::create(m_testLayer, m_path, error) && open_level(m_testLayer), "a new layer opens over the level");
			report(m_layer.layered() && m_document->objects.size() == baseObjects, "with all of the base's objects");
		}
		if (m_testBackFrame > 0 && m_testFrame == m_testBackFrame + 80 && m_layer.layered() && m_document->objects.size() > 2)
		{
			const std::string hidden = m_document->objects[0].key, moved = m_document->objects[1].key;
			apply({ { "op", "delete" }, { "object", hidden } });
			apply({ { "op", "move" }, { "object", moved }, { "dx", 1.0 }, { "dy", 0.0 } });
			report(save(), "the layer saves");
			Pakal::LevelDocument composed;
			std::string error;
			std::ifstream in(m_testLayer, std::ios::binary);
			const Json file = Json::parse(in, nullptr, false);
			report(file.is_object() && file.value("hide", Json::array()) == Json::array({ hidden }) && file.value("adjust", Json::object()).contains(moved)
				&& file.value("objects", Json::array()).empty(), "it holds only what was hidden and moved");
			report(Pakal::load_level(m_testLayer, composed, error) && composed.find(hidden) < 0 && composed.find(moved) >= 0, "and loads composed as edited");
			graphics.request_screenshot("editor_layer.tga");
		}
		// the intro camera in the layer: two shots over the view's middle, the first's eye taken by a click and moved as the
		// gizmo would, looked through, the path played, and saved in the layer
		const int camera = m_testBackFrame > 0 && m_layer.layered() ? m_testFrame - m_testBackFrame : 0;
		if (camera == 84)
		{
			const CameraPan::Point middle{ m_target[0], 0.0f, m_target[2] };
			const Json shots = Json::array({ { { "eye", level_point({ middle[0] - 4.0f, 6.0f, middle[2] }) }, { "look", level_point(middle) } },
				{ { "eye", level_point({ middle[0] + 4.0f, 3.0f, middle[2] + 4.0f }) }, { "look", level_point(middle) } } });
			report(set_camera_path({ { "seconds", 2.0 }, { "shots", shots } }), "an intro camera is set");
			const Json eye = camera_path()["shots"][0]["eye"];
			const CameraPan::Point at = LevelCameraPath::to_render(eye[0].get<double>(), eye[1].get<double>(), eye[2].get<double>(), m_document->unitsPerMeter);
			const ImGuiIO& io = ImGui::GetIO();
			auto& view = m_runtime->camera();
			float sx = 0.0f, sy = 0.0f;
			const bool seen = EditorMath::to_screen(view.get_view_matrix(), view.get_projection_matrix(), at[0], at[1], at[2], io.DisplaySize.x, io.DisplaySize.y, sx, sy);
			m_selection.clear();
			m_shot = -1;
			report(seen && pick_shot(sx + 3.0f, sy - 2.0f) && m_shot == 0 && !m_shotLook, "a click near a shot's eye takes it");
			m_dragged = Mat4{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, at[0] + 1.0f, at[1], at[2], 1 };
			m_dragging = true;   // let go on the next frame's gizmo
		}
		if (camera == 86)
		{
			const Json eye = camera_path()["shots"][0]["eye"];
			const CameraPan::Point at = LevelCameraPath::to_render(eye[0].get<double>(), eye[1].get<double>(), eye[2].get<double>(), m_document->unitsPerMeter);
			report(std::fabs(at[0] - (m_target[0] - 3.0f)) < 0.01f, "the gizmo moves the shot's eye (an undoable camera operation)");
			m_lookThrough = true;
			graphics.request_screenshot("editor_shot.tga");
		}
		if (camera == 88)
		{
			const auto position = m_runtime->camera().get_position();
			report(m_lookThrough && std::fabs(position[0] - (m_target[0] - 3.0f)) < 0.01f && std::fabs(position[1] - 6.0f) < 0.01f, "the view looks through the shot");
			stop_preview();
			m_previewing = true;
		}
		if (camera == 96)
		{
			const auto position = m_runtime->camera().get_position();
			const CameraPan::Point free = free_eye();
			report(m_previewing && m_previewSeconds > 0.0f && std::fabs(position[0] - free[0]) + std::fabs(position[2] - free[2]) > 0.1f, "the path plays");
			graphics.request_screenshot("editor_path.tga");
		}
		if (camera == 99)
		{
			stop_preview();
			report(save(), "the layer saves with its camera");
			std::ifstream in(m_testLayer, std::ios::binary);
			const Json file = Json::parse(in, nullptr, false);
			report(file.is_object() && file.contains("camera") && file["camera"]["intro"] == camera_path(), "the intro camera is in the layer file");
		}
		if (m_testBackFrame > 0 && m_testFrame == m_testBackFrame + 105 && !m_testLayer.empty())
		{
			std::filesystem::remove_all(std::filesystem::path(m_testLayer).parent_path());
			LOG_INFO("[LevelEditor] self test done: %d failed", m_testFailures);
		}
	}

	// --- opening a level ---

	namespace
	{
		const char* kRecentFile = "editor_recent.json";   // next to the game: the levels opened last (a convenience)
	}

	void LevelEditorState::read_recent()
	{
		m_recent.clear();
		std::ifstream in(kRecentFile);
		if (!in) return;
		const Json list = Json::parse(in, nullptr, false);
		if (!list.is_array()) return;
		for (const auto& item : list)
		{
			if (item.is_string()) m_recent.push_back(item.get<std::string>());
		}
	}

	void LevelEditorState::remember_recent(const std::string& path)
	{
		m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), path), m_recent.end());
		m_recent.insert(m_recent.begin(), path);
		if (m_recent.size() > 10) m_recent.resize(10);
		std::ofstream out(kRecentFile, std::ios::trunc);
		if (out) out << Json(m_recent).dump(1) << "\n";
	}

	void LevelEditorState::show_opener()
	{
		m_opener = true;
		m_discardAsked = false;
		m_openError.clear();
		std::snprintf(m_openPath, sizeof(m_openPath), "%s", m_path.c_str());
		// every level.json under Assets (histories left out)
		m_found.clear();
		std::error_code code;
		for (auto it = std::filesystem::recursive_directory_iterator("Assets", std::filesystem::directory_options::skip_permission_denied, code);
			it != std::filesystem::recursive_directory_iterator(); it.increment(code))
		{
			if (code) break;
			const std::string name = it->path().filename().string();
			if (it->is_directory(code) && name.size() > 8 && name.compare(name.size() - 8, 8, ".history") == 0)
			{
				it.disable_recursion_pending();
				continue;
			}
			if (name == "level.json") m_found.push_back(it->path().generic_string());
		}
		std::sort(m_found.begin(), m_found.end());
	}

	bool LevelEditorState::open_level(const std::string& path)
	{
		std::error_code code;
		if (!std::filesystem::is_regular_file(path, code))
		{
			m_openError = "no file '" + path + "'";
			return false;
		}
		const std::string previous = m_path;
		close_view();
		m_path = path;
		m_selection.clear();
		m_placing.clear();
		if (!reload_document())
		{
			m_openError = m_status;
			m_path = previous;
			if (!previous.empty() && reload_document()) open_view();
			return false;
		}
		m_distance = 30.0f;   // the camera takes in the whole level again
		open_view();
		remember_recent(path);
		m_opener = false;
		m_status = "opened " + path;
		LOG_INFO("[LevelEditor] editing %s: %zu objects, %zu prefabs", m_path.c_str(), m_document->objects.size(), m_prefabs.names().size());
		return true;
	}

	void LevelEditorState::draw_opener()
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(std::min(760.0f, io.DisplaySize.x - 40.0f), std::min(560.0f, io.DisplaySize.y - 40.0f)), ImGuiCond_Appearing);
		bool open = true;
		ImGui::Begin("Open a level", m_document ? &open : nullptr, ImGuiWindowFlags_NoCollapse);
		std::string chosen;
		ImGui::TextDisabled("A level.json (the pakal-level document), relative to the game's folder or a full path");
		ImGui::SetNextItemWidth(-90.0f);
		if (ImGui::InputText("##path", m_openPath, sizeof(m_openPath), ImGuiInputTextFlags_EnterReturnsTrue)) chosen = m_openPath;
		ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2(-1.0f, 0.0f))) chosen = m_openPath;
		if (!m_openError.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_openError.c_str());
		if (!m_recent.empty())
		{
			ImGui::SeparatorText("Recent");
			for (const auto& path : m_recent)
			{
				if (ImGui::Selectable(path.c_str())) std::snprintf(m_openPath, sizeof(m_openPath), "%s", path.c_str());
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) chosen = path;
			}
		}
		ImGui::SeparatorText("Levels under Assets");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##levelfilter", "filter", m_openFilter, sizeof(m_openFilter));
		ImGui::BeginChild("##levels");
		const std::string filter = m_openFilter;
		for (const auto& path : m_found)
		{
			if (!filter.empty() && path.find(filter) == std::string::npos) continue;
			if (ImGui::Selectable(path.c_str(), path == m_openPath)) std::snprintf(m_openPath, sizeof(m_openPath), "%s", path.c_str());
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) chosen = path;
		}
		if (m_found.empty()) ImGui::TextDisabled("none found: type a path above");
		ImGui::EndChild();
		ImGui::End();
		if (!open) m_opener = false;
		if (chosen.empty()) return;
		// what is not saved is not lost without asking
		if (m_dirty && !m_discardAsked)
		{
			m_discardAsked = true;
			m_openError = "the level has changes that are not saved: Open again to leave them, or close this and save";
			return;
		}
		open_level(chosen);
	}

	// --- layers ---

	void LevelEditorState::apply_dimming()
	{
		if (!m_layer.layered()) return;
		for (const auto& instance : m_objects.instances())
		{
			if (!instance.mesh || !m_layer.is_base(instance.key)) continue;
			if (m_dimBase) instance.mesh->set_color(0.55f, 0.55f, 0.62f);
			else instance.mesh->set_color(1.0f, 1.0f, 1.0f);
		}
	}

	void LevelEditorState::draw_hidden()
	{
		const std::vector<std::string> hidden = m_layer.hidden(*m_document);
		if (hidden.empty()) return;
		char header[64];
		std::snprintf(header, sizeof(header), "Hidden from the base (%zu)", hidden.size());
		if (!ImGui::CollapsingHeader(header)) return;
		for (const auto& key : hidden)
		{
			ImGui::PushID(key.c_str());
			if (ImGui::SmallButton("Show"))
			{
				// put back as the base has it, where it stands among the base's objects
				apply({ { "op", "insert" }, { "data", m_layer.base_object(key) }, { "at", m_layer.base_index(key, *m_document) } });
				ImGui::PopID();
				return;   // the list changed
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%s", key.c_str());
			ImGui::PopID();
		}
	}

	void LevelEditorState::draw_new_layer()
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(std::min(700.0f, io.DisplaySize.x - 40.0f), 0.0f), ImGuiCond_Appearing);
		ImGui::Begin("New layer over this level", &m_newLayer, ImGuiWindowFlags_NoCollapse);
		ImGui::TextWrapped("A level that extends %s: it holds only what it adds, hides or changes of it. The game loads the base once and "
			"changes the layer from mission to mission.", m_path.c_str());
		ImGui::SetNextItemWidth(-90.0f);
		const bool entered = ImGui::InputText("##layerpath", m_newLayerPath, sizeof(m_newLayerPath), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		const bool create = ImGui::Button("Create", ImVec2(-1.0f, 0.0f)) || entered;
		if (!m_newLayerError.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_newLayerError.c_str());
		ImGui::End();
		if (!create) return;
		if (m_dirty)
		{
			m_newLayerError = "save the level first (Ctrl+S): the layer is made over the level as its file has it";
			return;
		}
		std::string error;
		const std::string path = m_newLayerPath;
		if (!Pakal::LevelLayerEditing::create(path, m_path, error))
		{
			m_newLayerError = error;
			return;
		}
		m_newLayer = false;
		open_level(path);
	}

	// --- the intro camera ---

	Json LevelEditorState::camera_path() const
	{
		if (!m_document) return Json();
		const auto camera = m_document->extra.find("camera");
		if (camera == m_document->extra.end() || !camera->is_object()) return Json();
		const auto intro = camera->find("intro");
		return intro != camera->end() && intro->is_object() ? *intro : Json();
	}

	bool LevelEditorState::set_camera_path(const Json& path)
	{
		return apply({ { "op", "camera" }, { "path", "intro" }, { "data", path } });
	}

	void LevelEditorState::stop_preview()
	{
		m_previewing = false;
		m_lookThrough = false;
		m_previewSeconds = 0.0f;
	}

	bool LevelEditorState::camera_view(CameraPan::Point& eye, CameraPan::Point& look, float& fovY)
	{
		if (!m_document || (!m_lookThrough && !m_previewing)) return false;
		LevelCameraPath path;
		if (!path.read(camera_path(), m_document->unitsPerMeter, m_fovY))
		{
			stop_preview();
			return false;
		}
		if (!m_previewing)
		{
			if (m_shot < 0 || m_shot >= static_cast<int>(path.keys.size()))
			{
				m_lookThrough = false;
				return false;
			}
			const CameraPan::Key& key = path.keys[static_cast<size_t>(m_shot)];
			eye = key.eye;
			look = key.target;
			fovY = key.fovY > 0.0f ? key.fovY : m_fovY;
			return true;
		}
		CameraPan pan;
		pan.keys = path.keys;
		if (path.endOnPlay)
		{
			// the play camera at rest over the spawn, as the game's rig places it (its default offset, at the level's distance)
			const CameraRig::Settings rig;
			const Json camera = m_document->extra.value("camera", Json::object());
			const float scale = camera.is_object() ? camera.value("distance", 1.0f) : 1.0f;
			const CameraPan::Point focus = m_document->hasSpawn
				? LevelCameraPath::to_render(m_document->spawn[0], m_document->spawn[1], 0.0, m_document->unitsPerMeter)
				: CameraPan::Point{ m_target[0], 0.0f, m_target[2] };
			CameraPan::Key play;
			play.eye = { focus[0] + rig.side * scale, focus[1] + rig.height * scale, focus[2] + rig.back * scale };
			play.target = focus;
			play.fovY = m_fovY;
			play.at = 1.0f;
			pan.keys.push_back(play);
		}
		const float seconds = path.seconds > 0.0f ? path.seconds : kIntroSeconds;
		pan.durationMs = seconds * 1000.0;
		pan.sample(m_previewSeconds * 1000.0, eye, look, fovY);
		if (pan.keys.size() == 1)
		{
			eye = pan.keys[0].eye;
			look = pan.keys[0].target;
			fovY = pan.keys[0].fovY;
		}
		if (fovY <= 0.0f) fovY = m_fovY;
		if (m_previewSeconds > seconds + 0.75f) stop_preview();   // it rests on the last shot a moment, then the free camera
		return true;
	}

	void LevelEditorState::draw_camera_path()
	{
		if (m_previewing || m_lookThrough) return;
		LevelCameraPath path;
		if (!path.read(camera_path(), m_document->unitsPerMeter, m_fovY)) return;
		auto& graphics = m_runtime->graphics();
		auto v = [](const CameraPan::Point& p) { return tmath::vector3df(p[0], p[1], p[2]); };
		for (size_t i = 0; i < path.keys.size(); ++i)
		{
			const CameraPan::Key& key = path.keys[i];
			const bool chosen = static_cast<int>(i) == m_shot && m_selection.empty();
			const uint32_t color = chosen ? 0xff20d0ff : 0xffffc040;
			const float r = 0.35f;
			for (int axis = 0; axis < 3; ++axis)
			{
				CameraPan::Point a = key.eye, b = key.eye;
				a[axis] -= r;
				b[axis] += r;
				graphics.draw_line3d(v(a), v(b), color, false);
				a = key.target;
				b = key.target;
				a[axis] -= r * 0.6f;
				b[axis] += r * 0.6f;
				graphics.draw_line3d(v(a), v(b), chosen && m_shotLook ? 0xff20d0ff : 0xff40ff80, false);
			}
			graphics.draw_line3d(v(key.eye), v(key.target), chosen ? 0xff20d0ff : 0x80ffc040, false);
		}
		// the eye's way through the shots (the play camera's end is not drawn: it is where the player starts)
		if (path.keys.size() < 2) return;
		CameraPan pan;
		pan.keys = path.keys;
		pan.durationMs = 1000.0;
		CameraPan::Point before{}, target{};
		float fov = 0.0f;
		for (int step = 0; step <= 64; ++step)
		{
			CameraPan::Point eye{};
			pan.sample(1000.0 * step / 64.0, eye, target, fov);   // past the last shot it stays there
			if (step > 0) graphics.draw_line3d(v(before), v(eye), 0xc0ffc040, false);
			before = eye;
		}
	}

	bool LevelEditorState::pick_shot(float mouseX, float mouseY)
	{
		LevelCameraPath path;
		if (!path.read(camera_path(), m_document->unitsPerMeter, m_fovY)) return false;
		auto& camera = m_runtime->camera();
		const ImGuiIO& io = ImGui::GetIO();
		float best = 12.0f * 12.0f;   // pixels, squared
		int shot = -1;
		bool look = false;
		for (size_t i = 0; i < path.keys.size(); ++i)
		{
			for (int which = 0; which < 2; ++which)
			{
				const CameraPan::Point& p = which == 0 ? path.keys[i].eye : path.keys[i].target;
				float x = 0.0f, y = 0.0f;
				if (!EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), p[0], p[1], p[2], io.DisplaySize.x, io.DisplaySize.y, x, y))
					continue;
				const float d = (x - mouseX) * (x - mouseX) + (y - mouseY) * (y - mouseY);
				if (d < best)
				{
					best = d;
					shot = static_cast<int>(i);
					look = which == 1;
				}
			}
		}
		if (shot < 0) return false;
		m_shot = shot;
		m_shotLook = look;
		return true;
	}

	void LevelEditorState::draw_camera_tab()
	{
		const Json path = camera_path();
		ImGui::TextWrapped("The intro camera: the shots a mission opens with. Without them the game pans on its own.");
		const CameraPan::Point lookAt{ m_target[0], m_target[1], m_target[2] };
		const Json fromView = { { "eye", level_point(free_eye()) }, { "look", level_point(lookAt) } };
		if (!path.is_object())
		{
			if (ImGui::Button("Add a shot from the view"))
			{
				if (set_camera_path({ { "seconds", kIntroSeconds }, { "shots", Json::array({ fromView }) } }))
				{
					m_shot = 0;
					m_selection.clear();
				}
			}
			return;
		}
		Json edited = path;
		bool changed = false;
		float seconds = path.value("seconds", kIntroSeconds);
		if (ImGui::InputFloat("Seconds", &seconds, 0.5f, 1.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue) && seconds > 0.0f)
		{
			edited["seconds"] = tidy(seconds);
			changed = true;
		}
		bool endOnPlay = path.value("end", std::string("play")) == "play";
		if (ImGui::Checkbox("Ends on the play camera", &endOnPlay))
		{
			if (endOnPlay) edited.erase("end");   // the default
			else edited["end"] = "last";
			changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The game adds the play camera as the last shot, wherever the player starts.");
		if (m_previewing ? ImGui::Button("Stop (Esc)") : ImGui::Button("Play the path"))
		{
			const bool start = !m_previewing;
			stop_preview();
			m_previewing = start;
		}
		if (m_previewing)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%.1f s", m_previewSeconds);
		}

		ImGui::SeparatorText("Shots");
		Json& shots = edited["shots"];
		for (size_t i = 0; i < shots.size(); ++i)
		{
			char label[64];
			if (shots[i].contains("at")) std::snprintf(label, sizeof(label), "Shot %zu  (at %.2f)", i + 1, shots[i]["at"].get<double>());
			else std::snprintf(label, sizeof(label), "Shot %zu", i + 1);
			if (ImGui::Selectable(label, m_shot == static_cast<int>(i) && m_selection.empty()))
			{
				m_shot = static_cast<int>(i);
				m_selection.clear();
			}
		}
		if (ImGui::Button("Add a shot from the view"))
		{
			const size_t at = m_shot >= 0 && m_shot < static_cast<int>(shots.size()) ? static_cast<size_t>(m_shot) + 1 : shots.size();
			shots.insert(shots.begin() + static_cast<std::ptrdiff_t>(at), fromView);
			m_shot = static_cast<int>(at);
			m_selection.clear();
			changed = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("After the chosen shot: the eye where the camera is, looking at the point it turns about.");

		if (m_shot >= 0 && m_shot < static_cast<int>(shots.size()))
		{
			Json& shot = shots[static_cast<size_t>(m_shot)];
			ImGui::SeparatorText("The chosen shot");
			if (ImGui::RadioButton("Move the eye", !m_shotLook)) m_shotLook = false;
			ImGui::SameLine();
			if (ImGui::RadioButton("Move the look", m_shotLook)) m_shotLook = true;
			const double u = m_document->unitsPerMeter;
			for (const char* which : { "eye", "look" })
			{
				float p[3] = { static_cast<float>(shot[which][0].get<double>() / u), static_cast<float>(shot[which][1].get<double>() / u),
					static_cast<float>(shot[which][2].get<double>() / u) };
				if (ImGui::InputFloat3(std::string(which).append(" (m)").c_str(), p, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
				{
					shot[which] = Json::array({ tidy(p[0] * u), tidy(p[1] * u), tidy(p[2] * u) });
					changed = true;
				}
			}
			bool timed = shot.contains("at");
			if (ImGui::Checkbox("Its own time", &timed))
			{
				if (timed) shot["at"] = shots.size() > 1 ? tidy(double(m_shot) / double(shots.size())) : 0.0;
				else shot.erase("at");
				changed = true;
			}
			if (timed)
			{
				float at = shot["at"].get<float>();
				if (ImGui::InputFloat("At (share)", &at, 0.05f, 0.1f, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
				{
					shot["at"] = tidy(std::clamp(at, 0.0f, 1.0f));
					changed = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("When the shot is reached, as a share of the seconds (rising from 0 at the first)");
			}
			bool ownFov = shot.contains("fov");
			if (ImGui::Checkbox("Its own field of view", &ownFov))
			{
				if (ownFov) shot["fov"] = tidy(m_fovY);
				else shot.erase("fov");
				changed = true;
			}
			if (ownFov)
			{
				float fov = shot["fov"].get<float>();
				if (ImGui::InputFloat("Field of view (deg)", &fov, 1.0f, 5.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue) && fov > 1.0f && fov < 170.0f)
				{
					shot["fov"] = tidy(fov);
					changed = true;
				}
			}
			if (ImGui::Button(m_lookThrough ? "Back to the free camera" : "Look through it"))
			{
				const bool through = !m_lookThrough;
				stop_preview();
				m_lookThrough = through;
			}
			ImGui::SameLine();
			if (ImGui::Button("Set from the view"))
			{
				shot["eye"] = fromView["eye"];
				shot["look"] = fromView["look"];
				changed = true;
			}
			if (ImGui::Button("Earlier") && m_shot > 0)
			{
				std::swap(shots[static_cast<size_t>(m_shot)], shots[static_cast<size_t>(m_shot) - 1]);
				--m_shot;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Later") && m_shot + 1 < static_cast<int>(shots.size()))
			{
				std::swap(shots[static_cast<size_t>(m_shot)], shots[static_cast<size_t>(m_shot) + 1]);
				++m_shot;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete the shot"))
			{
				shots.erase(shots.begin() + m_shot);
				m_shot = std::min(m_shot, static_cast<int>(shots.size()) - 1);
				stop_preview();
				changed = true;
			}
		}
		ImGui::Separator();
		if (ImGui::Button("Remove the intro camera"))
		{
			stop_preview();
			m_shot = -1;
			set_camera_path(Json());
			return;
		}
		if (!changed) return;
		if (shots.empty())
		{
			set_camera_path(Json());   // no shots: no path
			m_shot = -1;
		}
		else
		{
			set_camera_path(edited);
		}
	}

	// --- live editing ---

	Json LevelEditorState::live_state() const
	{
		Json state = { { "level", m_path }, { "playing", m_playing.load() } };
		if (!m_document) return state;
		const double u = m_document->unitsPerMeter;
		state["layer"] = m_layer.layered();
		if (m_layer.layered()) state["base"] = m_layer.base_path();
		state["unsaved"] = m_dirty;
		state["objects"] = m_document->objects.size();
		Json selection = Json::array();
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index < 0) continue;
			const LevelObject& o = m_document->objects[static_cast<size_t>(index)];
			Json item = { { "key", o.key }, { "x", tidy(o.x / u) }, { "y", tidy(o.y / u) }, { "z", tidy(o.z / u) }, { "rot", Json::array({ o.rot[0], o.rot[1], o.rot[2] }) },
				{ "scale", o.scale } };
			if (!o.prefab.empty()) item["prefab"] = o.prefab;
			else item["mesh"] = o.mesh;
			if (m_layer.layered()) item["fromBase"] = m_layer.is_base(o.key);
			selection.push_back(item);
		}
		state["selection"] = selection;
		if (m_shot >= 0 && m_selection.empty()) state["shot"] = m_shot;
		// the view: where the camera is and the point it turns about, in level units (z up) and in meters
		const Json eye = level_point(free_eye()), look = level_point({ m_target[0], m_target[1], m_target[2] });
		state["view"] = { { "eye", eye }, { "look", look },
			{ "lookMeters", Json::array({ tidy(m_target[0]), tidy(-m_target[2]), tidy(m_target[1]) }) } };
		return state;
	}

	void LevelEditorState::serve_live()
	{
		const float dt = ImGui::GetIO().DeltaTime;
		m_liveNoteSeconds = std::max(0.0f, m_liveNoteSeconds - dt);
		if (!m_path.empty() && m_live.level() != m_path) m_live.open(m_path);
		m_beatTimer -= dt;
		if (m_beatTimer <= 0.0f)
		{
			m_beatTimer = 1.0f;
			m_live.beat(live_state());
		}
		m_liveTimer -= dt;
		if (m_liveTimer > 0.0f && m_liveWaiting.empty()) return;
		m_liveTimer = 0.1f;
		for (auto& request : m_live.take_requests()) m_liveWaiting.push_back(std::move(request));
		if (m_liveWaiting.empty()) return;

		// not while the person drags something or types in a field: theirs goes first, then these
		const ImGuiIO& io = ImGui::GetIO();
		const bool busy = m_dragging || ImGuizmo::IsUsing() || ImGui::IsAnyItemActive() || (!io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left));
		if (busy && !m_playing && m_document && m_viewOpen) return;

		std::string done;
		int count = 0;
		for (const auto& [id, request] : m_liveWaiting)
		{
			Json reply = { { "ok", false } };
			if (!request.is_object())
			{
				reply["error"] = "not a JSON object";
			}
			else if (request.value("query", std::string()) == "state")
			{
				reply["ok"] = true;
			}
			else if (m_playing)
			{
				reply["error"] = "the editor is playing the level: Stop first";
			}
			else if (!m_document || !m_viewOpen)
			{
				reply["error"] = "the editor has no level open";
			}
			else if (request.value("undo", false) || request.value("redo", false))
			{
				const bool redo = request.value("redo", false);
				const bool can = redo ? m_editor->can_redo() : m_editor->can_undo();
				if (can)
				{
					const bool wasDirty = m_dirty;
					undo(redo);
					reply["ok"] = true;
					reply["what"] = m_status;
					reply["saved"] = !wasDirty && save();
					done = m_status;
					++count;
				}
				else
				{
					reply["error"] = redo ? "nothing to redo" : "nothing to undo";
				}
			}
			else if (request.contains("ops") && request["ops"].is_array())
			{
				const bool wasDirty = m_dirty;
				Json results = Json::array();
				bool ok = true;
				for (const auto& op : request["ops"])
				{
					if (!ok)
					{
						results.push_back("not done: an operation before it failed");
						continue;
					}
					Json marked = op;
					if (marked.is_object()) marked["by"] = "agent";
					ok = marked.is_object() && apply(marked);
					results.push_back(ok ? std::string() : (marked.is_object() ? m_status : std::string("not an operation")));
					if (ok)
					{
						done = marked.value("op", std::string()) + (marked.contains("object") ? " " + text_of(marked["object"]) : std::string());
						++count;
					}
				}
				reply["ok"] = ok;
				reply["results"] = results;
				if (!ok) reply["error"] = "an operation failed (see results)";
				// nothing of the person's was unsaved: the agent's work is on disk at once
				reply["saved"] = count > 0 && !wasDirty && save();
			}
			else
			{
				reply["error"] = "a request has \"ops\", \"undo\", \"redo\" or \"query\"";
			}
			reply["state"] = live_state();
			m_live.answer(id, reply);
		}
		m_liveWaiting.clear();
		if (count > 0)
		{
			m_liveNote = "agent: " + done + (count > 1 ? " (+" + std::to_string(count - 1) + ")" : std::string());
			m_liveNoteSeconds = 6.0f;
			m_status = m_liveNote;
		}
	}
}
