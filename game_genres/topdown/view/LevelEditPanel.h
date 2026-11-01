///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditPanel.h
// Purpose: Edits the running level: a Dear ImGui panel (in the debug UI) over the level's document and
//          its LevelEditor (the same operations as pakal-level, so undo and the history are shared),
//          and gizmos on the floor: the selected object's ring and axes, the zones. While editing, a
//          click on the floor picks the nearest object and dragging it moves it; the panel's fields
//          move, turn and scale it. Each change places the object's model at once; adding or removing
//          objects (and the collision) takes a reload. The game owns it and keeps both of its calls
//          under one lock (draw on the graphics thread, update on the game's).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"
#include "LevelEditor.h"

#include <box2d/box2d.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class LevelEditPanel
	{
	public:
		// Places an object's model where the document says (the game's placement)
		using Place = std::function<void(Pakal::MeshComponent3D_Bgfx& mesh, const Pakal::LevelObject& object)>;

		LevelEditPanel(std::string path, std::unique_ptr<Pakal::LevelDocument> document, Place place);

		// The model of the document's object 'index' (null when it has none)
		void set_mesh(size_t index, Pakal::MeshComponent3D_Bgfx* mesh);

		// The panel's contents (inside a debug UI window)
		void draw();
		// Each frame: picking and dragging with the mouse on the floor while editing, and the gizmos. 'mouseFree': the click
		// is not over a panel.
		void update(bool mouseDown, bool mouseFree, bool aimed, const b2Vec2& mouseFloor, Pakal::BgfxGraphicsSystem& graphics);

		bool editing() const { return m_editing; }
		void set_editing(bool editing) { m_editing = editing; }
		const std::string& selected() const { return m_selected; }
		void select(const std::string& key) { m_selected = key; }
		// The level file changes from outside (pakal-level, an agent): the game reloads it
		bool live_reload() const { return m_liveReload; }
		void set_live_reload(bool on) { m_liveReload = on; }
		// The panel asked for a reload (after adding or removing objects, or its button); true once
		bool take_reload();
		// When the panel last saved the file (a reload watcher ignores its own saves)
		std::filesystem::file_time_type saved_at() const { return m_savedAt; }
		const std::string& path() const { return m_path; }
		// The document as the panel has it, and one read again from the file (the level changed outside and the game
		// brought its objects in line without building the level again): the panel edits it from now on
		const Pakal::LevelDocument& document() const { return *m_document; }
		void replace_document(const Pakal::LevelDocument& document);

	private:
		bool apply(const Pakal::Json& operation);
		bool save();
		void place_all();
		void place(int index);
		int index() const { return m_document->find(m_selected); }
		float meters(double units) const { return static_cast<float>(units / m_document->unitsPerMeter); }

		std::string m_path;
		std::unique_ptr<Pakal::LevelDocument> m_document;
		Pakal::LevelEditor m_editor;
		Place m_place;
		std::vector<Pakal::MeshComponent3D_Bgfx*> m_meshes;
		std::string m_selected;
		std::string m_status;
		char m_filter[64] = {};
		bool m_editing = false;
		bool m_liveReload = false;
		bool m_showZones = true;
		bool m_dirty = false;            // changed since the last save
		bool m_reload = false;
		// a drag of the selected object (the mouse or a field): where it started, so it is one undoable move
		bool m_dragging = false;
		bool m_wasDown = false;
		b2Vec2 m_grab = b2Vec2(0.0f, 0.0f);   // from the mouse to the object's center
		Pakal::LevelObject m_before;
		std::filesystem::file_time_type m_savedAt{};
	};
}
