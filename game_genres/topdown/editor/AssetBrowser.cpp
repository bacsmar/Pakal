///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetBrowser.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/editor/AssetBrowser.h"

#include "LogMgr.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "topdown/view/LevelRuntime.h"

#include <imgui/imgui.h>   // bgfx's ImGui helpers: a bgfx texture in ImGui::Image

#include <algorithm>
#include <filesystem>

namespace Pakal::TopDown
{
	struct AssetBrowser::Shared
	{
		struct Picture
		{
			enum class State { None, Asked, Ready, Failed } state = State::None;
			bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
			uint64_t used = 0;   // the frame it was last shown
		};
		Pakal::BgfxGraphicsSystem* graphics = nullptr;
		std::vector<Picture> pictures;
		std::deque<size_t> asked;
		int loading = -1;    // the entry whose model is loading to be drawn
		bool drawing = false;
		uint64_t frame = 0;
		size_t drawn = 0, read = 0, failed = 0, textures = 0;

		void show(size_t index, uint32_t size, const std::vector<uint8_t>& rgba)
		{
			Picture& picture = pictures[index];
			picture.texture = graphics->create_ui_texture(size, size, rgba);
			picture.state = bgfx::isValid(picture.texture) ? Picture::State::Ready : Picture::State::Failed;
			picture.used = frame;
			if (bgfx::isValid(picture.texture)) ++textures;
		}

		void forget(Picture& picture)
		{
			if (bgfx::isValid(picture.texture))
			{
				graphics->destroy_ui_texture(picture.texture);
				--textures;
			}
			picture.texture = BGFX_INVALID_HANDLE;
			picture.state = Picture::State::None;
		}
	};

	AssetBrowser::AssetBrowser() : m_shared(std::make_shared<Shared>())
	{
	}

	AssetBrowser::~AssetBrowser() = default;

	void AssetBrowser::open(LevelRuntime& runtime)
	{
		m_runtime = &runtime;
		m_catalog.load();
		for (const auto& line : m_catalog.sources()) LOG_INFO("[LevelEditor] assets: %s", line.c_str());
		m_shared->graphics = &runtime.graphics();
		m_shared->pictures.assign(m_catalog.entries().size(), Shared::Picture());
		m_shared->asked.clear();
		m_filterChanged = true;
	}

	void AssetBrowser::close()
	{
		if (!m_shared->graphics) return;
		for (auto& picture : m_shared->pictures) m_shared->forget(picture);
		m_shared->asked.clear();
		m_shared->loading = -1;
		// a picture still being drawn reports into a state no one reads
		m_shared = std::make_shared<Shared>();
	}

	std::string AssetBrowser::cache_file(size_t index) const
	{
		const AssetEntry& entry = m_catalog.entries()[index];
		return (std::filesystem::path(kCacheFolder) / entry.pack / (entry.id + ".png")).generic_string();
	}

	bgfx::TextureHandle AssetBrowser::picture(size_t index)
	{
		if (index >= m_shared->pictures.size()) return BGFX_INVALID_HANDLE;
		Shared::Picture& picture = m_shared->pictures[index];
		picture.used = m_shared->frame;
		if (picture.state == Shared::Picture::State::None)
		{
			picture.state = Shared::Picture::State::Asked;
			m_shared->asked.push_back(index);
		}
		return picture.texture;
	}

	size_t AssetBrowser::drawn() const { return m_shared->drawn; }
	size_t AssetBrowser::read() const { return m_shared->read; }
	size_t AssetBrowser::failed() const { return m_shared->failed; }
	bool AssetBrowser::idle() const { return m_shared->asked.empty() && m_shared->loading < 0 && !m_shared->drawing; }

	void AssetBrowser::update()
	{
		Shared& shared = *m_shared;
		if (!shared.graphics || !m_runtime) return;
		++shared.frame;
		// too many textures: the ones not shown for longest go (read from the disk again when they are)
		if (shared.textures > kMostTextures)
		{
			std::vector<size_t> held;
			for (size_t i = 0; i < shared.pictures.size(); ++i)
			{
				if (bgfx::isValid(shared.pictures[i].texture) && shared.pictures[i].used + 2 < shared.frame) held.push_back(i);
			}
			std::sort(held.begin(), held.end(), [&](size_t a, size_t b) { return shared.pictures[a].used < shared.pictures[b].used; });
			for (size_t i = 0; i < held.size() && shared.textures > kMostTextures * 3 / 4; ++i) shared.forget(shared.pictures[held[i]]);
		}
		// pictures on the disk: a few each frame
		std::error_code code;
		for (int budget = 8; budget > 0 && !shared.asked.empty(); --budget)
		{
			const size_t index = shared.asked.front();
			const std::string file = cache_file(index);
			const auto made = std::filesystem::last_write_time(file, code);
			const bool cached = !code && made >= std::filesystem::last_write_time(m_catalog.entries()[index].file, code) && !code;
			if (!cached) break;   // drawn below
			shared.asked.pop_front();
			uint32_t width = 0, height = 0;
			std::vector<uint8_t> rgba;
			if (Pakal::BgfxGraphicsSystem::read_image(file, width, height, rgba) && width == height)
			{
				shared.show(index, width, rgba);
				++shared.read;
			}
			else
			{
				shared.pictures[index].state = Shared::Picture::State::None;
				std::filesystem::remove(file, code);   // drawn again
				shared.asked.push_front(index);
				break;
			}
		}
		make_next();
	}

	void AssetBrowser::make_next()
	{
		Shared& shared = *m_shared;
		if (shared.drawing) return;
		if (shared.loading < 0)
		{
			if (shared.asked.empty()) return;
			shared.loading = static_cast<int>(shared.asked.front());
			shared.asked.pop_front();
		}
		const size_t index = static_cast<size_t>(shared.loading);
		const AssetEntry& entry = m_catalog.entries()[index];
		bool failed = false;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> made = m_runtime->model_if_ready(entry.file, failed);
		if (!made && !failed) return;   // still decoding
		shared.loading = -1;
		if (!made)
		{
			shared.pictures[index].state = Shared::Picture::State::Failed;
			++shared.failed;
			return;
		}
		// the model is drawn alone: it is not in the level's picture meanwhile
		made->set_visible(false);
		std::shared_ptr<Pakal::MeshComponent3D_Bgfx> mesh(std::move(made));
		shared.drawing = true;
		const std::string file = cache_file(index);
		std::weak_ptr<Shared> weak = m_shared;
		shared.graphics->request_model_picture(mesh.get(), kPictureSize, [weak, mesh, index, file](uint32_t size, const std::vector<uint8_t>& rgba)
		{
			std::shared_ptr<Shared> alive = weak.lock();
			if (!alive) return;
			alive->drawing = false;
			if (rgba.empty())
			{
				alive->pictures[index].state = Shared::Picture::State::Failed;
				++alive->failed;
				return;
			}
			std::error_code code;
			std::filesystem::create_directories(std::filesystem::path(file).parent_path(), code);
			if (!Pakal::BgfxGraphicsSystem::write_png(file, size, size, rgba)) LOG_WARNING("[LevelEditor] cannot write %s", file.c_str());
			alive->show(index, size, rgba);
			++alive->drawn;
		});
	}

	void AssetBrowser::draw()
	{
		const std::vector<AssetEntry>& entries = m_catalog.entries();
		if (entries.empty())
		{
			for (const auto& line : m_catalog.sources()) ImGui::TextWrapped("%s", line.c_str());
			return;
		}
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputTextWithHint("##assetsearch", "search (name, pack, role, tag)", m_search, sizeof(m_search))) m_filterChanged = true;
		auto combo = [&](const char* label, std::string& value, const std::vector<std::string>& options)
		{
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f - 4.0f);
			if (!ImGui::BeginCombo(label, value.empty() ? "(any)" : value.c_str())) return;
			if (ImGui::Selectable("(any)", value.empty())) { value.clear(); m_filterChanged = true; }
			for (const auto& option : options)
			{
				if (ImGui::Selectable(option.c_str(), option == value)) { value = option; m_filterChanged = true; }
			}
			ImGui::EndCombo();
		};
		combo("##pack", m_pack, m_catalog.packs());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pack");
		ImGui::SameLine();
		combo("##role", m_role, m_catalog.roles());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Role");
		if (m_filterChanged)
		{
			m_shown = m_catalog.filter(m_search, m_pack, m_role);
			m_filterChanged = false;
		}
		ImGui::TextDisabled("%zu of %zu models", m_shown.size(), entries.size());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::SliderFloat("##tile", &m_tile, 48.0f, 128.0f, "%.0f px");
		if (m_chosen >= 0 && m_chosen < static_cast<int>(entries.size()))
		{
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Click the view to place %s (Shift: keep placing, Esc: stop)", entries[static_cast<size_t>(m_chosen)].id.c_str());
		}
		if (!idle()) ImGui::TextDisabled("drawing pictures... (%zu drawn, %zu from the disk)", drawn(), read());

		ImGui::BeginChild("##assetgrid");
		const ImGuiStyle& style = ImGui::GetStyle();
		const float cell = m_tile + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
		const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + style.ItemSpacing.x) / cell));
		const int rows = (static_cast<int>(m_shown.size()) + columns - 1) / columns;
		const float rowHeight = m_tile + style.FramePadding.y * 2.0f + ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y;
		ImGuiListClipper clipper;
		clipper.Begin(rows, rowHeight);
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				for (int column = 0; column < columns; ++column)
				{
					const size_t at = static_cast<size_t>(row * columns + column);
					if (at >= m_shown.size()) break;
					const size_t index = m_shown[at];
					const AssetEntry& entry = entries[index];
					if (column > 0) ImGui::SameLine();
					ImGui::BeginGroup();
					ImGui::PushID(static_cast<int>(index));
					const bgfx::TextureHandle texture = picture(index);
					const bool chosen = m_chosen == static_cast<int>(index);
					if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.75f, 1.0f));
					bool clicked;
					if (bgfx::isValid(texture))
					{
						clicked = ImGui::ImageButton("##tile", ImGui::toId(texture, IMGUI_FLAGS_ALPHA_BLEND, 0), ImVec2(m_tile, m_tile));
					}
					else
					{
						clicked = ImGui::Button("...##tile", ImVec2(m_tile + style.FramePadding.x * 2.0f, m_tile + style.FramePadding.y * 2.0f));
					}
					if (chosen) ImGui::PopStyleColor();
					if (clicked) m_chosen = chosen ? -1 : static_cast<int>(index);
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
					{
						const int payload = static_cast<int>(index);
						ImGui::SetDragDropPayload("PAKAL_ASSET", &payload, sizeof(payload));
						if (bgfx::isValid(texture)) ImGui::Image(texture, ImVec2(48.0f, 48.0f));
						ImGui::TextUnformatted(entry.id.c_str());
						ImGui::EndDragDropSource();
						m_dragged = payload;
					}
					if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					{
						std::string roles, tags;
						for (const auto& role : entry.roles) roles += (roles.empty() ? "" : ", ") + role;
						for (const auto& tag : entry.tags) tags += (tags.empty() ? "" : ", ") + tag;
						ImGui::SetTooltip("%s\npack: %s\nroles: %s\ntags: %s\n%s%s\nDrag it into the view, or click it and click the view", entry.id.c_str(),
							entry.pack.c_str(), roles.c_str(), tags.c_str(), entry.file.c_str(), entry.colliders.empty() ? "" : "\nwith colliders");
					}
					// its name under it, cut to the tile
					const float width = m_tile + style.FramePadding.x * 2.0f;
					std::string label = entry.id;
					while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > width) label.pop_back();
					if (label.size() < entry.id.size() && label.size() > 2) label.replace(label.size() - 2, 2, "..");
					ImGui::TextUnformatted(label.c_str());
					ImGui::PopID();
					ImGui::EndGroup();
				}
			}
		}
		ImGui::EndChild();
		// a drag that ended (dropped anywhere) is the editor's to take this frame; after it, none
		if (m_dragged >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_dragged = -1;
	}
}
