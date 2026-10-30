///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditor.h
// Purpose: Edits a LevelDocument through named operations, each one undoable: what the pakal-level
//          command line, an agent and the game's level panel all use, so a level is changed the same
//          way whoever changes it. An operation is JSON ({ "op": "move", "object": "traffic_cone_03",
//          "x": 21.5, "y": 30 }); positions in it are meters (the document keeps level units). Objects are
//          named by their key, "#<index>" or "id:<game id>". The undo history can be kept in a folder
//          next to the level, so it lasts between runs of the command line.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"
#include "PrefabLibrary.h"

#include <string>
#include <vector>

namespace Pakal
{
	class LevelEditor
	{
	public:
		explicit LevelEditor(LevelDocument& document) : m_document(document) {}
		// The level's prefabs: instantiate checks the prefab exists, apply-to-prefab and make-prefab write prefab
		// files (a prefab's file is not part of the undo history: undoing gives back the level, not the prefab)
		void set_prefabs(PrefabLibrary* prefabs) { m_prefabs = prefabs; }

		// Does an operation; "" when done (then undoable), else what is wrong (and the document is as it was)
		std::string apply(const Json& operation);
		bool can_undo() const { return !m_undo.empty(); }
		bool can_redo() const { return !m_redo.empty(); }
		// What the last undone (or redone) operation was, or "" when there was none
		std::string undo();
		std::string redo();
		// The operations done, oldest first (each as it was given), and those that can be redone
		const std::vector<Json>& done() const { return m_done; }
		const std::vector<Json>& undone() const { return m_undoneOps; }

		// The history kept in a folder (one file per state, and an index): load it before editing, save it after
		bool load_history(const std::string& folder, std::string& error);
		bool save_history(const std::string& folder, std::string& error) const;

		// One line per operation: its name and its fields
		static const std::vector<std::string>& help();
		// How many states the history keeps
		static constexpr size_t kHistory = 30;

	private:
		std::string run(const Json& operation);
		void remember(const Json& operation);

		LevelDocument& m_document;
		PrefabLibrary* m_prefabs = nullptr;
		std::vector<Json> m_undo, m_redo;          // the document before each operation done, and after each one undone
		std::vector<Json> m_done, m_undoneOps;     // the operations themselves
	};
}
