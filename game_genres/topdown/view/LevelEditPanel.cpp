///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditPanel.cpp
// Purpose: Edits the running level - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/LevelEditPanel.h"
#include "topdown/view/RenderSpace.h"

#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <dear-imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Pakal::TopDown
{
	namespace
	{
		const uint32_t kSelected = 0xff30e0ffu;   // ABGR: yellow
		const uint32_t kAxisX = 0xff3040ffu;      // red: east
		const uint32_t kAxisY = 0xff40ff40u;      // green: north
		const uint32_t kZone = 0xc0ffd040u;       // light blue

		void ring(Pakal::BgfxGraphicsSystem& graphics, const b2Vec2& at, float radius, uint32_t color)
		{
			const int sides = 24;
			for (int i = 0; i < sides; ++i)
			{
				const float a = 6.2831853f * static_cast<float>(i) / sides, b = 6.2831853f * static_cast<float>(i + 1) / sides;
				graphics.draw_line3d(floor_point(at + radius * b2Vec2(std::cos(a), std::sin(a)), 0.05f),
					floor_point(at + radius * b2Vec2(std::cos(b), std::sin(b)), 0.05f), color, false);
			}
		}
	}

	LevelEditPanel::LevelEditPanel(std::string path, std::unique_ptr<Pakal::LevelDocument> document, Place place) :
		m_path(std::move(path)),
		m_document(std::move(document)),
		m_editor(*m_document),
		m_place(std::move(place)),
		m_meshes(m_document->objects.size(), nullptr)
	{
		const std::filesystem::path file(m_path);
		std::string error;
		m_editor.load_history((file.parent_path() / (file.stem().string() + ".history")).string(), error);
		// objects without a readable key get one in this session (saved with the first change)
		m_document->assign_keys();
	}

	void LevelEditPanel::set_mesh(size_t index, Pakal::MeshComponent3D_Bgfx* mesh)
	{
		if (index < m_meshes.size()) m_meshes[index] = mesh;
	}

	bool LevelEditPanel::take_reload()
	{
		const bool reload = m_reload;
		m_reload = false;
		return reload;
	}

	void LevelEditPanel::place(int index)
	{
		if (index < 0 || static_cast<size_t>(index) >= m_meshes.size() || !m_meshes[static_cast<size_t>(index)] || !m_place) return;
		m_place(*m_meshes[static_cast<size_t>(index)], m_document->objects[static_cast<size_t>(index)]);
	}

	void LevelEditPanel::place_all()
	{
		if (m_document->objects.size() != m_meshes.size())
		{
			m_reload = save();   // objects came or went: the level is built again
			return;
		}
		for (size_t i = 0; i < m_meshes.size(); ++i) place(static_cast<int>(i));
	}

	bool LevelEditPanel::apply(const Pakal::Json& operation)
	{
		const std::string error = m_editor.apply(operation);
		m_status = error.empty() ? operation.value("op", std::string()) + " done" : error;
		if (!error.empty()) return false;
		m_dirty = true;
		place_all();
		return true;
	}

	bool LevelEditPanel::save()
	{
		std::string error;
		const std::filesystem::path file(m_path);
		if (!m_document->save(m_path, error) || !m_editor.save_history((file.parent_path() / (file.stem().string() + ".history")).string(), error))
		{
			m_status = error;
			return false;
		}
		std::error_code code;
		m_savedAt = std::filesystem::last_write_time(m_path, code);
		m_dirty = false;
		m_status = "saved";
		return true;
	}

	void LevelEditPanel::update(bool mouseDown, bool mouseFree, bool aimed, const b2Vec2& mouseFloor, Pakal::BgfxGraphicsSystem& graphics)
	{
		const bool pressed = mouseDown && !m_wasDown;
		m_wasDown = mouseDown;
		const int selected = index();
		if (m_editing && aimed && mouseFree)
		{
			if (pressed)
			{
				// on the selected one: grab it; elsewhere: pick the nearest object
				const bool onSelected = selected >= 0
					&& (b2Vec2(meters(m_document->objects[static_cast<size_t>(selected)].x), meters(m_document->objects[static_cast<size_t>(selected)].y)) - mouseFloor).Length() < 1.0f;
				if (onSelected)
				{
					const LevelObject& o = m_document->objects[static_cast<size_t>(selected)];
					m_dragging = true;
					m_before = o;
					m_grab = b2Vec2(meters(o.x), meters(o.y)) - mouseFloor;
				}
				else
				{
					int nearest = -1;
					float best = 2.5f;
					for (size_t i = 0; i < m_document->objects.size(); ++i)
					{
						const LevelObject& o = m_document->objects[i];
						const float distance = (b2Vec2(meters(o.x), meters(o.y)) - mouseFloor).Length();
						if (distance < best)
						{
							best = distance;
							nearest = static_cast<int>(i);
						}
					}
					if (nearest >= 0) m_selected = m_document->objects[static_cast<size_t>(nearest)].key;
				}
			}
			else if (m_dragging && mouseDown && selected >= 0)
			{
				// the model follows the mouse; the document takes one move when it is let go
				LevelObject& o = m_document->objects[static_cast<size_t>(selected)];
				const b2Vec2 at = mouseFloor + m_grab;
				o.x = at.x * m_document->unitsPerMeter;
				o.y = at.y * m_document->unitsPerMeter;
				place(selected);
			}
		}
		if (m_dragging && !mouseDown)
		{
			m_dragging = false;
			if (selected >= 0)
			{
				LevelObject& o = m_document->objects[static_cast<size_t>(selected)];
				const b2Vec2 at(meters(o.x), meters(o.y));
				o = m_before;
				apply({ { "op", "move" }, { "object", o.key }, { "x", at.x }, { "y", at.y } });
			}
		}

		// gizmos: the selected object's ring and axes, and the zones
		if (!m_editing) return;
		const int shown = index();
		if (shown >= 0)
		{
			const LevelObject& o = m_document->objects[static_cast<size_t>(shown)];
			const b2Vec2 at(meters(o.x), meters(o.y));
			ring(graphics, at, 0.8f, kSelected);
			graphics.draw_line3d(floor_point(at, 0.05f), floor_point(at + b2Vec2(1.5f, 0.0f), 0.05f), kAxisX, false);
			graphics.draw_line3d(floor_point(at, 0.05f), floor_point(at + b2Vec2(0.0f, 1.5f), 0.05f), kAxisY, false);
			graphics.draw_line3d(floor_point(at, 0.0f), floor_point(at, 2.0f), kSelected, false);
		}
		if (m_showZones)
		{
			for (const auto& zone : m_document->zones)
			{
				const b2Vec2 a(meters(zone.x0), meters(zone.y0)), b(meters(zone.x1), meters(zone.y0)), c(meters(zone.x1), meters(zone.y1)), d(meters(zone.x0), meters(zone.y1));
				graphics.draw_line3d(floor_point(a, 0.08f), floor_point(b, 0.08f), kZone, false);
				graphics.draw_line3d(floor_point(b, 0.08f), floor_point(c, 0.08f), kZone, false);
				graphics.draw_line3d(floor_point(c, 0.08f), floor_point(d, 0.08f), kZone, false);
				graphics.draw_line3d(floor_point(d, 0.08f), floor_point(a, 0.08f), kZone, false);
			}
		}
	}

	void LevelEditPanel::draw()
	{
		ImGui::Checkbox("Editing (pauses; click picks, drag moves)", &m_editing);
		ImGui::Checkbox("Live reload (pakal-level, agents)", &m_liveReload);
		ImGui::SameLine();
		ImGui::Checkbox("Zones", &m_showZones);
		ImGui::Text("%zu objects, %zu zones%s", m_document->objects.size(), m_document->zones.size(), m_dirty ? "  (not saved)" : "");
		ImGui::BeginDisabled(!m_editor.can_undo());
		if (ImGui::Button("Undo"))
		{
			m_status = "undone " + m_editor.undo();
			m_dirty = true;
			place_all();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_editor.can_redo());
		if (ImGui::Button("Redo"))
		{
			m_status = "redone " + m_editor.redo();
			m_dirty = true;
			place_all();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Save")) save();
		ImGui::SameLine();
		if (ImGui::Button("Save and reload")) m_reload = save();
		if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());

		// the objects, filtered by key, name or model
		ImGui::InputText("Filter", m_filter, sizeof(m_filter));
		if (ImGui::BeginListBox("##objects", ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f)))
		{
			int listed = 0;
			for (const auto& o : m_document->objects)
			{
				if (m_filter[0] && o.key.find(m_filter) == std::string::npos && o.name.find(m_filter) == std::string::npos && o.mesh.find(m_filter) == std::string::npos)
				{
					continue;
				}
				if (++listed > 300) break;
				if (ImGui::Selectable(o.key.c_str(), o.key == m_selected)) m_selected = o.key;
			}
			ImGui::EndListBox();
		}

		const int selected = index();
		if (selected < 0) return;
		LevelObject& o = m_document->objects[static_cast<size_t>(selected)];
		ImGui::Text("%s  (%s)", o.key.c_str(), o.name.c_str());
		ImGui::TextWrapped("%s", o.mesh.c_str());
		if (o.id >= 0) ImGui::Text("game id %d", o.id);
		// a field being dragged moves the model; letting it go makes one undoable operation
		float at[3] = { meters(o.x), meters(o.y), meters(o.z) };
		float yaw = static_cast<float>(o.rot[2]);
		float scale = static_cast<float>(o.scale);
		auto begin_edit = [&]() { if (ImGui::IsItemActivated()) m_before = o; };
		bool moved = ImGui::DragFloat3("Position (m)", at, 0.05f);
		begin_edit();
		const bool movedDone = ImGui::IsItemDeactivatedAfterEdit();
		bool turned = ImGui::DragFloat("Yaw", &yaw, 1.0f, -360.0f, 360.0f, "%.1f deg");
		begin_edit();
		const bool turnedDone = ImGui::IsItemDeactivatedAfterEdit();
		bool scaled = ImGui::DragFloat("Scale", &scale, 0.01f, 0.05f, 20.0f, "%.2f");
		begin_edit();
		const bool scaledDone = ImGui::IsItemDeactivatedAfterEdit();
		if (moved || turned || scaled)
		{
			o.x = at[0] * m_document->unitsPerMeter;
			o.y = at[1] * m_document->unitsPerMeter;
			o.z = at[2] * m_document->unitsPerMeter;
			o.rot[2] = yaw;
			o.scale = std::max(0.05f, scale);
			place(selected);
		}
		if (movedDone || turnedDone || scaledDone)
		{
			const std::string key = o.key;
			o = m_before;
			if (movedDone) apply({ { "op", "move" }, { "object", key }, { "x", at[0] }, { "y", at[1] }, { "z", at[2] } });
			if (turnedDone) apply({ { "op", "turn" }, { "object", key }, { "yaw", yaw } });
			if (scaledDone) apply({ { "op", "scale" }, { "object", key }, { "scale", std::max(0.05f, scale) } });
		}
		char role[48], zone[48];
		std::snprintf(role, sizeof(role), "%s", o.role.c_str());
		std::snprintf(zone, sizeof(zone), "%s", o.zone.c_str());
		if (ImGui::InputText("Role", role, sizeof(role), ImGuiInputTextFlags_EnterReturnsTrue)) apply({ { "op", "set" }, { "object", o.key }, { "field", "role" }, { "value", role } });
		if (ImGui::InputText("Zone", zone, sizeof(zone), ImGuiInputTextFlags_EnterReturnsTrue)) apply({ { "op", "set" }, { "object", o.key }, { "field", "zone" }, { "value", zone } });
		if (ImGui::Button("Copy 1 m east"))
		{
			const std::string key = o.key;
			apply({ { "op", "copy" }, { "object", key }, { "x", meters(o.x) + 1.0f }, { "y", meters(o.y) } });
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete"))
		{
			const std::string key = o.key;
			if (apply({ { "op", "delete" }, { "object", key } })) m_selected.clear();
		}
	}
}
