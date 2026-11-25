///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ShadowProxies.h
// Purpose: Which placed models cast their shadow through a proxy (docs/SHADOW_PROXIES.md): a few
//          alpha-tested quads with the model's silhouette (tools/foliage/shadow_proxy.py writes them
//          beside the model), drawn only in the sun's shadow pass while the model itself casts none. A
//          tree of tens of thousands of leaf cards costs as much in the shadow map as on screen; its
//          proxy is six triangles. The rules are data: {"suffix": "_shadow", "models": [name prefixes]}.
//          No graphics here: the level loaders (LevelRuntime::place_level, a game's own) ask it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace Pakal::TopDown
{
	class ShadowProxies
	{
	public:
		ShadowProxies() = default;
		// The rules from data: {"suffix": "_shadow", "models": ["Tree_", "Palm_"]} (an object without "models": none)
		explicit ShadowProxies(const nlohmann::json& rules);

		// The proxy of a model file: "<file without its extension><suffix>.glb" when the file's name starts with one of the
		// prefixes and that proxy exists (asked once a file, then remembered), else "". Off (always "") with
		// PAKAL_SHADOW_PROXY=0, to compare. Any thread that loads levels; not thread-safe.
		std::string proxy_for(const std::string& modelFile);

		bool empty() const { return m_prefixes.empty(); }
		const std::string& suffix() const { return m_suffix; }

		// Whether a file exists (ResourceMgr by default; a test gives its own)
		void set_exists(std::function<bool(const std::string&)> exists) { m_exists = std::move(exists); m_known.clear(); }

	private:
		std::vector<std::string> m_prefixes;
		std::string m_suffix = "_shadow";
		std::function<bool(const std::string&)> m_exists;
		std::map<std::string, std::string> m_known;
	};
}
