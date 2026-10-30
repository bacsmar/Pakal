///////////////////////////////////////////////////////////////////////////////////////////////////////
// File: AndroidAssetSource.h
// Purpose: ResourceManager's source for the APK's own assets (AAssetManager): the game reads its
//          files straight out of the package, nothing is copied to the app's storage first. A path
//          under the resource root (the app's storage, where writable things live) is looked up in the
//          APK by the part after the root ("<root>/Assets/x.json" -> "Assets/x.json").
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ISource.h"

#include <android/asset_manager.h>

#include <sstream>
#include <string>
#include <unordered_set>

namespace Pakal
{
	class AndroidAssetSource final : public ISource
	{
		DECLARE_RTTI_WITH_BASE(AndroidAssetSource, ISource);
	public:
		bool initialize(AAssetManager* assets, const std::string& root)
		{
			m_assets = assets;
			m_root = root.empty() || root.back() == '/' ? root : root + "/";
			if (!m_assets) return false;
			// the package's index (assets.list, "path<TAB>md5" a line, written when it is packaged): whether a file is
			// there is a lookup, not a call into the asset manager
			if (AAsset* list = AAssetManager_open(m_assets, "assets.list", AASSET_MODE_BUFFER))
			{
				const char* text = static_cast<const char*>(AAsset_getBuffer(list));
				std::istringstream lines(std::string(text ? text : "", text ? static_cast<size_t>(AAsset_getLength(list)) : 0));
				for (std::string line; std::getline(lines, line);)
				{
					std::string path = line.substr(0, line.find('\t'));
					while (!path.empty() && (path.back() == '\r' || path.back() == ' ')) path.pop_back();
					if (!path.empty()) m_index.insert(path);
				}
				AAsset_close(list);
			}
			return true;
		}

		bool has_resource(const Path& pathToResource) override
		{
			const std::string path = in_package(pathToResource.c_str());
			if (path.empty()) return false;
			if (!m_index.empty()) return m_index.count(path) != 0;
			AAsset* asset = AAssetManager_open(m_assets, path.c_str(), AASSET_MODE_UNKNOWN);
			if (asset) AAsset_close(asset);
			return asset != nullptr;
		}

		SharedPtr<std::istream> open_resource(const Path& pathToResource) override
		{
			const std::string path = in_package(pathToResource.c_str());
			if (path.empty() || (!m_index.empty() && !m_index.count(path))) return nullptr;
			AAsset* asset = AAssetManager_open(m_assets, path.c_str(), AASSET_MODE_BUFFER);
			if (!asset) return nullptr;
			const void* data = AAsset_getBuffer(asset);
			const off_t size = AAsset_getLength(asset);
			SharedPtr<std::istream> stream;
			if (data && size >= 0)
			{
				stream = std::make_shared<std::istringstream>(std::string(static_cast<const char*>(data), static_cast<size_t>(size)),
					std::ios::binary);
			}
			AAsset_close(asset);
			return stream;
		}

	private:
		// The path inside the package: what follows the root, without "./"; empty when it is outside it (another absolute path)
		std::string in_package(std::string path) const
		{
			if (!m_root.empty() && path.compare(0, m_root.size(), m_root) == 0) path = path.substr(m_root.size());
			while (path.compare(0, 2, "./") == 0) path = path.substr(2);
			return path.empty() || path[0] == '/' ? std::string() : path;
		}

		AAssetManager* m_assets = nullptr;
		std::string m_root;
		std::unordered_set<std::string> m_index;   // the package's files (assets.list); empty: ask the asset manager
	};
}
