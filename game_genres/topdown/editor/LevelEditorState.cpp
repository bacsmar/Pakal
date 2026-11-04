///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditorState.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/editor/LevelEditorState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "Profiler.h"
#include "mcp/EngineTools.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "topdown/LevelCameraPath.h"
#include "topdown/editor/EditorMath.h"
#include "topdown/view/CameraRig.h"
#include "topdown/view/SceneLook.h"

#include <dear-imgui/imgui.h>
#include <dear-imgui/imgui_internal.h>   // ImGuiItemFlags_MixedValue: a checkbox over objects that differ

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

		// One field of a component, as its schema says: true when it was changed (the new value in 'changed'). 'mixed': the
		// objects edited together differ in it (it shows a dash, and what is set goes to all of them).
		bool edit_field(const FieldSchema& field, const Json& current, bool mixed, Json& changed)
		{
			using Type = FieldSchema::Type;
			const char* label = field.name.c_str();
			// a number or a text over several objects that differ: an empty field with a dash, set on Enter
			auto mixed_text = [&](Json& out) -> bool
			{
				char buffer[256] = {};
				if (!ImGui::InputTextWithHint(label, "-", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue) || !buffer[0]) return false;
				if (field.type == Type::String || field.type == Type::Asset)
				{
					out = std::string(buffer);
					return true;
				}
				char* stop = nullptr;
				const double value = std::strtod(buffer, &stop);
				if (stop == buffer) return false;
				out = field.type == Type::Int ? Json(static_cast<int>(value)) : Json(value);
				return true;
			};
			switch (field.type)
			{
			case Type::Float:
			{
				if (mixed && mixed_text(changed))
				{
					if (field.max > field.min) changed = std::clamp(changed.get<double>(), field.min, field.max);
					return true;
				}
				if (mixed) return false;
				float value = current.is_number() ? current.get<float>() : 0.0f;
				if (!ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) return false;
				if (field.max > field.min) value = std::clamp(value, static_cast<float>(field.min), static_cast<float>(field.max));
				changed = value;
				return true;
			}
			case Type::Int:
			{
				if (mixed) return mixed_text(changed);
				int value = current.is_number() ? current.get<int>() : 0;
				if (!ImGui::InputInt(label, &value, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue)) return false;
				changed = value;
				return true;
			}
			case Type::Bool:
			{
				bool value = !mixed && current.is_boolean() && current.get<bool>();
				if (mixed) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
				const bool clicked = ImGui::Checkbox(label, &value);
				if (mixed) ImGui::PopItemFlag();
				if (!clicked) return false;
				changed = mixed ? true : value;
				return true;
			}
			case Type::Enum:
			{
				const std::string value = mixed ? std::string("-") : text_of(current);
				bool picked = false;
				if (ImGui::BeginCombo(label, value.c_str()))
				{
					for (const auto& option : field.options)
					{
						if (ImGui::Selectable(option.c_str(), !mixed && option == value)) { changed = option; picked = true; }
					}
					ImGui::EndCombo();
				}
				return picked;
			}
			case Type::Color:
			case Type::Vec3:
			{
				float value[3] = { 0.0f, 0.0f, 0.0f };
				for (size_t i = 0; i < 3 && current.is_array() && i < current.size(); ++i) value[i] = current[i].get<float>();
				// a color applies as it changes (each change undoable); a vector when Enter is pressed. Mixed: the first's
				// shown, the label says so
				const std::string shown = mixed ? field.name + " (mixed)" : field.name;
				const bool done = field.type == Type::Color ? ImGui::ColorEdit3(shown.c_str(), value, ImGuiColorEditFlags_NoInputs)
					: ImGui::InputFloat3(shown.c_str(), value, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
				if (!done) return false;
				changed = Json::array({ value[0], value[1], value[2] });
				return true;
			}
			case Type::IntList:
			{
				std::string list;
				if (!mixed)
				{
					for (const auto& v : current.is_array() ? current : Json::array()) list += (list.empty() ? "" : ", ") + v.dump();
				}
				char buffer[256];
				std::snprintf(buffer, sizeof(buffer), "%s", list.c_str());
				if (!ImGui::InputTextWithHint(label, mixed ? "-" : "", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) return false;
				changed = Json::array();
				for (const char* p = buffer; *p;)
				{
					char* end = nullptr;
					const long v = std::strtol(p, &end, 10);
					if (end == p) { ++p; continue; }
					changed.push_back(static_cast<int>(v));
					p = end;
				}
				return true;
			}
			case Type::String:
			case Type::Asset:
			default:
			{
				if (mixed) return mixed_text(changed);
				char buffer[256];
				std::snprintf(buffer, sizeof(buffer), "%s", text_of(current).c_str());
				if (!ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) return false;
				changed = std::string(buffer);
				return true;
			}
			}
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
		// models keep their triangles: a model is dropped on the surface under the pointer
		Pakal::MeshComponent3D_Bgfx::set_keep_pick_geometry(true);
		read_settings();
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
		m_assets.open(*m_runtime);
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
		if (m_testTools.joinable()) m_testTools.join();
		m_device.disconnect();
		m_runtime->on_graphics([this]() { close_view(); m_assets.close(); });
		m_runtime->end();
		m_runtime.reset();
		Pakal::MeshComponent3D_Bgfx::set_keep_pick_geometry(false);
	}

	void LevelEditorState::on_update(unsigned long dtMilliseconds)
	{
		(void)dtMilliseconds;
		if (m_testQuit.exchange(false) && m_engine->os_manager()) m_engine->os_manager()->event_app_finished();
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
		hide_ghost();
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
		// the boxes of the whole selection in sight, from where the camera looks
		float low[3] = { 1e30f, 1e30f, 1e30f }, high[3] = { -1e30f, -1e30f, -1e30f };
		bool any = false;
		const double u = m_document->unitsPerMeter;
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index < 0) continue;
			const LevelObject& object = m_document->objects[static_cast<size_t>(index)];
			float center[3] = { static_cast<float>(object.x / u), static_cast<float>(object.z / u), static_cast<float>(-object.y / u) }, extent[3] = { 0.5f, 0.5f, 0.5f };
			if (LevelObjects::Instance* instance = m_objects.find(key); instance && instance->mesh) instance->mesh->get_world_box(center, extent);
			for (int i = 0; i < 3; ++i)
			{
				low[i] = std::min(low[i], center[i] - extent[i]);
				high[i] = std::max(high[i], center[i] + extent[i]);
			}
			any = true;
		}
		if (!any) return;
		for (int i = 0; i < 3; ++i) m_target[i] = (low[i] + high[i]) * 0.5f;
		const float radius = 0.5f * std::sqrt((high[0] - low[0]) * (high[0] - low[0]) + (high[1] - low[1]) * (high[1] - low[1]) + (high[2] - low[2]) * (high[2] - low[2]));
		const float half = 0.5f * m_fovY * 3.14159265f / 180.0f;
		m_distance = std::clamp(std::max(radius, 0.5f) / std::sin(half) * 1.15f, 2.0f, 1500.0f);
	}

	std::vector<std::string> LevelEditorState::keys_in_rect(float x0, float y0, float x1, float y1)
	{
		if (x0 > x1) std::swap(x0, x1);
		if (y0 > y1) std::swap(y0, y1);
		std::vector<std::string> keys;
		auto& camera = m_runtime->camera();
		const ImGuiIO& io = ImGui::GetIO();
		for (const auto& instance : m_objects.instances())
		{
			if (!instance.mesh || instance.mesh->hidden() || (m_lockBase && m_layer.is_base(instance.key))) continue;
			float center[3], extent[3];
			instance.mesh->get_world_box(center, extent);
			float sx = 0.0f, sy = 0.0f;
			if (!EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), center[0], center[1], center[2], io.DisplaySize.x, io.DisplaySize.y, sx, sy)) continue;
			if (sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1) keys.push_back(instance.key);
		}
		return keys;
	}

	void LevelEditorState::select_same()
	{
		const LevelObject* first = selected_object();
		if (!first) return;
		const std::string prefab = first->prefab, mesh = first->mesh;
		std::vector<std::string> same = { first->key };
		for (const auto& o : m_document->objects)
		{
			if (o.key == first->key || m_hidden.count(o.key) || (m_lockBase && m_layer.is_base(o.key))) continue;
			if (prefab.empty() ? (o.prefab.empty() && o.mesh == mesh) : o.prefab == prefab) same.push_back(o.key);
		}
		m_selection = same;
		m_status = std::to_string(same.size()) + " of " + (prefab.empty() ? mesh : prefab);
	}

	void LevelEditorState::hide_selection(bool isolate)
	{
		if (isolate)
		{
			const std::set<std::string> kept(m_selection.begin(), m_selection.end());
			for (const auto& o : m_document->objects)
			{
				if (!kept.count(o.key)) m_hidden.insert(o.key);
			}
		}
		else
		{
			m_hidden.insert(m_selection.begin(), m_selection.end());
			m_selection.clear();
		}
		apply_dimming();
		m_status = std::to_string(m_hidden.size()) + " hidden from the view (Alt+H shows them)";
	}

	void LevelEditorState::show_all()
	{
		m_hidden.clear();
		apply_dimming();
	}

	void LevelEditorState::draw_box()
	{
		if (!m_boxing) return;
		const ImGuiIO& io = ImGui::GetIO();
		ImDrawList* draw = ImGui::GetForegroundDrawList();
		const ImVec2 a(m_boxFrom[0], m_boxFrom[1]), b(io.MousePos.x, io.MousePos.y);
		const ImVec2 low(std::min(a.x, b.x), std::min(a.y, b.y)), high(std::max(a.x, b.x), std::max(a.y, b.y));
		draw->AddRectFilled(low, high, IM_COL32(80, 160, 255, 40));
		draw->AddRect(low, high, IM_COL32(80, 160, 255, 200));
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
		m_assets.update();
		draw_assets();
		if (m_deviceOpen) m_device.draw(&m_deviceOpen);
		draw_gizmo();
		draw_drop();
		vertex_snap();
		viewport_input();
		draw_selection();
		draw_grid();
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
			ImGui::Separator();
			if (ImGui::MenuItem("Select the same prefab", "Ctrl+Shift+A", false, !m_selection.empty())) select_same();
			if (ImGui::MenuItem("Frame the selection", "F", false, !m_selection.empty())) focus_selection();
			ImGui::Separator();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selection.empty())) copy_selection();
			if (ImGui::MenuItem("Paste (where the camera looks)", "Ctrl+V")) paste(m_target[0], -m_target[2], m_target[1]);
			ImGui::Separator();
			if (ImGui::MenuItem("Hang from the first selected", "Ctrl+P", false, m_selection.size() > 1)) parent_selection(false);
			if (ImGui::MenuItem("Hang from nothing", "Ctrl+Shift+P", false, !m_selection.empty())) parent_selection(true);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::RadioButton("Move (W)", m_gizmo == Gizmo::Move)) m_gizmo = Gizmo::Move;
		if (ImGui::RadioButton("Turn (E)", m_gizmo == Gizmo::Turn)) m_gizmo = Gizmo::Turn;
		if (ImGui::RadioButton("Scale (R)", m_gizmo == Gizmo::Scale)) m_gizmo = Gizmo::Scale;
		ImGui::Checkbox("Local", &m_local);
		ImGui::Checkbox("Snap", &m_snap);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("##snapsteps");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click: the steps");
		if (ImGui::BeginPopup("##snapsteps"))
		{
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::InputFloat("Move (m)", &m_snapMove, 0.0f, 0.0f, "%.2f")) m_snapMove = std::max(0.01f, m_snapMove);
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::InputFloat("Turn (deg)", &m_snapTurn, 0.0f, 0.0f, "%.1f")) m_snapTurn = std::clamp(m_snapTurn, 0.5f, 180.0f);
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::InputFloat("Scale", &m_snapScale, 0.0f, 0.0f, "%.2f")) m_snapScale = std::max(0.01f, m_snapScale);
			ImGui::EndPopup();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", "G", &m_grid);
			ImGui::Separator();
			if (ImGui::MenuItem("Hide the selection", "H", false, !m_selection.empty())) hide_selection(false);
			if (ImGui::MenuItem("Show the selection alone", "Shift+H", false, !m_selection.empty())) hide_selection(true);
			if (ImGui::MenuItem("Show all", "Alt+H", false, !m_hidden.empty())) show_all();
			ImGui::Separator();
			ImGui::MenuItem("Device (profile and tune a running game)", nullptr, &m_deviceOpen);
			ImGui::EndMenu();
		}
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
				// with a hierarchy and no filter: the tree, each object under the one it hangs from; else the list
				std::map<std::string, std::vector<int>> children;
				std::vector<int> roots;
				if (filter.empty())
				{
					for (size_t i = 0; i < m_document->objects.size(); ++i)
					{
						const LevelObject& o = m_document->objects[i];
						if (!o.parent.empty() && m_document->find(o.parent) >= 0) children[o.parent].push_back(static_cast<int>(i));
						else roots.push_back(static_cast<int>(i));
					}
				}
				const bool tree = !children.empty();
				ImGui::BeginChild(tree ? "##tree" : "##objects");
				if (tree)
				{
					for (int root : roots) draw_tree(m_document->objects[static_cast<size_t>(root)].key, children, root);
				}
				ImGuiListClipper clipper;
				clipper.Begin(tree ? 0 : static_cast<int>(shown.size()));
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						const LevelObject& o = m_document->objects[static_cast<size_t>(shown[static_cast<size_t>(row)])];
						const bool selected = std::find(m_selection.begin(), m_selection.end(), o.key) != m_selection.end();
						const bool base = m_layer.is_base(o.key) || m_hidden.count(o.key) > 0;
						if (base) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
						char label[256];
						std::snprintf(label, sizeof(label), "%s%s%s%s##%d", o.key.c_str(), o.prefab.empty() ? "" : "  <", o.prefab.empty() ? "" : (o.prefab + ">").c_str(),
							m_hidden.count(o.key) ? "  (hidden)" : "", shown[static_cast<size_t>(row)]);
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
		if (m_selection.size() > 1)
		{
			draw_inspector_many();
			ImGui::End();
			return;
		}
		// the operations below may change the document: work on a copy of what is shown
		const LevelObject object = *selected;
		const std::string key = object.key;
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

	bool LevelEditorState::apply_all(const std::vector<Json>& ops)
	{
		if (ops.empty()) return false;
		return apply(ops.size() == 1 ? ops.front() : Json{ { "op", "batch" }, { "ops", ops } });
	}

	void LevelEditorState::draw_inspector_many()
	{
		// the objects as they are now (copies: an operation below changes the document)
		std::vector<LevelObject> objects;
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index >= 0) objects.push_back(m_document->objects[static_cast<size_t>(index)]);
		}
		if (objects.empty()) return;
		ImGui::TextColored(kOverridden, "%zu objects", objects.size());
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("A field set here is set on all of them (one undo); one they differ in shows a dash.");
		ImGui::PopStyleColor();
		const double u = m_document->unitsPerMeter;
		auto each = [&](const std::function<Json(const LevelObject&)>& op)
		{
			std::vector<Json> ops;
			for (const auto& o : objects)
			{
				Json one = op(o);
				if (!one.is_null()) ops.push_back(std::move(one));
			}
			apply_all(ops);
		};

		// a text field of the object's own
		auto text_many = [&](const char* label, const std::function<std::string(const LevelObject&)>& get, const char* field)
		{
			const std::string first = get(objects.front());
			bool same = true;
			for (const auto& o : objects) same = same && get(o) == first;
			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", same ? first.c_str() : "");
			if (ImGui::InputTextWithHint(label, same ? "" : "-", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				const std::string value = buffer;
				each([&](const LevelObject& o) { return Json{ { "op", "set" }, { "object", o.key }, { "field", field }, { "value", value } }; });
			}
		};
		text_many("Name", [](const LevelObject& o) { return o.name; }, "name");
		text_many("Role", [](const LevelObject& o) { return o.role; }, "role");
		{
			bool same = true;
			for (const auto& o : objects) same = same && o.zone == objects.front().zone;
			const std::string preview = !same ? std::string("-") : (objects.front().zone.empty() ? std::string("(none)") : objects.front().zone);
			if (ImGui::BeginCombo("Zone", preview.c_str()))
			{
				std::string chosen;
				bool picked = false;
				if (ImGui::Selectable("(none)")) picked = true;
				for (const auto& zone : m_document->zones)
				{
					if (ImGui::Selectable(zone.name.c_str())) { chosen = zone.name; picked = true; }
				}
				ImGui::EndCombo();
				if (picked) each([&](const LevelObject& o) { return Json{ { "op", "set" }, { "object", o.key }, { "field", "zone" }, { "value", chosen } }; });
			}
		}

		ImGui::SeparatorText("Transform");
		// three numbers on a row, each its own field: one that differs shows a dash, and what is typed goes to all
		auto row = [&](const char* label, const std::function<double(const LevelObject&, int)>& get, int count, const char* format,
			const std::function<Json(const LevelObject&, int, double)>& set)
		{
			ImGui::PushID(label);
			const float width = (ImGui::CalcItemWidth() - ImGui::GetStyle().ItemInnerSpacing.x * float(count - 1)) / float(count);
			for (int axis = 0; axis < count; ++axis)
			{
				if (axis > 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::PushID(axis);
				ImGui::SetNextItemWidth(width);
				const double first = get(objects.front(), axis);
				bool same = true;
				for (const auto& o : objects) same = same && std::fabs(get(o, axis) - first) < 1e-6;
				char buffer[64] = {};
				if (same) std::snprintf(buffer, sizeof(buffer), format, first);
				if (ImGui::InputTextWithHint("##v", "-", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific) && buffer[0])
				{
					const double value = std::strtod(buffer, nullptr);
					each([&](const LevelObject& o) { return set(o, axis, value); });
				}
				ImGui::PopID();
			}
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(label);
			ImGui::PopID();
		};
		static const char* kAxes[] = { "x", "y", "z" };
		row("Position (m)", [u](const LevelObject& o, int axis) { return (axis == 0 ? o.x : axis == 1 ? o.y : o.z) / u; }, 3, "%.2f",
			[](const LevelObject& o, int axis, double value) { return Json{ { "op", "place" }, { "object", o.key }, { kAxes[axis], tidy(value) } }; });
		row("Rotation (deg)", [](const LevelObject& o, int axis) { return o.rot[static_cast<size_t>(axis)]; }, 3, "%.1f",
			[](const LevelObject& o, int axis, double value)
			{
				Json rot = Json::array({ o.rot[0], o.rot[1], o.rot[2] });
				rot[static_cast<size_t>(axis)] = tidy(value);
				return Json{ { "op", "place" }, { "object", o.key }, { "rot", rot } };
			});
		row("Scale", [](const LevelObject& o, int) { return o.scale; }, 1, "%.3f",
			[](const LevelObject& o, int, double value) { return value > 0.0 ? Json{ { "op", "place" }, { "object", o.key }, { "scale", value } } : Json(); });

		ImGui::SeparatorText("Prefab");
		bool samePrefab = true, anyOverrides = false;
		for (const auto& o : objects)
		{
			samePrefab = samePrefab && o.prefab == objects.front().prefab;
			anyOverrides = anyOverrides || (!o.prefab.empty() && !o.overrides.empty());
		}
		ImGui::TextDisabled("%s", samePrefab ? (objects.front().prefab.empty() ? "(none)" : objects.front().prefab.c_str()) : "- (several)");
		ImGui::BeginDisabled(!anyOverrides);
		if (ImGui::Button("Apply to prefab"))
		{
			each([](const LevelObject& o) { return !o.prefab.empty() && !o.overrides.empty() ? Json{ { "op", "apply-to-prefab" }, { "object", o.key } } : Json(); });
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert all"))
		{
			each([](const LevelObject& o) { return !o.overrides.empty() ? Json{ { "op", "revert" }, { "object", o.key } } : Json(); });
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Select the same prefab")) select_same();

		// the components all of them have, each field shown once
		ImGui::SeparatorText("Components");
		std::vector<Json> resolved;
		for (const auto& o : objects)
		{
			std::string error;
			resolved.push_back(m_prefabs.resolve(o, error).value("components", Json::object()));
		}
		std::vector<std::string> common;
		for (auto it = resolved.front().begin(); it != resolved.front().end(); ++it)
		{
			bool everywhere = true;
			for (const auto& components : resolved) everywhere = everywhere && components.contains(it.key());
			if (everywhere) common.push_back(it.key());
		}
		if (common.empty()) ImGui::TextDisabled("none that all of them have");
		for (const auto& name : common)
		{
			ImGui::PushID(name.c_str());
			const bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
			if (ImGui::SmallButton("Remove"))
			{
				// from each: one its prefab has is removed by an override; one of its own goes with its override
				each([&](const LevelObject& o)
				{
					std::string error;
					const bool inPrefab = !o.prefab.empty() && m_prefabs.prefab(o.prefab, error).value("components", Json::object()).contains(name);
					return inPrefab ? Json{ { "op", "override" }, { "object", o.key }, { "path", "/components/" + name }, { "value", nullptr } }
						: Json{ { "op", "revert" }, { "object", o.key }, { "path", "/components/" + name } };
				});
				ImGui::PopID();
				return;   // the document changed
			}
			const ComponentKind* kind = open ? m_schema.find(name) : nullptr;
			if (open && !kind) ImGui::TextDisabled("not in the schema");
			for (const auto& field : kind ? kind->fields : std::vector<FieldSchema>())
			{
				const std::string path = "/components/" + name + "/" + field.name;
				const Json first = m_schema.with_defaults(name, resolved.front()[name]).value(field.name, field.value);
				bool mixed = false;
				for (const auto& components : resolved) mixed = mixed || m_schema.with_defaults(name, components[name]).value(field.name, field.value) != first;
				ImGui::PushID(field.name.c_str());
				Json changed;
				const bool edited = edit_field(field, first, mixed, changed);
				if (!field.help.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", field.help.c_str());
				ImGui::PopID();
				if (edited)
				{
					each([&](const LevelObject& o) { return Json{ { "op", "override" }, { "object", o.key }, { "path", path }, { "value", changed } }; });
					ImGui::PopID();
					return;   // the document changed
				}
			}
			ImGui::PopID();
		}
		if (ImGui::BeginCombo("##addmany", "Add component to all"))
		{
			for (const auto& kind : m_schema.kinds())
			{
				if (std::find(common.begin(), common.end(), kind.name) != common.end()) continue;
				if (ImGui::Selectable(kind.name.c_str()))
				{
					std::vector<Json> ops;
					for (size_t i = 0; i < objects.size(); ++i)
					{
						if (!resolved[i].contains(kind.name)) ops.push_back({ { "op", "override" }, { "object", objects[i].key }, { "path", "/components/" + kind.name }, { "value", Json::object() } });
					}
					apply_all(ops);
					break;
				}
			}
			ImGui::EndCombo();
		}
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
						const bool edited = edit_field(field, current, false, changed);
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
		if (m_vertexDragging || (ImGui::IsKeyDown(ImGuiKey_V) && !ImGui::GetIO().WantCaptureKeyboard)) return;   // V: the vertex snap's
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
			// the rest of the selection follows: the same move, or the same turn and scale about each one's own pivot
			for (const auto& key : m_selection)
			{
				const int index = m_document->find(key);
				LevelObjects::Instance* other = key == object->key ? nullptr : m_objects.find(key);
				if (index < 0 || !other || !other->mesh) continue;
				other->mesh->set_matrix(follow(level_placement(m_document->objects[static_cast<size_t>(index)], m_document->unitsPerMeter),
					level_placement(*object, m_document->unitsPerMeter), matrix).data());
			}
			// and what hangs from them, rigidly
			std::map<std::string, std::pair<Mat4, Mat4>> moved;
			const Mat4 first = level_placement(*object, m_document->unitsPerMeter);
			for (const auto& key : m_selection)
			{
				const int index = m_document->find(key);
				if (index < 0) continue;
				const Mat4 was = level_placement(m_document->objects[static_cast<size_t>(index)], m_document->unitsPerMeter);
				moved[key] = { was, key == object->key ? matrix : follow(was, first, matrix) };
			}
			carry_children(moved, nullptr);
		}
		m_gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
		if (m_dragging && !ImGuizmo::IsUsing())
		{
			m_dragging = false;
			const Mat4 before = level_placement(*object, m_document->unitsPerMeter);
			std::vector<Json> ops;
			std::map<std::string, std::pair<Mat4, Mat4>> moved;
			for (const auto& key : m_selection)
			{
				const int index = m_document->find(key);
				if (index < 0) continue;
				const LevelObject& o = m_document->objects[static_cast<size_t>(index)];
				const Mat4 placed = key == object->key ? m_dragged : follow(level_placement(o, m_document->unitsPerMeter), before, m_dragged);
				ops.push_back(place_operation(o, placed));
				moved[key] = { level_placement(o, m_document->unitsPerMeter), placed };
			}
			carry_children(moved, &ops);
			apply_all(ops);
		}
	}

	Mat4 LevelEditorState::follow(const Mat4& placement, const Mat4& before, const Mat4& after)
	{
		// the first's change: its move added; its turn and scale (after * before^-1, the 3x3) applied about this one's pivot
		Mat4 inverse;
		Mat4 out = placement;
		if (!EditorMath::inverse(before, inverse)) return out;
		Mat4 change = mat4_mul(after, inverse);
		change[12] = change[13] = change[14] = 0.0f;
		out = mat4_mul(change, placement);
		for (int i = 0; i < 3; ++i) out[12 + i] = placement[12 + i] + (after[12 + i] - before[12 + i]);
		return out;
	}

	void LevelEditorState::carry_children(const std::map<std::string, std::pair<Mat4, Mat4>>& moved, std::vector<Json>* ops)
	{
		if (moved.empty()) return;
		const double u = m_document->unitsPerMeter;
		for (const auto& o : m_document->objects)
		{
			if (o.parent.empty() || moved.count(o.key)) continue;
			// its nearest ancestor that moved
			std::string up = o.parent;
			const std::pair<Mat4, Mat4>* carrier = nullptr;
			for (size_t depth = 0; !up.empty() && depth <= m_document->objects.size(); ++depth)
			{
				const auto found = moved.find(up);
				if (found != moved.end()) { carrier = &found->second; break; }
				const int next = m_document->find(up);
				up = next >= 0 ? m_document->objects[static_cast<size_t>(next)].parent : std::string();
			}
			if (!carrier) continue;
			Mat4 inverse;
			if (!EditorMath::inverse(carrier->first, inverse)) continue;
			const Mat4 placed = mat4_mul(mat4_mul(carrier->second, inverse), level_placement(o, u));
			if (ops) ops->push_back(place_operation(o, placed));
			else if (LevelObjects::Instance* instance = m_objects.find(o.key); instance && instance->mesh) instance->mesh->set_matrix(placed.data());
		}
	}

	namespace
	{
		const char* kClipboardFile = "editor_clipboard.json";   // next to the game: what Ctrl+C took, for any level

		std::string from_game(const std::string& levelDir, const std::string& path)
		{
			if (path.empty()) return path;
			return std::filesystem::absolute(std::filesystem::path(levelDir) / path).lexically_normal().generic_string();
		}

		std::string to_level(const std::string& levelDir, const std::string& absolute)
		{
			if (absolute.empty()) return absolute;
			const std::filesystem::path relative = std::filesystem::path(absolute).lexically_relative(std::filesystem::absolute(levelDir).lexically_normal());
			return relative.empty() ? absolute : relative.generic_string();
		}
	}

	bool LevelEditorState::copy_selection()
	{
		const std::string levelDir = Pakal::LevelDocument::directory_of(m_path);
		const double u = m_document->unitsPerMeter;
		Json objects = Json::array(), prefabs = Json::object();
		Pakal::LevelDocument one;
		one.unitsPerMeter = u;
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index < 0) continue;
			one.objects.push_back(m_document->objects[static_cast<size_t>(index)]);
		}
		if (one.objects.empty()) return false;
		const Json written = one.to_json();
		for (auto object : written.value("objects", Json::array()))
		{
			// places in meters (levels differ in units), the model's path from the game's folder
			for (const char* axis : { "x", "y", "z" }) object[axis] = object.value(axis, 0.0) / u;
			if (object.contains("mesh") && object["mesh"].is_string()) object["mesh"] = from_game(levelDir, object["mesh"].get<std::string>());
			const std::string prefab = object.value("prefab", std::string());
			if (!prefab.empty() && !prefabs.contains(prefab))
			{
				std::string error;
				Json resolved = m_prefabs.prefab(prefab, error);
				if (error.empty())
				{
					if (resolved.contains("mesh") && resolved["mesh"].is_string()) resolved["mesh"] = from_game(levelDir, resolved["mesh"].get<std::string>());
					prefabs[prefab] = resolved;
				}
			}
			objects.push_back(object);
		}
		std::ofstream out(kClipboardFile, std::ios::trunc);
		if (!out) return false;
		out << Json{ { "objects", objects }, { "prefabs", prefabs } }.dump(1) << "\n";
		m_status = "copied " + std::to_string(objects.size()) + " (Ctrl+V pastes, here or in another level)";
		return true;
	}

	bool LevelEditorState::paste(double x, double y, double z)
	{
		std::ifstream in(kClipboardFile);
		const Json clip = in ? Json::parse(in, nullptr, false) : Json();
		if (!clip.is_object() || clip.value("objects", Json::array()).empty())
		{
			m_status = "nothing to paste";
			return false;
		}
		const std::string levelDir = Pakal::LevelDocument::directory_of(m_path);
		const double u = m_document->unitsPerMeter;
		const Json objects = clip["objects"];
		// the group's middle across, and its lowest
		double cx = 0.0, cy = 0.0, low = 1e30;
		for (const auto& o : objects)
		{
			cx += o.value("x", 0.0);
			cy += o.value("y", 0.0);
			low = std::min(low, o.value("z", 0.0));
		}
		cx /= double(objects.size());
		cy /= double(objects.size());
		// the prefabs this level lacks, written into its folder (whole: their bases merged in)
		const Json prefabs = clip.value("prefabs", Json::object());
		for (auto it = prefabs.begin(); it != prefabs.end(); ++it)
		{
			if (m_prefabs.exists(it.key())) continue;
			Json prefab = it.value();
			if (prefab.contains("mesh") && prefab["mesh"].is_string()) prefab["mesh"] = to_level(m_prefabs.folder(), prefab["mesh"].get<std::string>());
			std::string error;
			if (!m_prefabs.save_in(m_prefabs.folder(), it.key(), prefab, error)) { m_status = error; return false; }
		}
		// new keys, each free here; parents among the pasted follow theirs, others are let go
		std::map<std::string, std::string> keys;
		std::set<std::string> taken;
		for (const auto& o : m_document->objects) taken.insert(o.key);
		for (const auto& o : objects)
		{
			const std::string old = o.value("key", std::string());
			std::string base = old.empty() ? std::string("pasted") : old;
			const size_t cut = base.find_last_of('_');
			if (cut != std::string::npos && cut + 1 < base.size() && base.find_first_not_of("0123456789", cut + 1) == std::string::npos) base = base.substr(0, cut);
			std::string fresh;
			for (int n = 1; fresh.empty() || taken.count(fresh); ++n)
			{
				char suffix[16];
				std::snprintf(suffix, sizeof(suffix), "_%02d", n);
				fresh = base + suffix;
			}
			taken.insert(fresh);
			keys[old] = fresh;
		}
		std::vector<Json> ops;
		std::vector<std::string> pasted;
		for (auto o : objects)
		{
			const std::string fresh = keys[o.value("key", std::string())];
			o["key"] = fresh;
			o["x"] = (o.value("x", 0.0) - cx + x) * u;
			o["y"] = (o.value("y", 0.0) - cy + y) * u;
			o["z"] = (o.value("z", 0.0) - low + z) * u;
			o.erase("id");          // a copy is not the game's object of that id
			o.erase("lightmap");    // nor in this level's lightmap
			o.erase("baked");
			if (o.contains("mesh") && o["mesh"].is_string()) o["mesh"] = to_level(levelDir, o["mesh"].get<std::string>());
			const std::string parent = o.value("parent", std::string());
			if (!parent.empty())
			{
				if (keys.count(parent)) o["parent"] = keys[parent];
				else o.erase("parent");
			}
			ops.push_back({ { "op", "insert" }, { "data", o } });
			pasted.push_back(fresh);
		}
		if (!apply_all(ops)) return false;
		m_selection = pasted;
		m_status = "pasted " + std::to_string(pasted.size());
		return true;
	}

	void LevelEditorState::parent_selection(bool unparent)
	{
		if (m_selection.empty() || (!unparent && m_selection.size() < 2)) return;
		std::vector<Json> ops;
		for (size_t i = unparent ? 0 : 1; i < m_selection.size(); ++i)
		{
			ops.push_back({ { "op", "set" }, { "object", m_selection[i] }, { "field", "parent" }, { "value", unparent ? Json() : Json(m_selection.front()) } });
		}
		if (apply_all(ops)) m_status = unparent ? "hung from nothing" : std::to_string(ops.size()) + " hung from " + m_selection.front();
	}

	void LevelEditorState::draw_tree(const std::string& key, const std::map<std::string, std::vector<int>>& children, int index)
	{
		const LevelObject& o = m_document->objects[static_cast<size_t>(index)];
		const auto kids = children.find(key);
		const bool selected = std::find(m_selection.begin(), m_selection.end(), o.key) != m_selection.end();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (selected ? ImGuiTreeNodeFlags_Selected : 0);
		if (kids == children.end()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		const bool dim = m_layer.is_base(o.key) || m_hidden.count(o.key) > 0;
		if (dim) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(index)), flags, "%s%s%s%s", o.key.c_str(), o.prefab.empty() ? "" : "  <",
			o.prefab.empty() ? "" : (o.prefab + ">").c_str(), m_hidden.count(o.key) ? "  (hidden)" : "");
		if (dim) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			const ImGuiIO& io = ImGui::GetIO();
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
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) focus_selection();
		if (kids == children.end() || !open) return;
		for (int child : kids->second) draw_tree(m_document->objects[static_cast<size_t>(child)].key, children, child);
		ImGui::TreePop();
	}

	Json LevelEditorState::place_operation(const LevelObject& object, const Mat4& placed) const
	{
		double position[3], rotation[3], scale = 1.0;
		EditorMath::decompose(placed, m_document->unitsPerMeter, position, rotation, scale);
		const double u = m_document->unitsPerMeter;
		Json operation = { { "op", "place" }, { "object", object.key }, { "x", tidy(position[0] / u) }, { "y", tidy(position[1] / u) }, { "z", tidy(position[2] / u) } };
		if (object.has_basis)
		{
			// its own matrix stays one (a tilt or a scale per axis that angles and one scale cannot say)
			Json basis = Json::array();
			for (int column = 0; column < 3; ++column)
			{
				for (int row = 0; row < 3; ++row) basis.push_back(std::round(double(placed[column * 4 + row]) * 1e6) / 1e6);
			}
			operation["basis"] = basis;
		}
		else
		{
			operation["rot"] = Json::array({ tidy(rotation[0]), tidy(rotation[1]), tidy(rotation[2]) });
			operation["scale"] = tidy(scale);
		}
		return operation;
	}

	void LevelEditorState::draw_grid()
	{
		m_gridLines = 0;
		if (!m_grid || m_lookThrough || m_previewing) return;
		// the snap step near, coarser far: at most some 80 lines each way
		float step = std::max(0.05f, m_snapMove);
		const float reach = std::clamp(m_distance * 1.6f, 8.0f, 800.0f);
		static const float grow[] = { 2.0f, 2.5f, 2.0f };
		for (int i = 0; reach * 2.0f / step > 80.0f; ++i) step *= grow[i % 3];
		const float major = step * 10.0f;
		auto& graphics = m_runtime->graphics();
		const float y = 0.01f;
		const float x0 = std::floor((m_target[0] - reach) / step) * step, x1 = m_target[0] + reach;
		const float z0 = std::floor((m_target[2] - reach) / step) * step, z1 = m_target[2] + reach;
		auto color = [&](float at)
		{
			if (std::fabs(at) < step * 0.01f) return 0xc0ffffffu;
			const float m = std::fabs(std::fmod(at, major));
			return (m < step * 0.01f || major - m < step * 0.01f) ? 0x70ffffffu : 0x30ffffffu;
		};
		for (float x = x0; x <= x1; x += step, ++m_gridLines) graphics.draw_line3d(tmath::vector3df(x, y, z0), tmath::vector3df(x, y, z1), color(x), true);
		for (float z = z0; z <= z1; z += step, ++m_gridLines) graphics.draw_line3d(tmath::vector3df(x0, y, z), tmath::vector3df(x1, y, z), color(z), true);
	}

	void LevelEditorState::turn_selection(double degrees)
	{
		std::vector<Json> ops;
		std::map<std::string, std::pair<Mat4, Mat4>> moved;
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index < 0) continue;
			ops.push_back({ { "op", "turn" }, { "object", key }, { "by", degrees } });
			LevelObject turned = m_document->objects[static_cast<size_t>(index)];
			const Mat4 was = level_placement(turned, m_document->unitsPerMeter);
			turned.rot[2] += degrees;
			moved[key] = { was, level_placement(turned, m_document->unitsPerMeter) };
		}
		carry_children(moved, &ops);   // what hangs from them goes round their pivots
		if (apply_all(ops)) m_status = "turned " + std::to_string(static_cast<int>(degrees)) + " deg";
	}

	bool LevelEditorState::vertex_near(const std::string& key, float x, float y, float maxPixels, float out[3])
	{
		const int index = m_document->find(key);
		const LevelObjects::Instance* instance = m_objects.find(key);
		if (index < 0 || !instance || !instance->mesh || !instance->mesh->get_pick_geometry()) return false;
		const Mat4 placement = level_placement(m_document->objects[static_cast<size_t>(index)], m_document->unitsPerMeter);
		const std::vector<float>& positions = instance->mesh->get_pick_geometry()->positions;
		auto& camera = m_runtime->camera();
		const ImGuiIO& io = ImGui::GetIO();
		float best = maxPixels * maxPixels;
		bool found = false;
		for (size_t i = 0; i + 2 < positions.size(); i += 3)
		{
			const auto p = EditorMath::transform(placement, positions[i], positions[i + 1], positions[i + 2], 1.0f);
			float sx = 0.0f, sy = 0.0f;
			if (!EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), p[0], p[1], p[2], io.DisplaySize.x, io.DisplaySize.y, sx, sy)) continue;
			const float d = (sx - x) * (sx - x) + (sy - y) * (sy - y);
			if (d < best)
			{
				best = d;
				std::copy(p.begin(), p.begin() + 3, out);
				found = true;
			}
		}
		return found;
	}

	std::string LevelEditorState::surface_key(const EditorMath::Ray& ray, float point[3])
	{
		std::set<std::string> skip(m_selection.begin(), m_selection.end());
		float best = 1e30f;
		std::string key;
		for (const auto& instance : m_objects.instances())
		{
			if (!instance.mesh || instance.mesh->hidden() || skip.count(instance.key)) continue;
			float distance = 0.0f, n[3];
			if (instance.mesh->ray_hit(ray.origin.data(), ray.direction.data(), distance, n) && distance < best)
			{
				best = distance;
				key = instance.key;
			}
		}
		if (!key.empty())
		{
			for (int i = 0; i < 3; ++i) point[i] = ray.origin[i] + ray.direction[i] * best;
			return key;
		}
		float normal[3];
		surface_at(ray, point, normal, &skip);
		return key;
	}

	bool LevelEditorState::snap_target(const EditorMath::Ray& ray, float x, float y, float maxPixels, float out[3])
	{
		float point[3];
		const std::string key = surface_key(ray, point);
		std::copy(point, point + 3, out);
		if (!key.empty()) vertex_near(key, x, y, maxPixels, out);
		return true;
	}

	void LevelEditorState::vertex_snap()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const bool held = ImGui::IsKeyDown(ImGuiKey_V) && !io.WantCaptureKeyboard && !io.KeyCtrl;
		auto& graphics = m_runtime->graphics();
		auto cross = [&](const float at[3], uint32_t color)
		{
			const float r = std::max(0.05f, m_distance * 0.006f);
			for (int axis = 0; axis < 3; ++axis)
			{
				float a[3] = { at[0], at[1], at[2] }, b[3] = { at[0], at[1], at[2] };
				a[axis] -= r;
				b[axis] += r;
				graphics.draw_line3d(tmath::vector3df(a[0], a[1], a[2]), tmath::vector3df(b[0], b[1], b[2]), color, false);
			}
		};
		if (!m_vertexDragging)
		{
			if (!held || m_selection.empty() || io.WantCaptureMouse) return;
			// the vertex it would take: shown while V is held
			float from[3];
			if (!vertex_near(m_selection.front(), io.MousePos.x, io.MousePos.y, 1e6f, from)) return;
			cross(from, 0xff20d0ff);
			if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
			std::copy(from, from + 3, m_snapFrom);
			std::copy(from, from + 3, m_snapTo);
			m_snapStart.clear();
			for (const auto& key : m_selection)
			{
				const int index = m_document->find(key);
				if (index >= 0) m_snapStart.push_back({ key, level_placement(m_document->objects[static_cast<size_t>(index)], m_document->unitsPerMeter) });
			}
			m_vertexDragging = true;   // live edits wait meanwhile (the left button is down over the view)
			return;
		}
		// dragging: the vertex follows the pointer onto the nearest vertex (or the surface) under it
		auto& camera = m_runtime->camera();
		EditorMath::Ray ray;
		if (EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, ray))
		{
			snap_target(ray, io.MousePos.x, io.MousePos.y, 24.0f, m_snapTo);
		}
		const float delta[3] = { m_snapTo[0] - m_snapFrom[0], m_snapTo[1] - m_snapFrom[1], m_snapTo[2] - m_snapFrom[2] };
		for (const auto& [key, start] : m_snapStart)
		{
			LevelObjects::Instance* instance = m_objects.find(key);
			if (!instance || !instance->mesh) continue;
			Mat4 moved = start;
			for (int i = 0; i < 3; ++i) moved[12 + i] += delta[i];
			instance->mesh->set_matrix(moved.data());
		}
		cross(m_snapTo, 0xff40ff80);
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
		// let go: one undoable step for the whole selection
		m_vertexDragging = false;
		m_snapStart.clear();
		apply_vertex_snap(delta);
	}

	void LevelEditorState::apply_vertex_snap(const float delta[3])
	{
		const double u = m_document->unitsPerMeter;
		std::vector<Json> ops;
		std::map<std::string, std::pair<Mat4, Mat4>> moved;
		for (const auto& key : m_selection)
		{
			const int index = m_document->find(key);
			if (index < 0) continue;
			const LevelObject& o = m_document->objects[static_cast<size_t>(index)];
			ops.push_back({ { "op", "place" }, { "object", o.key }, { "x", tidy(o.x / u + delta[0]) }, { "y", tidy(o.y / u - delta[2]) }, { "z", tidy(o.z / u + delta[1]) } });
			Mat4 was = level_placement(o, u), now = was;
			for (int i = 0; i < 3; ++i) now[12 + i] += delta[i];
			moved[key] = { was, now };
		}
		carry_children(moved, &ops);
		if (apply_all(ops)) m_status = "snapped to a vertex";
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
			if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false)) select_same();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) parent_selection(io.KeyShift);
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) copy_selection();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
			{
				// around the surface under the pointer (else the point the camera looks at)
				auto& camera = m_runtime->camera();
				EditorMath::Ray ray;
				float point[3] = { m_target[0], 0.0f, m_target[2] }, normal[3];
				if (!io.WantCaptureMouse && EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), io.MousePos.x, io.MousePos.y,
					io.DisplaySize.x, io.DisplaySize.y, ray)) surface_at(ray, point, normal);
				paste(point[0], -point[2], point[1]);
			}
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
				if (ImGui::IsKeyPressed(ImGuiKey_G, false)) m_grid = !m_grid;
				if (ImGui::IsKeyPressed(ImGuiKey_H, false))
				{
					if (io.KeyAlt) show_all();
					else hide_selection(io.KeyShift);
				}
				// [ and ] turn the selection (what is being placed turns in draw_drop)
				if (m_assets.chosen() < 0 && m_assets.dragged() < 0 && m_placing.empty())
				{
					const double step = io.KeyShift ? 90.0 : m_snapTurn;
					if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) turn_selection(-step);
					if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) turn_selection(step);
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			{
				for (const auto& key : std::vector<std::string>(m_selection)) apply({ { "op", "delete" }, { "object", key } });
				m_selection.clear();
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				if (m_lookThrough || m_previewing) stop_preview();
				else if (m_assets.chosen() >= 0 || !m_placing.empty()) { m_assets.choose(-1); m_placing.clear(); }
				else { m_selection.clear(); m_shot = -1; }
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
		// a left drag in the view draws a box: what it holds is selected when it is let go
		if (m_boxPressed)
		{
			if (!m_boxing && (std::fabs(io.MousePos.x - m_boxFrom[0]) > 5.0f || std::fabs(io.MousePos.y - m_boxFrom[1]) > 5.0f) && !m_dragging && !ImGuizmo::IsUsing())
				m_boxing = true;
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				if (m_boxing)
				{
					const std::vector<std::string> inside = keys_in_rect(m_boxFrom[0], m_boxFrom[1], io.MousePos.x, io.MousePos.y);
					if (io.KeyShift || io.KeyCtrl)
					{
						m_selection = m_boxBefore;
						for (const auto& key : inside)
						{
							auto at = std::find(m_selection.begin(), m_selection.end(), key);
							if (io.KeyCtrl && at != m_selection.end()) m_selection.erase(at);
							else if (!io.KeyCtrl && at == m_selection.end()) m_selection.push_back(key);
						}
					}
					else
					{
						m_selection = inside;
					}
					m_status = std::to_string(inside.size()) + " in the box";
				}
				m_boxing = m_boxPressed = false;
			}
		}
		draw_box();
		if (io.WantCaptureMouse || m_gizmoActive || m_vertexDragging || ImGui::IsKeyDown(ImGuiKey_V) || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
		if (m_assets.chosen() < 0 && m_placing.empty())
		{
			m_boxPressed = true;
			m_boxFrom[0] = io.MousePos.x;
			m_boxFrom[1] = io.MousePos.y;
			m_boxBefore = m_selection;
		}
		auto& camera = m_runtime->camera();
		EditorMath::Ray ray;
		if (!EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, ray)) return;
		if (m_assets.chosen() >= 0)
		{
			place_asset(m_assets.chosen(), ray);
			if (!io.KeyShift) m_assets.choose(-1);   // Shift keeps placing
			return;
		}
		if (!m_placing.empty())
		{
			// on the surface under the pointer, as a model from the browser
			double position[3];
			std::array<double, 3> rotation{};
			if (!drop_point(ray, position, rotation)) return;
			const size_t before = m_document->objects.size();
			if (apply({ { "op", "instantiate" }, { "prefab", m_placing }, { "x", tidy(position[0]) }, { "y", tidy(position[1]) }, { "z", tidy(position[2]) },
				{ "rot", Json::array({ tidy(rotation[0]), tidy(rotation[1]), tidy(rotation[2]) }) } }) && m_document->objects.size() > before)
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
			if (!instance.mesh || instance.mesh->hidden()) continue;
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
			m_testLevel = m_path;
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
			// back to the level the test began on, for the scene editing's tests
			if (!m_testLevel.empty()) open_level(m_testLevel);
			m_testStage = 1;
			m_testStageFrame = m_testFrame;
		}
		if (m_testStage > 0) self_test_scene(report);
	}

	// The scene editing's tests, stage by stage (each waits for what it needs): the asset browser's pictures, models dropped
	// on the ground, on another model and on a roof as prefabs made for them
	void LevelEditorState::self_test_scene(const std::function<void(bool, const char*)>& report)
	{
		auto& graphics = m_runtime->graphics();
		const int frames = m_testFrame - m_testStageFrame;
		auto next = [&]() { ++m_testStage; m_testStageFrame = m_testFrame; };
		const double u = m_document ? m_document->unitsPerMeter : 100.0;
		// a ray straight down onto a point of the level (meters)
		auto down_at = [](double x, double y) { EditorMath::Ray ray; ray.origin = { float(x), 200.0f, float(-y) }; ray.direction = { 0.0f, -1.0f, 0.0f }; return ray; };
		switch (m_testStage)
		{
		case 1:
		{
			if (frames < 5) return;
			const auto& entries = m_assets.catalog().entries();
			report(!entries.empty(), "the asset browser has the catalog's models");
			if (entries.empty()) { m_testStage = 100; return; }
			// a small model to drop (clutter or a prop), and the first pictures of the grid
			m_testAsset = 0;
			for (size_t i = 0; i < entries.size(); ++i)
			{
				const auto& e = entries[i];
				const bool small = (e.size[0] > 0.2f && e.size[0] < 2.0f && e.size[1] > 0.2f && e.size[1] < 2.0f && e.size[2] > 0.3f && e.size[2] < 2.0f);
				if (small && std::find(e.tags.begin(), e.tags.end(), "clutter") != e.tags.end()) { m_testAsset = static_cast<int>(i); break; }
			}
			LOG_INFO("[LevelEditor] self test: the model dropped is %s (%s)", entries[static_cast<size_t>(m_testAsset)].id.c_str(), entries[static_cast<size_t>(m_testAsset)].file.c_str());
			m_assets.picture(static_cast<size_t>(m_testAsset));
			for (size_t i = 0; i < entries.size() && i < 12; ++i) m_assets.picture(i);
			next();
			return;
		}
		case 2:
			if (!m_assets.idle() && frames < 1200) return;
			{
				const std::string cached = m_assets.cache_file(static_cast<size_t>(m_testAsset));
				std::error_code code;
				report(m_assets.idle() && m_assets.drawn() + m_assets.read() >= 1, "pictures of the models are drawn by the engine or read from the disk");
				report(bgfx::isValid(m_assets.picture(static_cast<size_t>(m_testAsset))) && std::filesystem::is_regular_file(cached, code),
					"a model's picture is kept on the disk");
				LOG_INFO("[LevelEditor] self test: %zu pictures drawn, %zu read, %zu failed (%s)", m_assets.drawn(), m_assets.read(), m_assets.failed(), cached.c_str());
				graphics.request_screenshot("editor_assets.tga");
			}
			next();
			return;
		case 3:
		{
			if (frames < 5) return;
			// on the ground, west of everything, then on top of it
			double west = 1e30, south = 1e30;
			for (const auto& o : m_document->objects) { west = std::min(west, o.x / u); south = std::min(south, o.y / u); }
			const double x = west - 12.0, y = south - 12.0;
			m_testObjects = m_document->objects.size();
			const bool first = place_asset(m_testAsset, down_at(x, y));
			const LevelObject* placed = first ? &m_document->objects.back() : nullptr;
			report(placed && std::fabs(placed->z) < 0.01 && !placed->prefab.empty(), "a model dropped on the ground stands on it, made from a prefab");
			if (!placed) { m_testStage = 100; return; }
			const std::string prefab = placed->prefab, key = placed->key;
			std::string error;
			const Json made = m_prefabs.prefab(prefab, error);
			report(error.empty() && made.value("catalog", std::string()).find(m_assets.catalog().entries()[static_cast<size_t>(m_testAsset)].id) != std::string::npos,
				"its prefab was written for the model (catalog)");
			const LevelObjects::Instance* instance = m_objects.find(key);
			report(instance && instance->mesh, "the prefab's model loads");
			float low[3] = {}, high[3] = {};
			if (instance && instance->mesh) instance->mesh->get_bounds(low, high);
			const bool second = place_asset(m_testAsset, down_at(placed->x / u + (m_snap ? 0.0 : 0.01), placed->y / u));
			const LevelObject* top = second ? &m_document->objects.back() : nullptr;
			LOG_INFO("[LevelEditor] self test: dropped at %.2f, %.2f: on the ground z %.2f, on it z %.2f (its top %.2f)", x, y, 0.0, top ? top->z / u : -1.0, high[1]);
			report(top && top->z / u > high[1] * 0.5 && top->z / u < high[1] + 0.05, "a model dropped on another stands on it");
			// on a roof: the tallest model's top, under its box's middle
			std::string tallest;
			float tallestHeight = 0.0f, center[3] = {};
			for (const auto& candidate : m_objects.instances())
			{
				if (!candidate.mesh || candidate.key == key) continue;
				float c[3], e[3];
				candidate.mesh->get_world_box(c, e);
				if (c[1] + e[1] > tallestHeight) { tallestHeight = c[1] + e[1]; tallest = candidate.key; std::copy(c, c + 3, center); }
			}
			if (!tallest.empty() && tallestHeight > 3.0f)
			{
				const bool roof = place_asset(m_testAsset, down_at(center[0], -center[2]));
				const LevelObject* onRoof = roof ? &m_document->objects.back() : nullptr;
				LOG_INFO("[LevelEditor] self test: dropped over %s (%.1f m tall): z %.2f", tallest.c_str(), tallestHeight, onRoof ? onRoof->z / u : -1.0);
				report(onRoof && onRoof->z / u > 2.0, "a model dropped over a building lands on its roof");
			}
			// the view on the two
			m_target[0] = static_cast<float>(x);
			m_target[1] = 0.0f;
			m_target[2] = static_cast<float>(-y);
			m_distance = 8.0f;
			m_selection = { key };
			next();
			return;
		}
		case 4:
			if (frames == 10) graphics.request_screenshot("editor_drop.tga");
			if (frames < 20) return;
			// put back as it was: the placements undone, the prefabs the test made removed
			while (m_document->objects.size() > m_testObjects && m_editor->can_undo()) undo(false);
			report(m_document->objects.size() == m_testObjects, "the placements are undone");
			for (const auto& file : m_testPrefabs)
			{
				std::error_code code;
				std::filesystem::remove(file, code);
			}
			m_testPrefabs.clear();
			m_prefabs.refresh();
			save();
			next();
			return;
		case 5:
		{
			// placing aids: the grid, a turn by the step, standing on a slope, and a vertex snap between two models
			report(m_grid && m_gridLines > 0, "the grid is drawn on the ground");
			const float slope[3] = { 0.3f, 0.9f, -0.2f };
			const float length = std::sqrt(slope[0] * slope[0] + slope[1] * slope[1] + slope[2] * slope[2]);
			const float normal[3] = { slope[0] / length, slope[1] / length, slope[2] / length };
			LevelObject standing;
			standing.rot = standing_rotation(normal, 30.0);
			const Mat4 m = level_placement(standing, u);
			const float up = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
			const float off = std::fabs(m[4] / up - normal[0]) + std::fabs(m[5] / up - normal[1]) + std::fabs(m[6] / up - normal[2]);
			report(off < 0.001f && std::fabs(standing.rot[2] - 30.0) < 1e-6, "a model placed on a slope stands on its normal, turned as asked");
			// two models with triangles: the smallest one moves, onto the top of the tallest
			std::string small, tall;
			float smallSize = 1e30f, tallHeight = -1e30f;
			for (const auto& instance : m_objects.instances())
			{
				if (!instance.mesh || !instance.mesh->get_pick_geometry() || (m_layer.layered() && m_layer.is_base(instance.key))) continue;
				float c[3], e[3];
				instance.mesh->get_world_box(c, e);
				if (std::max({ e[0], e[1], e[2] }) < smallSize) { smallSize = std::max({ e[0], e[1], e[2] }); small = instance.key; }
				if (c[1] + e[1] > tallHeight) { tallHeight = c[1] + e[1]; tall = instance.key; }
			}
			if (small.empty() || tall.empty() || small == tall) { report(false, "two models to snap"); m_testStage = 100; return; }
			m_selection = { small };
			const double before = m_document->objects[static_cast<size_t>(m_document->find(small))].rot[2];
			turn_selection(m_snapTurn);
			const double after = m_document->objects[static_cast<size_t>(m_document->find(small))].rot[2];
			report(std::fabs(after - before - m_snapTurn) < 1e-6, "] turns the selection by the turn step");
			undo(false);
			// the camera over the tall model's highest vertex
			const int index = m_document->find(tall);
			const Mat4 placement = level_placement(m_document->objects[static_cast<size_t>(index)], u);
			const auto& positions = m_objects.find(tall)->mesh->get_pick_geometry()->positions;
			float best = -1e30f;
			for (size_t i = 0; i + 2 < positions.size(); i += 3)
			{
				const auto p = EditorMath::transform(placement, positions[i], positions[i + 1], positions[i + 2], 1.0f);
				if (p[1] > best) { best = p[1]; std::copy(p.begin(), p.begin() + 3, m_testOtherVertex); }
			}
			m_testOther = tall;
			std::copy(m_testOtherVertex, m_testOtherVertex + 3, m_target);
			m_distance = 12.0f;
			m_pitch = 1.2f;
			next();
			return;
		}
		case 6:
		{
			if (frames < 3) return;
			const ImGuiIO& io = ImGui::GetIO();
			auto& camera = m_runtime->camera();
			float sx = 0.0f, sy = 0.0f;
			EditorMath::Ray ray;
			const bool seen = EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), m_testOtherVertex[0], m_testOtherVertex[1], m_testOtherVertex[2],
				io.DisplaySize.x, io.DisplaySize.y, sx, sy) && EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), sx, sy, io.DisplaySize.x, io.DisplaySize.y, ray);
			float target[3] = {};
			const bool aimed = seen && snap_target(ray, sx, sy, 24.0f, target);
			const float miss = std::fabs(target[0] - m_testOtherVertex[0]) + std::fabs(target[1] - m_testOtherVertex[1]) + std::fabs(target[2] - m_testOtherVertex[2]);
			LOG_INFO("[LevelEditor] self test: the vertex snap aims at %.2f %.2f %.2f (the vertex %.2f %.2f %.2f of %s)", target[0], target[1], target[2], m_testOtherVertex[0],
				m_testOtherVertex[1], m_testOtherVertex[2], m_testOther.c_str());
			report(aimed && miss < 0.01f, "a vertex snap aims at the vertex drawn nearest the pointer");
			// the selection's vertex nearest a point of the screen, then the move that takes it onto the other
			const std::string small = m_selection.empty() ? std::string() : m_selection.front();
			const int index = m_document->find(small);
			float from[3] = {};
			const LevelObject* object = index >= 0 ? &m_document->objects[static_cast<size_t>(index)] : nullptr;
			float fx = 0.0f, fy = 0.0f;
			if (object)
			{
				const Mat4 placement = level_placement(*object, u);
				const auto& positions = m_objects.find(small)->mesh->get_pick_geometry()->positions;
				const auto p = EditorMath::transform(placement, positions[0], positions[1], positions[2], 1.0f);
				std::copy(p.begin(), p.begin() + 3, m_testVertex);
				EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), p[0], p[1], p[2], io.DisplaySize.x, io.DisplaySize.y, fx, fy);
			}
			const bool taken = object && vertex_near(small, fx, fy, 1e6f, from);
			const float projectedMiss = std::fabs(from[0] - m_testVertex[0]) + std::fabs(from[1] - m_testVertex[1]) + std::fabs(from[2] - m_testVertex[2]);
			report(taken && projectedMiss < 0.05f, "the selection's vertex nearest the pointer is taken");
			const float delta[3] = { m_testOtherVertex[0] - m_testVertex[0], m_testOtherVertex[1] - m_testVertex[1], m_testOtherVertex[2] - m_testVertex[2] };
			apply_vertex_snap(delta);
			const int moved = m_document->find(small);
			float landed[3] = {};
			if (moved >= 0)
			{
				const Mat4 placement = level_placement(m_document->objects[static_cast<size_t>(moved)], u);
				const auto& positions = m_objects.find(small)->mesh->get_pick_geometry()->positions;
				const auto p = EditorMath::transform(placement, positions[0], positions[1], positions[2], 1.0f);
				std::copy(p.begin(), p.begin() + 3, landed);
			}
			const float landedMiss = std::fabs(landed[0] - m_testOtherVertex[0]) + std::fabs(landed[1] - m_testOtherVertex[1]) + std::fabs(landed[2] - m_testOtherVertex[2]);
			report(landedMiss < 0.01f, "the vertex snap puts the selection's vertex on the other's");
			graphics.request_screenshot("editor_snap.tga");
			next();
			return;
		}
		case 7:
		{
			if (frames < 3) return;
			undo(false);
			// selecting: three copies of one prefab west of everything, chosen by "the same prefab", framed, hidden, shown
			// alone and picked by a box
			Json before;
			std::string error;
			const std::string prefab = catalog_prefab(m_assets.catalog().entries()[static_cast<size_t>(m_testAsset)], before, error);
			double west = 1e30, south = 1e30;
			for (const auto& o : m_document->objects) { west = std::min(west, o.x / u); south = std::min(south, o.y / u); }
			m_testObjects = m_document->objects.size();
			Json ops = Json::array();
			for (int i = 0; i < 3; ++i) ops.push_back({ { "op", "instantiate" }, { "prefab", prefab }, { "x", tidy(west - 10.0 - 3.0 * i) }, { "y", tidy(south - 10.0) } });
			report(!prefab.empty() && apply({ { "op", "batch" }, { "ops", ops } }) && m_document->objects.size() == m_testObjects + 3, "three objects of one prefab in one step");
			m_selection = { m_document->objects[m_testObjects + 1].key };
			select_same();
			report(m_selection.size() == 3, "the same prefab selects the three");
			focus_selection();
			next();
			return;
		}
		case 8:
		{
			if (frames < 3) return;
			// framed: each one's middle is drawn in the view
			const ImGuiIO& io = ImGui::GetIO();
			auto& camera = m_runtime->camera();
			int seen = 0;
			float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
			for (const auto& key : m_selection)
			{
				float center[3], extent[3], sx = 0.0f, sy = 0.0f;
				m_objects.find(key)->mesh->get_world_box(center, extent);
				if (EditorMath::to_screen(camera.get_view_matrix(), camera.get_projection_matrix(), center[0], center[1], center[2], io.DisplaySize.x, io.DisplaySize.y, sx, sy)
					&& sx > 0.0f && sy > 0.0f && sx < io.DisplaySize.x && sy < io.DisplaySize.y) ++seen;
				x0 = std::min(x0, sx); y0 = std::min(y0, sy); x1 = std::max(x1, sx); y1 = std::max(y1, sy);
			}
			report(seen == 3, "F frames the whole selection");
			// a box around the three holds them (and what else stands in it)
			const std::vector<std::string> boxed = keys_in_rect(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f);
			int found = 0;
			for (const auto& key : m_selection) found += std::find(boxed.begin(), boxed.end(), key) != boxed.end() ? 1 : 0;
			report(found == 3, "a box drawn around them selects them");
			const std::vector<std::string> narrow = keys_in_rect(x0 - 2.0f, y0 - 2.0f, x0 + 2.0f, y0 + 2.0f);
			report(narrow.size() < boxed.size(), "a smaller box holds fewer");
			graphics.request_screenshot("editor_select.tga");
			// hidden: not drawn and not picked; shown alone: everything else hidden; shown again
			const std::vector<std::string> three = m_selection;
			const std::string first = three.front();
			hide_selection(false);
			const LevelObjects::Instance* hidden = m_objects.find(first);
			report(hidden && hidden->mesh && hidden->mesh->hidden() && m_selection.empty() && keys_in_rect(-1e6f, -1e6f, 1e6f, 1e6f).size() + 3 <= m_objects.instances().size(),
				"H hides the selection from the view, and a box does not take it");
			show_all();
			m_selection = three;
			hide_selection(true);
			size_t visible = 0;
			for (const auto& instance : m_objects.instances()) visible += instance.mesh && !instance.mesh->hidden() ? 1 : 0;
			report(visible == 3, "Shift+H shows the selection alone");
			show_all();
			visible = 0;
			for (const auto& instance : m_objects.instances()) visible += instance.mesh && !instance.mesh->hidden() ? 1 : 0;
			report(visible == m_objects.instances().size(), "Alt+H shows everything again");
			next();
			return;
		}
		case 9:
		{
			if (frames < 3) return;
			// several selected: the gizmo let go moves all of them (one step), and a turn turns each about its own pivot
			m_selection.clear();
			for (size_t i = m_testObjects; i < m_document->objects.size(); ++i) m_selection.push_back(m_document->objects[i].key);
			std::vector<std::array<double, 4>> was;
			for (const auto& key : m_selection)
			{
				const LevelObject& o = m_document->objects[static_cast<size_t>(m_document->find(key))];
				was.push_back({ o.x / u, o.y / u, o.z / u, o.rot[2] });
			}
			m_gizmo = Gizmo::Move;
			m_dragged = level_placement(*selected_object(), u);
			m_dragged[12] += 2.0f;
			m_dragging = true;   // let go on this frame's gizmo
			m_testVertex[0] = static_cast<float>(was[0][0]);
			m_testVertex[1] = static_cast<float>(was[1][0]);
			m_testVertex[2] = static_cast<float>(was[2][0]);
			next();
			return;
		}
		case 10:
		{
			if (frames < 2) return;
			bool moved = m_selection.size() == 3;
			for (size_t i = 0; i < m_selection.size() && i < 3; ++i)
			{
				const LevelObject& o = m_document->objects[static_cast<size_t>(m_document->find(m_selection[i]))];
				moved = moved && std::fabs(o.x / u - (m_testVertex[i] + 2.0)) < 0.01;
			}
			report(moved, "the gizmo moves the whole selection");
			undo(false);
			report(std::fabs(m_document->objects[static_cast<size_t>(m_document->find(m_selection[1]))].x / u - m_testVertex[1]) < 0.01, "in one undo");
			// a quarter turn of the first: each turns a quarter about its own pivot, none moves
			const LevelObject& first = *selected_object();
			const Mat4 before = level_placement(first, u);
			LevelObject turned = first;
			turned.rot[2] += 90.0;
			const Mat4 after = level_placement(turned, u);
			const LevelObject& second = m_document->objects[static_cast<size_t>(m_document->find(m_selection[1]))];
			const Json op = place_operation(second, follow(level_placement(second, u), before, after));
			report(std::fabs(op["x"].get<double>() - second.x / u) < 0.01 && std::fabs(std::fmod(op["rot"][2].get<double>() - second.rot[2] + 360.0, 360.0) - 90.0) < 0.01,
				"a turn of several turns each about its own pivot");
			// a field set on all of them: one batch of overrides, as the inspector makes it
			std::vector<Json> ops;
			for (const auto& key : m_selection) ops.push_back({ { "op", "override" }, { "object", key }, { "path", "/components/GiftBox/hidden" }, { "value", true } });
			auto hidden = [&]()
			{
				int set = 0;
				for (const auto& key : m_selection)
				{
					std::string error;
					set += m_prefabs.resolve(m_document->objects[static_cast<size_t>(m_document->find(key))], error).value("components", Json::object())
						.value("GiftBox", Json::object()).value("hidden", false) ? 1 : 0;
				}
				return set;
			};
			apply_all(ops);
			const int set = hidden();
			graphics.request_screenshot("editor_many.tga");
			undo(false);
			report(set == 3 && hidden() == 0, "a field set on several is set on all of them in one step");
			next();
			return;
		}
		case 11:
			if (frames < 3) return;
			while (m_document->objects.size() > m_testObjects && m_editor->can_undo()) undo(false);
			for (const auto& file : m_testPrefabs)
			{
				std::error_code code;
				std::filesystem::remove(file, code);
			}
			m_testPrefabs.clear();
			m_prefabs.refresh();
			m_selection.clear();
			next();
			return;
		case 12:
		{
			// the engine's tools (what a computer or an agent calls through the game's MCP server), each called as a client's
			// request would be, from another thread: they wait for this one (no socket: the server's own test covers it)
			if (frames < 3) return;
			Pakal::Profiler::get().set_live(true);
			m_testTools = std::thread([this]()
			{
				Pakal::McpServer tools;
				Pakal::add_engine_tools(tools, *m_engine);
				int id = 1;
				auto call = [&](const char* name, const Json& arguments)
				{
					const nlohmann::json request = { { "jsonrpc", "2.0" }, { "id", id++ }, { "method", "tools/call" },
						{ "params", { { "name", name }, { "arguments", nlohmann::json::parse(arguments.dump()) } } } };
					const nlohmann::json answer = nlohmann::json::parse(tools.handle_message(request.dump()), nullptr, false);
					if (!answer.is_object() || !answer.contains("result") || answer["result"].value("isError", true)) return Json();
					return Json::parse(answer["result"]["content"][0].value("text", std::string("null")), nullptr, false);
				};
				Json result;
				result["settings"] = call("get_settings", Json::object());
				result["set"] = call("set_settings", { { "post", { { "vignette", 0.33 } } } });
				result["held"] = call("set_camera", { { "eye", Json::array({ 1.0, 30.0, 40.0 }) }, { "look", Json::array({ 0.0, 0.0, 0.0 }) } });
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				// the renderer puts the camera there before each frame (the editor's own camera is set again after)
				result["heldNow"] = m_runtime->graphics().camera_held();
				result["released"] = call("set_camera", { { "release", true } });
				std::this_thread::sleep_for(std::chrono::milliseconds(2300));   // a second's records, live
				result["stats"] = call("get_stats", Json::object());
				result["profile"] = call("get_profile", { { "after", -1.0 } });
				const Json before = result["settings"].is_object() ? result["settings"].value("post", Json::object()) : Json::object();
				call("set_settings", { { "post", { { "vignette", before.value("vignette", 0.0) } } } });
				m_testReply = result;
			});
			next();
			return;
		}
		case 13:
		{
			if (frames < 240) return;
			if (m_testTools.joinable()) m_testTools.join();
			const Json& r = m_testReply;
			LOG_INFO("[LevelEditor] self test: the engine's tools: quality %s, vignette set %s, held %s, stats %s", r.value("settings", Json::object()).value("quality", std::string("?")).c_str(),
				r.value("set", Json::object()).value("post", Json::object()).value("vignette", Json()).dump().c_str(), r.value("heldNow", false) ? "yes" : "no",
				r.value("stats", Json::object()).value("counters", Json::object()).dump().substr(0, 300).c_str());
			report(r.value("settings", Json::object()).contains("presets") && r["settings"].contains("post"), "get_settings tells the quality, its presets and the post-process");
			report(std::fabs(r.value("set", Json::object()).value("post", Json::object()).value("vignette", 0.0) - 0.33) < 1e-4, "set_settings changes the post-process now");
			report(r.value("held", Json::object()).value("held", false) && r.value("heldNow", false), "set_camera holds the camera at a pose");
			report(r.value("released", Json::object()).value("held", true) == false, "and lets it go");
			const Json counters = r.value("stats", Json::object()).value("counters", Json::object());
			report(r.value("stats", Json::object()).contains("fps") && counters.contains("gpu.draws") && counters.contains("render.scale"),
				"get_stats has the last second's frame times and counters");
			report(!r.value("profile", Json::object()).value("records", Json::array()).empty(), "get_profile gives the records kept");
			// the Device window over the same tools (called in the process: no socket)
			m_testServer = std::make_unique<Pakal::McpServer>();
			Pakal::add_engine_tools(*m_testServer, *m_engine);
			m_device.set_transport([this](const std::string& tool, const Json& arguments, Json& out, std::string& error)
			{
				const nlohmann::json request = { { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "tools/call" },
					{ "params", { { "name", tool }, { "arguments", nlohmann::json::parse(arguments.dump()) } } } };
				const nlohmann::json answer = nlohmann::json::parse(m_testServer->handle_message(request.dump()), nullptr, false);
				if (!answer.is_object() || !answer.contains("result"))
				{
					error = answer.is_object() && answer.contains("error") ? answer["error"].value("message", std::string("error")) : "no answer";
					return false;
				}
				out = Json::parse(answer["result"]["content"][0].value("text", std::string("null")), nullptr, false);
				if (answer["result"].value("isError", false)) { error = out.dump(); return false; }
				return true;
			});
			m_deviceOpen = true;
			m_device.connect();
			m_device.start_recording("editor_cache/selftest_device.ndjson");
			next();
			return;
		}
		case 14:
		{
			if (frames < 200) return;   // some seconds received
			const Json settings = m_device.settings();
			report(m_device.records() >= 2 && settings.is_object() && settings.contains("post"), "the Device window receives the records and the settings");
			graphics.request_screenshot("editor_device.tga");
			m_device.keep(0);
			const double vignette = settings.value("post", Json::object()).value("vignette", 0.0);
			m_testX = vignette;
			m_device.set({ { "post", { { "vignette", 0.6 } } } });
			next();
			return;
		}
		case 15:
			if (frames < 60) return;
			report(std::fabs(m_device.settings().value("post", Json::object()).value("vignette", 0.0) - 0.6) < 1e-4, "a setting changed in it applies to the game");
			m_device.keep(1);
			m_device.apply(0);
			next();
			return;
		case 16:
		{
			if (frames < 60) return;
			report(std::fabs(m_device.settings().value("post", Json::object()).value("vignette", -1.0) - m_testX) < 1e-4 && m_device.snapshot(1).is_object(),
				"A/B: a snapshot put back puts the setting back");
			m_device.stop_recording();
			m_device.disconnect();
			m_device.set_transport({});
			m_deviceOpen = false;
			std::ifstream in("editor_cache/selftest_device.ndjson");
			int lines = 0;
			bool fields = true;
			for (std::string line; std::getline(in, line);)
			{
				const Json record = Json::parse(line, nullptr, false);
				fields = fields && record.is_object() && record.contains("frame_ms") && record.contains("counters");
				++lines;
			}
			report(lines >= 2 && fields, "it records the profiler's records (NDJSON)");
			Pakal::Profiler::get().set_live(false);
			next();
			return;
		}
		case 17:
		{
			// a hierarchy: one object hung from another is carried by the gizmo and by a turn, and let go when it goes
			Json before;
			std::string error;
			const std::string prefab = catalog_prefab(m_assets.catalog().entries()[static_cast<size_t>(m_testAsset)], before, error);
			double west = 1e30, south = 1e30;
			for (const auto& o : m_document->objects) { west = std::min(west, o.x / u); south = std::min(south, o.y / u); }
			m_testObjects = m_document->objects.size();
			apply({ { "op", "batch" }, { "ops", Json::array({ { { "op", "instantiate" }, { "prefab", prefab }, { "x", tidy(west - 10.0) }, { "y", tidy(south - 10.0) } },
				{ { "op", "instantiate" }, { "prefab", prefab }, { "x", tidy(west - 7.0) }, { "y", tidy(south - 10.0) } } }) } });
			if (m_document->objects.size() != m_testObjects + 2) { report(false, "two objects for the hierarchy"); m_testStage = 100; return; }
			const std::string parent = m_document->objects[m_testObjects].key, child = m_document->objects[m_testObjects + 1].key;
			m_selection = { parent, child };
			parent_selection(false);
			report(m_document->objects[m_testObjects + 1].parent == parent, "Ctrl+P hangs the selection from its first object");
			m_testKey = child;
			m_testX = m_document->objects[m_testObjects + 1].x / u;
			m_selection = { parent };
			m_gizmo = Gizmo::Move;
			m_dragged = level_placement(*selected_object(), u);
			m_dragged[12] += 3.0f;
			m_dragging = true;   // let go on this frame's gizmo
			next();
			return;
		}
		case 18:
		{
			if (frames < 2) return;
			const LevelObject* child = &m_document->objects[static_cast<size_t>(m_document->find(m_testKey))];
			report(std::fabs(child->x / u - (m_testX + 3.0)) < 0.01, "the gizmo carries what hangs from what it moves");
			// a quarter turn of the parent takes the child round it
			const LevelObject& parent = *selected_object();
			const double dx = (child->x - parent.x) / u, dy = (child->y - parent.y) / u;
			turn_selection(90.0);
			child = &m_document->objects[static_cast<size_t>(m_document->find(m_testKey))];
			const LevelObject& turned = *selected_object();
			const double nx = (child->x - turned.x) / u, ny = (child->y - turned.y) / u;
			LOG_INFO("[LevelEditor] self test: the child at %.2f, %.2f of its parent, turned: %.2f, %.2f (rot %.1f)", dx, dy, nx, ny, child->rot[2]);
			report(std::fabs(nx + dy) < 0.01 && std::fabs(ny - dx) < 0.01 && std::fabs(std::fmod(child->rot[2] + 360.0, 360.0) - 90.0) < 0.01,
				"a turn takes it round its parent's pivot, turned with it");
			graphics.request_screenshot("editor_tree.tga");
			const std::string key = turned.key;
			apply({ { "op", "delete" }, { "object", key } });
			report(m_document->find(m_testKey) >= 0 && m_document->objects[static_cast<size_t>(m_document->find(m_testKey))].parent.empty(),
				"a deleted parent lets its children go");
			next();
			return;
		}
		case 19:
			if (frames < 3) return;
			while (m_document->objects.size() != m_testObjects && m_editor->can_undo()) undo(false);
			for (const auto& file : m_testPrefabs)
			{
				std::error_code code;
				std::filesystem::remove(file, code);
			}
			m_testPrefabs.clear();
			m_prefabs.refresh();
			m_selection.clear();
			save();
			next();
			return;
		case 20:
		{
			// copy and paste into another level: two objects, one hung from the other, into a new empty level
			Json before;
			std::string error;
			const std::string prefab = catalog_prefab(m_assets.catalog().entries()[static_cast<size_t>(m_testAsset)], before, error);
			m_testObjects = m_document->objects.size();
			apply({ { "op", "batch" }, { "ops", Json::array({ { { "op", "instantiate" }, { "prefab", prefab }, { "x", -40.0 }, { "y", -40.0 } },
				{ { "op", "instantiate" }, { "prefab", prefab }, { "x", -38.0 }, { "y", -40.0 }, { "z", 1.0 } } }) } });
			m_selection = { m_document->objects[m_testObjects].key, m_document->objects[m_testObjects + 1].key };
			parent_selection(false);
			report(copy_selection(), "Ctrl+C copies the selection");
			while (m_document->objects.size() > m_testObjects && m_editor->can_undo()) undo(false);
			for (const auto& file : m_testPrefabs)
			{
				std::error_code code;
				std::filesystem::remove(file, code);
			}
			m_testPrefabs.clear();
			m_prefabs.refresh();
			save();
			// another level, empty, beside the test's
			const std::filesystem::path folder = std::filesystem::path(m_testLevel).parent_path();
			m_testLayer = ((folder.parent_path() / (folder.filename().string() + "_selftest_paste")) / "level.json").generic_string();
			std::error_code code;
			std::filesystem::remove_all(std::filesystem::path(m_testLayer).parent_path(), code);
			std::filesystem::create_directories(std::filesystem::path(m_testLayer).parent_path(), code);
			{
				std::ofstream out(m_testLayer);
				out << R"({"format": "pakal-level", "version": 2, "name": "paste", "units_per_meter": 100, "spawn": [0, 0], "objects": []})" << "\n";
			}
			report(open_level(m_testLayer), "an empty level opens");
			next();
			return;
		}
		case 21:
		{
			if (frames < 5) return;
			report(paste(5.0, 5.0, 0.0) && m_document->objects.size() == 2, "Ctrl+V pastes them into it");
			if (m_document->objects.size() == 2)
			{
				const LevelObject& first = m_document->objects[0];
				const LevelObject& second = m_document->objects[1];
				const LevelObjects::Instance* instance = m_objects.find(second.key);
				LOG_INFO("[LevelEditor] self test: pasted %s at %.2f %.2f %.2f and %s (parent %s) at %.2f %.2f %.2f", first.key.c_str(), first.x / 100.0, first.y / 100.0,
					first.z / 100.0, second.key.c_str(), second.parent.c_str(), second.x / 100.0, second.y / 100.0, second.z / 100.0);
				report(second.parent == first.key && std::fabs((second.x - first.x) / 100.0 - 2.0) < 0.01 && std::fabs(second.z / 100.0 - 1.0) < 0.01,
					"with their places to each other and the one hung from the other");
				report(m_prefabs.exists(first.prefab) && instance && instance->mesh, "their prefab is written into the level and its model loads");
			}
			// back to the test's level, the other one removed
			const std::string other = m_testLayer;
			open_level(m_testLevel);
			std::error_code code;
			std::filesystem::remove_all(std::filesystem::path(other).parent_path(), code);
			m_testStage = 100;
			return;
		}
		case 100:
			LOG_INFO("[LevelEditor] self test done: %d failed", m_testFailures);
			m_testStage = 0;
			if (std::getenv("PAKAL_EDITOR_SELFTEST_EXIT")) m_testQuit = true;
			return;
		default:
			return;
		}
	}

	// --- the asset browser and placing ---

	namespace
	{
		const char* kSettingsFile = "editor_settings.json";   // next to the game: the editor's own choices
	}

	void LevelEditorState::read_settings()
	{
		std::ifstream in(kSettingsFile);
		if (!in) return;
		const Json settings = Json::parse(in, nullptr, false);
		if (!settings.is_object()) return;
		m_prefabHome = settings.value("prefabHome", std::string("level")) == "library" ? PrefabHome::Library : PrefabHome::Level;
		std::snprintf(m_library, sizeof(m_library), "%s", settings.value("prefabLibrary", std::string()).c_str());
		m_alignToSurface = settings.value("alignToSurface", false);
	}

	void LevelEditorState::write_settings()
	{
		const Json settings = { { "prefabHome", m_prefabHome == PrefabHome::Library ? "library" : "level" }, { "prefabLibrary", std::string(m_library) },
			{ "alignToSurface", m_alignToSurface } };
		std::ofstream out(kSettingsFile, std::ios::trunc);
		if (out) out << settings.dump(1) << "\n";
	}

	std::string LevelEditorState::library_folder() const
	{
		// by default a "prefabs" folder beside the level's own: shared by the levels of one game
		if (m_library[0]) return m_library;
		return (std::filesystem::path(m_path).parent_path().parent_path() / "prefabs").generic_string();
	}

	void LevelEditorState::draw_assets()
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(300.0f, io.DisplaySize.y - 300.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(std::max(300.0f, io.DisplaySize.x - 660.0f), 300.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Assets");
		if (ImGui::SmallButton("Placing...")) ImGui::OpenPopup("##placing");
		if (ImGui::BeginPopup("##placing"))
		{
			bool changed = false;
			ImGui::TextDisabled("A model placed from here is made from a prefab of its own, written the first time:");
			changed |= ImGui::RadioButton("in the level's prefabs folder", m_prefabHome == PrefabHome::Level) && (m_prefabHome = PrefabHome::Level, true);
			changed |= ImGui::RadioButton("in a shared library the level names", m_prefabHome == PrefabHome::Library) && (m_prefabHome = PrefabHome::Library, true);
			if (m_prefabHome == PrefabHome::Library)
			{
				ImGui::SetNextItemWidth(320.0f);
				changed |= ImGui::InputTextWithHint("##library", library_folder().c_str(), m_library, sizeof(m_library));
				ImGui::TextDisabled("%s", library_folder().c_str());
			}
			ImGui::Separator();
			changed |= ImGui::Checkbox("Stand on the surface's slope", &m_alignToSurface);
			if (changed) write_settings();
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s, turn %.0f deg", m_prefabHome == PrefabHome::Level ? "prefabs in the level's folder" : "prefabs in the shared library", m_placeYaw);
		m_assets.draw();
		ImGui::End();
	}

	void LevelEditorState::hide_ghost()
	{
		m_ghost.reset();
		m_ghostFile.clear();
	}

	std::array<double, 3> LevelEditorState::standing_rotation(const float normal[3], double yaw)
	{
		// the normal in level axes (x east, y north, z up), then seen from the model's own turn about z
		const double nx = normal[0], ny = -normal[2], nz = normal[1];
		const double turn = yaw * 3.14159265358979 / 180.0, c = std::cos(turn), s = std::sin(turn);
		const double x = c * nx + s * ny, y = -s * nx + c * ny;
		// Rz(yaw) * Ry(b) * Rx(a) takes up (0, 0, 1) to (sin b cos a, -sin a, cos b cos a) before the turn
		const double toDegrees = 180.0 / 3.14159265358979;
		const double a = std::asin(std::clamp(-y, -1.0, 1.0)), b = std::atan2(x, nz);
		return { tidy(a * toDegrees), tidy(b * toDegrees), tidy(yaw) };
	}

	bool LevelEditorState::surface_at(const EditorMath::Ray& ray, float point[3], float normal[3], const std::set<std::string>* skip)
	{
		float best = 1e30f;
		for (const auto& instance : m_objects.instances())
		{
			if (!instance.mesh || instance.mesh->hidden() || (skip && skip->count(instance.key))) continue;
			float distance = 0.0f, n[3];
			if (instance.mesh->ray_hit(ray.origin.data(), ray.direction.data(), distance, n) && distance < best)
			{
				best = distance;
				for (int i = 0; i < 3; ++i)
				{
					point[i] = ray.origin[i] + ray.direction[i] * distance;
					normal[i] = n[i];
				}
			}
		}
		if (best < 1e30f) return true;
		float x = 0.0f, z = 0.0f;
		if (!EditorMath::hit_floor(ray, 0.0f, x, z)) return false;
		point[0] = x;
		point[1] = 0.0f;
		point[2] = z;
		normal[0] = normal[2] = 0.0f;
		normal[1] = 1.0f;
		return true;
	}

	bool LevelEditorState::drop_point(const EditorMath::Ray& ray, double position[3], std::array<double, 3>& rotation)
	{
		float point[3], normal[3];
		if (!surface_at(ray, point, normal)) return false;
		// on the grid across (the height is the surface's)
		const float grid = m_snap ? m_snapMove : 0.0f;
		auto snapped = [grid](float v) { return grid > 0.0f ? std::round(v / grid) * grid : v; };
		position[0] = snapped(point[0]);
		position[1] = snapped(-point[2]);
		position[2] = point[1];
		rotation = m_alignToSurface ? standing_rotation(normal, m_placeYaw) : std::array<double, 3>{ 0.0, 0.0, m_placeYaw };
		return true;
	}

	std::string LevelEditorState::catalog_prefab(const AssetEntry& entry, Json& before, std::string& error)
	{
		// its own name, unless a prefab of that name comes from elsewhere
		std::string name = AssetCatalog::prefab_name(entry, std::string());
		if (m_prefabs.exists(name))
		{
			const Json& held = m_prefabs.raw(name);
			const std::string from = held.is_object() ? held.value("catalog", std::string("(made by hand)")) : std::string("(not valid)");
			name = AssetCatalog::prefab_name(entry, from);
			if (name == AssetCatalog::prefab_name(entry, std::string())) return name;   // it is this model's
		}
		const bool library = m_prefabHome == PrefabHome::Library;
		if (library)
		{
			// the level names the library (once): its prefabs are found from it, here and in the game
			const std::string folder = library_folder();
			const std::string relative = Pakal::PrefabLibrary::relative_path(
				std::filesystem::absolute(folder).lexically_normal().generic_string(),
				std::filesystem::absolute(std::filesystem::path(m_path).parent_path()).lexically_normal().generic_string());
			const Json listed = m_document->meta.value(Pakal::PrefabLibrary::kLibrariesKey, Json::array());
			if (std::find(listed.begin(), listed.end(), Json(relative)) == listed.end()) before = { { "op", "prefab-library" }, { "folder", relative } };
			std::vector<std::string> libraries = m_prefabs.libraries();
			if (std::find(libraries.begin(), libraries.end(), folder + "/") == libraries.end()) libraries.push_back(folder);
			m_prefabs.set_libraries(libraries);   // until the document names it (open again after the operation)
		}
		if (m_prefabs.exists(name)) return name;
		const std::string folder = library ? library_folder() : m_prefabs.folder();
		if (!m_prefabs.save_in(folder, name, AssetCatalog::prefab_for(entry, folder), error)) return std::string();
		LOG_INFO("[LevelEditor] made the prefab %s for %s (%s)", name.c_str(), entry.file.c_str(), folder.c_str());
		if (m_selfTest) m_testPrefabs.push_back((std::filesystem::path(folder) / (name + Pakal::PrefabLibrary::kExtension)).generic_string());
		return name;
	}

	bool LevelEditorState::place_asset(int index, const EditorMath::Ray& ray)
	{
		const auto& entries = m_assets.catalog().entries();
		if (index < 0 || index >= static_cast<int>(entries.size()) || !m_document) return false;
		const AssetEntry& entry = entries[static_cast<size_t>(index)];
		double position[3];
		std::array<double, 3> rotation{};
		if (!drop_point(ray, position, rotation)) return false;
		Json before;
		std::string error;
		const std::string name = catalog_prefab(entry, before, error);
		if (name.empty())
		{
			m_status = "cannot make a prefab for " + entry.id + ": " + error;
			return false;
		}
		const Json instantiate = { { "op", "instantiate" }, { "prefab", name }, { "x", tidy(position[0]) }, { "y", tidy(position[1]) }, { "z", tidy(position[2]) },
			{ "rot", Json::array({ rotation[0], rotation[1], rotation[2] }) } };
		const size_t count = m_document->objects.size();
		const bool done = apply(before.is_object() ? Json{ { "op", "batch" }, { "ops", Json::array({ before, instantiate }) } } : instantiate);
		if (m_prefabHome == PrefabHome::Library) m_prefabs.open(m_path, *m_document);
		if (!done || m_document->objects.size() <= count) return false;
		m_selection = { m_document->objects.back().key };
		m_status = "placed " + entry.id + " (prefab " + name + ")";
		return true;
	}

	void LevelEditorState::draw_drop()
	{
		const int dragged = m_assets.dragged();
		const int index = dragged >= 0 ? dragged : m_assets.chosen();
		const ImGuiIO& io = ImGui::GetIO();
		// over the view: no window under the pointer (a drag from the browser keeps the mouse ImGui's)
		const bool overView = !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
		const auto& entries = m_assets.catalog().entries();
		EditorMath::Ray ray;
		auto& camera = m_runtime->camera();
		if (index < 0 || index >= static_cast<int>(entries.size()) || !overView
			|| !EditorMath::screen_ray(camera.get_view_matrix(), camera.get_projection_matrix(), io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, ray))
		{
			hide_ghost();
			if (dragged >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_assets.end_drag();   // dropped on a window: nothing
			return;
		}
		// [ and ] turn what is placed by the turn step (Shift: a quarter turn)
		if (!io.WantCaptureKeyboard || dragged >= 0)
		{
			const double step = io.KeyShift ? 90.0 : m_snapTurn;
			if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) m_placeYaw = std::fmod(m_placeYaw - step + 360.0, 360.0);
			if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) m_placeYaw = std::fmod(m_placeYaw + step, 360.0);
		}
		if (dragged >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			hide_ghost();
			place_asset(dragged, ray);
			m_assets.end_drag();
			return;
		}
		// the model where it would land
		const AssetEntry& entry = entries[static_cast<size_t>(index)];
		if (m_ghostFile != entry.file)
		{
			bool failed = false;
			m_ghost = m_runtime->model_if_ready(entry.file, failed);
			if (m_ghost || failed) m_ghostFile = entry.file;
			if (m_ghost) m_ghost->set_color(0.7f, 0.85f, 1.2f);
		}
		double position[3];
		std::array<double, 3> rotation{};
		if (!m_ghost || !drop_point(ray, position, rotation)) return;
		Pakal::LevelObject at;
		const double u = m_document->unitsPerMeter;
		at.x = position[0] * u;
		at.y = position[1] * u;
		at.z = position[2] * u;
		at.rot = rotation;
		m_ghost->set_matrix(level_placement(at, u).data());
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
		for (const auto& instance : m_objects.instances())
		{
			if (!instance.mesh) continue;
			instance.mesh->set_visible(!m_hidden.count(instance.key));   // what the view hides (H)
			if (!m_layer.layered() || !m_layer.is_base(instance.key)) continue;
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
