///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetBrowser.h
// Purpose: The editor's asset browser: the catalog's models (AssetCatalog) as a grid of pictures with a
//          search box and filters by pack and role. Each picture is drawn by the engine (the model alone,
//          BgfxGraphicsSystem::request_model_picture) the first time it is shown and kept on disk
//          (editor_cache/thumbnails/<pack>/<model>.png next to the game, drawn again when the model's file
//          is newer). A tile is dragged into the view to place its model, or clicked to place it with
//          clicks. Graphics thread (the debug UI's panel).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/editor/AssetCatalog.h"

#include <bgfx/bgfx.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class LevelRuntime;

	class AssetBrowser
	{
	public:
		static constexpr const char* kCacheFolder = "editor_cache/thumbnails";
		static constexpr uint32_t kPictureSize = 96;
		static constexpr size_t kMostTextures = 600;   // pictures kept as textures at once (the least used go first)

		AssetBrowser();
		~AssetBrowser();
		AssetBrowser(const AssetBrowser&) = delete;
		AssetBrowser& operator=(const AssetBrowser&) = delete;

		// Reads the catalog (the game's folder: the working one) and takes the runtime that loads the models
		void open(LevelRuntime& runtime);
		// Lets the textures and the models go (graphics thread)
		void close();
		AssetCatalog& catalog() { return m_catalog; }

		// The panel's contents: the search box, the filters and the grid
		void draw();
		// Each frame: the next pictures read from the disk or drawn
		void update();

		// The entry dragged out of the grid now (-1 none); dropped: what the drag ends on is the editor's
		int dragged() const { return m_dragged; }
		void end_drag() { m_dragged = -1; }
		// The entry clicked to place with clicks (-1 none)
		int chosen() const { return m_chosen; }
		void choose(int index) { m_chosen = index; }
		// Its picture as a texture (invalid while it is being made); asks for it when it has none
		bgfx::TextureHandle picture(size_t index);
		// What has been done: pictures drawn by the engine, read from the disk, failed
		size_t drawn() const;
		size_t read() const;
		size_t failed() const;
		// Whether every picture asked for is done (made, read or failed)
		bool idle() const;
		std::string cache_file(size_t index) const;

	private:
		struct Shared;   // what a picture being drawn reports into (it may come back after the browser is gone)
		void make_next();

		AssetCatalog m_catalog;
		LevelRuntime* m_runtime = nullptr;
		std::shared_ptr<Shared> m_shared;
		char m_search[96] = {};
		std::string m_pack, m_role;
		std::vector<size_t> m_shown;
		bool m_filterChanged = true;
		int m_dragged = -1, m_chosen = -1;
		float m_tile = 72.0f;
	};
}
