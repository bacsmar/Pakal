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
//          View > Device (DevicePanel) measures and tunes a running game, here or on a phone, through its engine tools.
//          A hierarchy: an object hangs from another ("parent", Edit > Hang from the first selected, Ctrl+P); the gizmo, a
//          vertex snap and [ ] carry what hangs from what they move; the list shows the tree.
//          With several selected, the inspector sets a field on all of them at once (one undo; a field they differ in shows a
//          dash) and the gizmo moves, turns or scales all of them, each about its own pivot.
//          Selecting: a left drag in the view draws a box (Shift adds, Ctrl takes out), Edit > Select the same prefab
//          (Ctrl+Shift+A), F frames the whole selection, H hides it from the view, Shift+H shows it alone, Alt+H shows all.
//          Placing aids: a grid on the ground (View > Grid), [ and ] turn the selection by the turn step, and V held
//          snaps: a vertex of the selection (the one nearest the pointer when the drag starts) is dragged onto a vertex of
//          another model (or the surface under the pointer).
//
//          The Camera tab edits the level's intro camera ("camera": {"intro": ...}, LevelCameraPath): its shots are drawn in
//          the view, a click on one's eye or look point takes it and the gizmo moves it; a shot can be looked through and
//          the whole path played, ending on the play camera at the spawn when it ends on play.
//
//          While it is open, pakal-level (or an agent) works in it with the person (LiveEditing): its operations are
//          done in the open document, undoable with the person's own and marked as the agent's, after the person lets
//          go of what they drag; they are saved at once when nothing else was unsaved.
//
//          The Assets window (AssetBrowser) shows every model of the catalog with its picture; one dragged into the view
//          (or clicked, then the view clicked) lands on the surface under the pointer (a model's triangles: a floor, a
//          roof; else the ground) as an object made from a prefab of its own, written for it the first time it is used:
//          in the level's prefabs folder, or in a shared library the level names (editor_settings.json says which).
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
#include "topdown/editor/AssetBrowser.h"
#include "topdown/editor/DevicePanel.h"
#include "mcp/McpServer.h"
#include "topdown/editor/EditorMath.h"
#include "topdown/view/LevelObjects.h"
#include "topdown/view/LevelRuntime.h"

#include <atomic>
#include <filesystem>
#include <map>
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
		// Several objects selected: each field set on all of them in one step, a field they differ in shown as mixed
		void draw_inspector_many();
		bool apply_all(const std::vector<Pakal::Json>& ops);
		void draw_components(Pakal::LevelObject& object, const Pakal::Json& resolved);
		void draw_prefabs();
		void draw_gizmo();
		// The gizmo over several objects: where one goes when the first went from 'before' to 'after' (the same move, the
		// same turn and scale about its own pivot), and a placement as a place operation
		static Mat4 follow(const Mat4& placement, const Mat4& before, const Mat4& after);
		// The hierarchy ("parent": the object one hangs from): the objects under some of 'moved' (children, theirs...) that
		// are not moved themselves, carried rigidly with the nearest one moved (its before and after placements): shown
		// there (ops null) or placed there by operations added to 'ops'
		void carry_children(const std::map<std::string, std::pair<Mat4, Mat4>>& moved, std::vector<Pakal::Json>* ops);
		// The selection hung from its first object, or from nothing; the list as a tree
		void parent_selection(bool unparent);
		// Copy and paste, also between levels (editor_clipboard.json next to the game): the selection as the level writes
		// it, its models' paths from the game's folder, its prefabs whole; pasted around a point of the level (meters, its
		// lowest on 'z'), with new keys, paths made the level's and the prefabs it lacks written into its folder
		bool copy_selection();
		bool paste(double x, double y, double z);
		void draw_tree(const std::string& key, const std::map<std::string, std::vector<int>>& children, int index);
		Pakal::Json place_operation(const Pakal::LevelObject& object, const Mat4& placed) const;
		void draw_selection();
		// Placing aids: the grid on the ground, the selection turned by the turn step, and vertex snapping (V held)
		void draw_grid();
		void turn_selection(double degrees);
		void vertex_snap();
		void apply_vertex_snap(const float delta[3]);   // the selection moved by it (render meters): one undoable step
		// The vertex of an object drawn nearest a point of the screen (render space; false without one within 'maxPixels'),
		// and what a vertex snap aims at under the pointer: the nearest vertex of the model the ray meets when it is drawn
		// within 'maxPixels' of the pointer, else the point of the surface
		bool vertex_near(const std::string& key, float x, float y, float maxPixels, float out[3]);
		bool snap_target(const EditorMath::Ray& ray, float x, float y, float maxPixels, float out[3]);
		std::string surface_key(const EditorMath::Ray& ray, float point[3]);
		void viewport_input();
		void place_camera();
		void check_files();
		void focus_selection();
		// Selecting: the objects whose middle is drawn in a rectangle of the screen (not hidden, not a locked base), every
		// object of the first one's prefab (its model, without one), and what the view hides (not the document: the view's)
		std::vector<std::string> keys_in_rect(float x0, float y0, float x1, float y1);
		void select_same();
		void hide_selection(bool isolate);
		void show_all();
		void draw_box();
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
		// The asset browser: its window, a model dragged over the view (or chosen and clicked) shown where it would land and
		// placed there, the prefab made for a model the first time ("" and 'error' when it cannot be; an operation the level
		// needs first, naming a shared library, goes into 'before')
		void draw_assets();
		void draw_drop();
		bool place_asset(int index, const EditorMath::Ray& ray);
		std::string catalog_prefab(const AssetEntry& entry, Pakal::Json& before, std::string& error);
		bool drop_point(const EditorMath::Ray& ray, double position[3], std::array<double, 3>& rotation);
		void hide_ghost();
		// Where a ray meets the level's surfaces: the nearest model's triangles (a roof, a floor tile, a table), else the
		// ground (render y 0). Render space; the normal faces the ray. 'skip': objects left out (what is being placed)
		bool surface_at(const EditorMath::Ray& ray, float point[3], float normal[3], const std::set<std::string>* skip = nullptr);
		// The rotation (degrees, level axes, Rz*Ry*Rx) that turns 'yaw' about a surface's normal and stands on it
		static std::array<double, 3> standing_rotation(const float normal[3], double yaw);
		// The editor's own settings (editor_settings.json next to the game): where the catalog's prefabs go
		void read_settings();
		void write_settings();
		std::string library_folder() const;
		void self_test();
		void self_test_scene(const std::function<void(bool, const char*)>& report);

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
		bool m_grid = true;
		// a box being drawn: where the press was, what was selected before it, whether it is a box yet
		bool m_boxing = false, m_boxPressed = false;
		float m_boxFrom[2] = {};
		std::vector<std::string> m_boxBefore;
		std::set<std::string> m_hidden;   // hidden from the view (H), by key
		int m_gridLines = 0;   // drawn last frame
		// a vertex snap being dragged: the selection's vertex taken, where it is now, and the selection's places when it began
		bool m_vertexDragging = false;
		float m_snapFrom[3] = {}, m_snapTo[3] = {};
		std::vector<std::pair<std::string, Mat4>> m_snapStart;
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

		// the Device window: a running game's engine tools (this computer's or a phone's), graphs and settings
		DevicePanel m_device;
		bool m_deviceOpen = false;

		// the asset browser and placing what it gives
		AssetBrowser m_assets;
		enum class PrefabHome { Level, Library };
		PrefabHome m_prefabHome = PrefabHome::Level;   // where a catalog model's prefab is written
		char m_library[256] = {};                     // the shared library ("" a "prefabs" folder beside the level's folder)
		bool m_alignToSurface = false;                // a placed model stands on the surface's slope
		double m_placeYaw = 0.0;                      // the turn a placed model gets (degrees)
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> m_ghost;   // the model being placed, where it would land
		std::string m_ghostFile;

		// PAKAL_EDITOR_SELFTEST
		bool m_selfTest = false;
		int m_testFrame = 0, m_testPlayFrame = 0, m_testBackFrame = 0, m_testFailures = 0;
		std::string m_testLayer;
		std::thread m_testAsker;   // another program asking the open editor (live editing)
		Pakal::Json m_testReply;
		std::string m_testKey;
		double m_testX = 0.0;
		std::string m_testLevel;   // the level the test started on (it comes back to it after the layer)
		int m_testAsset = -1;
		size_t m_testObjects = 0;
		std::vector<std::string> m_testPrefabs;   // the prefab files the test made (removed after)
		std::string m_testOther;
		float m_testVertex[3] = {}, m_testOtherVertex[3] = {};
		int m_testStage = 0, m_testStageFrame = 0;
		std::atomic<bool> m_testQuit{ false };
		std::thread m_testTools;   // the engine's tools called as a client would
		std::unique_ptr<Pakal::McpServer> m_testServer;   // PAKAL_EDITOR_SELFTEST_EXIT: the game ends when the test is done
	};
}
