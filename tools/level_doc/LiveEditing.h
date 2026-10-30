///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LiveEditing.h
// Purpose: A level editor that is open takes LevelEditor operations from another program (pakal-level, an
//          agent) into the document it has open: they are done there, undoable with its own, while the
//          person at the editor goes on working, and nothing is overwritten.
//
//          The way is a folder next to the level, <level>.live/ (as <level>.history/):
//          - editor.json: the editor's heartbeat, written every second while it is open (its process, the time,
//            and what it shows: the selection, the camera, unsaved changes). A heartbeat older than a few seconds
//            means no editor.
//          - inbox/<id>.json: a request, written whole (a temporary file renamed): {"ops": [...]} (done in order,
//            stopping at the first that fails), {"undo": true}, {"redo": true}, or {"query": "state"}.
//          - outbox/<id>.json: the editor's answer: {"ok", "results" (one per operation: "" done, else why not),
//            "saved", "error", "state"}.
//          No network: only files the user's own programs can write.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <string>
#include <utility>
#include <vector>

namespace Pakal::LiveEditing
{
	// <folder of the level>/<its name without extension>.live
	std::string folder_of(const std::string& levelPath);

	// The editor's side
	class Host
	{
	public:
		~Host();
		// Takes requests for this level (and stops taking them for the one before)
		bool open(const std::string& levelPath);
		// Removes the folder: no editor any more
		void close();
		const std::string& level() const { return m_level; }
		// The heartbeat with what the editor shows
		void beat(const Json& state);
		// The requests written since the last call, oldest first (each read once)
		std::vector<std::pair<std::string, Json>> take_requests();
		void answer(const std::string& id, const Json& reply);

	private:
		std::string m_level, m_folder;
	};

	// The other side. Is an editor open on this level (its heartbeat fresh)? Its state, when it is
	bool editor_open(const std::string& levelPath, Json* state = nullptr);
	// Sends a request and waits for the answer; false when no editor answers in time ('error' says why)
	bool ask(const std::string& levelPath, const Json& request, Json& reply, std::string& error, double timeoutSeconds = 10.0);
}
