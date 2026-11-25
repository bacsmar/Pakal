///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ShadowProxies.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/ShadowProxies.h"

#include "ResourceManager.h"

#include <cstdlib>
#include <nlohmann/json.hpp>

namespace Pakal::TopDown
{
	ShadowProxies::ShadowProxies(const nlohmann::json& rules)
	{
		if (!rules.is_object()) return;
		m_suffix = rules.value("suffix", m_suffix);
		const nlohmann::json models = rules.value("models", nlohmann::json::array());
		if (!models.is_array()) return;
		for (const nlohmann::json& prefix : models)
		{
			if (prefix.is_string() && !prefix.get<std::string>().empty()) m_prefixes.push_back(prefix.get<std::string>());
		}
	}

	std::string ShadowProxies::proxy_for(const std::string& modelFile)
	{
		static const bool off = [] { const char* v = std::getenv("PAKAL_SHADOW_PROXY"); return v && std::string(v) == "0"; }();
		if (off || m_prefixes.empty() || modelFile.empty()) return std::string();
		const auto cached = m_known.find(modelFile);
		if (cached != m_known.end()) return cached->second;

		const size_t slash = modelFile.find_last_of("/\\");
		const size_t dot = modelFile.rfind('.');
		const size_t nameAt = slash == std::string::npos ? 0 : slash + 1;
		std::string proxy;
		if (dot != std::string::npos && dot > nameAt)
		{
			const std::string name = modelFile.substr(nameAt);
			for (const std::string& prefix : m_prefixes)
			{
				if (name.rfind(prefix, 0) != 0) continue;
				// a proxy is not a model with a proxy of its own
				if (name.size() >= m_suffix.size() && modelFile.compare(dot - m_suffix.size(), m_suffix.size(), m_suffix) == 0) break;
				const std::string candidate = modelFile.substr(0, dot) + m_suffix + ".glb";
				const bool exists = m_exists ? m_exists(candidate) : ResourceMgr.exists(candidate);
				if (exists) proxy = candidate;
				break;
			}
		}
		m_known[modelFile] = proxy;
		return proxy;
	}
}
