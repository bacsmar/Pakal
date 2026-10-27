#include "PrefabResolver.h"
#include "PrefabCatalog.h"
#include "LogMgr.h"
#include <algorithm>
#include <unordered_set>

namespace Pakal
{
    PrefabResolver::PrefabResolver(const PrefabCatalog* catalog)
        : m_catalog(catalog)
    {
    }

    // Recursively merges patch into base.
    // - Objects: deep merge. Keys present only in base are kept; keys in patch
    //   overwrite or extend. Object keys are inserted sorted for determinism.
    // - Everything else (scalars, arrays): patch fully replaces base.
    JsonValue PrefabResolver::deep_merge_data(const JsonValue& base, const JsonValue& patch)
    {
        if (!base.is_object() || !patch.is_object())
            return patch;

        JsonValue result;
        result.set_object();

        // Collect all unique keys from both sides, sort for determinism.
        std::vector<std::string> baseKeys = base.keys();
        std::vector<std::string> patchKeys = patch.keys();

        std::vector<std::string> allKeys;
        allKeys.reserve(baseKeys.size() + patchKeys.size());
        allKeys.insert(allKeys.end(), baseKeys.begin(), baseKeys.end());
        for (const auto& k : patchKeys)
        {
            if (!base.has(k))
                allKeys.push_back(k);
        }
        std::sort(allKeys.begin(), allKeys.end());

        for (const auto& key : allKeys)
        {
            const bool inBase  = base.has(key);
            const bool inPatch = patch.has(key);

            if (inBase && inPatch &&
                base[key].is_object() && patch[key].is_object())
            {
                result.insert(key, deep_merge_data(base[key], patch[key]));
            }
            else if (inPatch)
            {
                result.insert(key, patch[key]);
            }
            else
            {
                result.insert(key, base[key]);
            }
        }

        return result;
    }

    JsonValue PrefabResolver::resolve(const std::string& prefabId,
                                      const JsonValue& overrides) const
    {
        const JsonValue* def = m_catalog ? m_catalog->find(prefabId) : nullptr;
        if (!def)
        {
            LOG_ERROR("[Prefab] Unknown prefab '%s'", prefabId.c_str());
            return {};
        }

        const JsonValue& comps = (*def)["components"];

        // Validate all override keys up front.
        if (overrides.is_object())
        {
            // Build set of valid component ids from the prefab definition.
            std::unordered_set<std::string> validIds;
            for (size_t i = 0; i < comps.size(); ++i)
                validIds.insert(comps[i]["id"].as_string());

            for (const auto& oid : overrides.keys())
            {
                if (!validIds.count(oid))
                {
                    LOG_ERROR("[Prefab] Override targets unknown component id '%s' in prefab '%s'",
                              oid.c_str(), prefabId.c_str());
                    return {};
                }
                const JsonValue& ov = overrides[oid];
                if (ov.has("type"))
                {
                    LOG_ERROR("[Prefab] Override cannot change 'type' of component '%s' in prefab '%s'",
                              oid.c_str(), prefabId.c_str());
                    return {};
                }
                if (ov.has("id"))
                {
                    LOG_ERROR("[Prefab] Override cannot change 'id' of component '%s' in prefab '%s'",
                              oid.c_str(), prefabId.c_str());
                    return {};
                }
            }
        }

        // Build concrete definition, preserving component order from the prefab.
        JsonValue result;
        result.set_object();

        if (def->has("entity_type"))
            result.insert("entity_type", (*def)["entity_type"]);

        JsonValue mergedComps;
        mergedComps.set_array();

        for (size_t i = 0; i < comps.size(); ++i)
        {
            const JsonValue& comp = comps[i];
            const std::string cid = comp["id"].as_string();

            const bool hasOverride = overrides.is_object() && overrides.has(cid);

            if (hasOverride)
            {
                JsonValue merged;
                merged.set_object();
                merged.insert("id",   comp["id"]);
                merged.insert("type", comp["type"]);

                JsonValue baseData = comp.has("data") ? comp["data"] : JsonValue();
                if (baseData.is_null())
                    baseData.set_object();

                const JsonValue& ovEntry = overrides[cid];
                if (ovEntry.has("data"))
                    merged.insert("data", deep_merge_data(baseData, ovEntry["data"]));
                else
                    merged.insert("data", baseData);

                mergedComps.push_back(merged);
            }
            else
            {
                mergedComps.push_back(comp);
            }
        }

        result.insert("components", mergedComps);
        return result;
    }

} // namespace Pakal
