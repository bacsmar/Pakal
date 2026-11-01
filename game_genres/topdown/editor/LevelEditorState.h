///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditorState.h
// Purpose: The level editor: a game state that shows a level document's objects (LevelObjects, no
//          simulation) under a free camera, with Dear ImGui windows over it: the objects (by zone,
//          filtered), the selected one's inspector (its transform, its prefab, its components from the
//          ComponentSchema, what it overrides), the prefabs to place, and a gizmo (ImGuizmo) to move,
//          turn and scale it. Every change is a LevelEditor operation (undoable, the history shared with
//          pakal-level); a file changed outside (pakal-level, an agent, a prefab edited by hand) comes in
//          at once. Play saves and runs the game's own state for the level on top; Stop comes back.
//
//          A layer over a base (a level that "extends" another, LevelLayers) is edited composed with its base: the
//          base's objects are dimmed and, while the base is locked, not picked by a click. Saving writes only the layer:
//          new objects as its own, a base object moved or changed as "adjust", one deleted as "hide" (LevelLayerEditing).
//
//          Mouse: left picks (Shift adds), right drags turn the camera, middle drags pan it, the wheel
//          zooms; W E R: move, turn, scale; F: look at the selection; Delete; Ctrl+Z / Ctrl+Y; Ctrl+S;
//          Ctrl+D duplicates; Ctrl+O opens another level. Started without a level, it opens on the level chooser.
//
//          The Camera tab edits the level's intro camera ("camera": {"intro": ...}, LevelCameraPath): its shots are drawn in
//          the view, a click on one's eye or look point takes it and the gizmo moves it; a shot can be looked through and
//          the whole path played, ending on the play camera at the spawn when it ends on play.
//
//          While it is open, pakal-level (or an agent) works in it with the person (LiveEditing): its operations are
//          done in the open document, undoable with the person's own and marked as the agent's, after the person lets
//          go of what they drag; they are saved at once when nothing else was unsaved.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "LevelDocument.h"
#include "LevelEditor.h"
#include "LevelLayerEditing.h"
#include "LiveEditing.h"
#include "PrefabLibrary.h"
#include "topdown/CameraPan.h"
#include "topdown/ComponentSchema.h"
#include "topdown/editor/EditorMath.h"
#include "topdown/view/LevelObjects.h"
#include "topdown/view/LevelRuntime.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace Pakal::TopDown
{
	class LevelEditorState : public Pakal::BaseGameState
	{
	public:
		// The game's state that plays a level file (null: the editor has no Play)
		using PlayFactory = std::function<Pakal::BaseGameState*(const std::string& levelPath)>;
		// Adds the game's own components to the schema (TopDown's are in it)
		using SchemaKinds = std::function<void(ComponentSchema& schema)>;

		LevelEditorState(std::string levelPath, PlayFactory play = {}, SchemaKinds kinds = {});
		~LevelEditorState() override;

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override;
		void on_deactivate(Pakal::Engine* engine) override;

	private:
		enum class Gizmo { Move, Turn, Scale };

		// Graphics thread (the debug UI's panel): all of the editor
		void panel();
		void open_view();
		void close_view();
		void draw_menu();
		void draw_hierarchy();
		void draw_inspector();
		void draw_components(Pakal::LevelObject& object, const Pakal::Json& resolved);
		void draw_prefabs();
		void draw_gizmo();
		void draw_selection();
		void viewport_input();
		void place_camera();
		void check_files();
		void focus_selection();
		// A layer over a base: its base's objects dimmed (and not picked while locked), the base's hidden ones listed, a new
		// layer made over the level open
		void apply_dimming();
		void draw_hidden();
		void draw_new_layer();
		// The level's intro camera ("camera": {"intro": ...}): its shots listed and edited (the Camera tab), drawn in the
		// view (each eye, the line to what it looks at, the path), one point at a time moved by the gizmo, and seen through
		// (one shot, or the whole path played as the game plays it)
		void draw_camera_tab();
		void draw_camera_path();
		bool pick_shot(float mouseX, float mouseY);
		bool set_camera_path(const Pakal::Json& path);
		Pakal::Json camera_path() const;
		// Where the camera is while it looks through a shot or plays the path; false when it is the free camera
		bool camera_view(CameraPan::Point& eye, CameraPan::Point& look, float& fovY);
		// The free camera's eye (render space), and a render point as a level point (level units, z up)
		CameraPan::Point free_eye() const;
		Pakal::Json level_point(const CameraPan::Point& point) const;
		void stop_preview();
		// Live editing (LiveEditing): another program's operations done here, between the person's own; what the editor
		// shows, for it
		void serve_live();
		Pakal::Json live_state() const;
		// Opening a level: the window that chooses one (Level > Open, Ctrl+O, or at the start without one)
		void show_opener();
		void draw_opener();
		bool open_level(const std::string& path);
		void read_recent();
		void remember_recent(const std::string& path);
		// The object under a ray (the smallest box it goes through), or ""
		std::string pick_at(const EditorMath::Ray& ray) const;
		void self_test();

		bool apply(const Pakal::Json& operation);
		void undo(bool redo);
		bool save();
		bool reload_document();
		int selected_index() const;
		Pakal::LevelObject* selected_object();

		std::string m_path;
		PlayFactory m_play;
		SchemaKinds m_kinds;
		Pakal::Engine* m_engine = nullptr;
		std::unique_ptr<LevelRuntime> m_runtime;
		std::unique_ptr<Pakal::LevelDocument> m_document;
		std::unique_ptr<Pakal::LevelEditor> m_editor;
		Pakal::PrefabLibrary m_prefabs;
		ComponentSchema m_schema;
		LevelObjects m_objects;
		std::string m_historyFolder;
		unsigned m_panelId = 0;
		std::recursive_mutex m_mutex;   // the panel (graphics thread) and the state's calls (the game's)

		// what is edited
		std::vector<std::string> m_selection;   // keys; the first is the one the inspector and the gizmo show
		Gizmo m_gizmo = Gizmo::Move;
		bool m_local = false;
		bool m_snap = true;
		float m_snapMove = 0.5f, m_snapTurn = 15.0f, m_snapScale = 0.1f;
		bool m_gizmoActive = false;
		bool m_dragging = false;   // the gizmo moved the object: the document changes when it is let go
		Mat4 m_dragged{};
		std::string m_placing;                  // a prefab placed where the floor is clicked ("" when not placing)
		char m_filter[64] = {};
		char m_newPrefab[64] = {};
		std::string m_status;
		bool m_dirty = false;
		std::filesystem::file_time_type m_savedAt{};
		float m_checkTimer = 0.0f;

		// the free camera: it looks at a point of render space from 'distance' away, turned 'yaw' and raised 'pitch'
		float m_target[3] = { 0.0f, 0.0f, 0.0f };
		float m_yaw = 0.6f, m_pitch = 0.9f, m_distance = 30.0f;

		float m_fovY = 22.0f;   // the editor camera's

		// the intro camera: the shot edited (-1: none) and which of its points the gizmo moves; looking through a shot, or
		// playing the path (seconds into it)
		int m_shot = -1;
		bool m_shotLook = false;
		bool m_lookThrough = false;
		bool m_previewing = false;
		float m_previewSeconds = 0.0f;

		// live editing: requests waiting until the person lets go of what they drag; the last one done, shown a while
		Pakal::LiveEditing::Host m_live;
		std::vector<std::pair<std::string, Pakal::Json>> m_liveWaiting;
		float m_liveTimer = 0.0f, m_beatTimer = 0.0f;
		std::string m_liveNote;
		float m_liveNoteSeconds = 0.0f;

		// Play: the game's state on top of the editor
		std::atomic<bool> m_playAsked{ false };
		std::atomic<bool> m_playing{ false };
		bool m_viewOpen = false;

		// a layer over a base (LevelLayerEditing): what is saved is the layer
		Pakal::LevelLayerEditing m_layer;
		std::filesystem::file_time_type m_baseSavedAt{};
		bool m_lockBase = true, m_dimBase = true;
		bool m_newLayer = false;
		char m_newLayerPath[512] = {};
		std::string m_newLayerError;

		// the level opener
		bool m_opener = false;
		bool m_discardAsked = false;
		char m_openPath[512] = {};
		char m_openFilter[64] = {};
		std::string m_openError;
		std::vector<std::string> m_found, m_recent;

		// PAKAL_EDITOR_SELFTEST
		bool m_selfTest = false;
		int m_testFrame = 0, m_testPlayFrame = 0, m_testBackFrame = 0, m_testFailures = 0;
		std::string m_testLayer;
		std::thread m_testAsker;   // another program asking the open editor (live editing)
		Pakal::Json m_testReply;
		std::string m_testKey;
		double m_testX = 0.0;
	};
}
