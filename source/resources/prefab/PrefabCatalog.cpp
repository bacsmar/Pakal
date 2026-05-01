#include "PrefabCatalog.h"
#include "LogMgr.h"
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Pakal
{
    bool PrefabCatalog::load_from_file(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR("[PrefabCatalog] Failed to open catalog file: %s", path.c_str());
            return false;
        }
        std::stringstream buf;
        buf << file.rdbuf();
        return load_from_string(buf.str());
    }

    bool PrefabCatalog::load_from_string(const std::string& json)
    {
        JsonValue root = SimpleJsonParser::parse(json);
        if (root.is_null() || !root.has("prefabs") || !root["prefabs"].is_object())
        {
            LOG_ERROR("[PrefabCatalog] Catalog JSON must have a top-level 'prefabs' object");
            return false;
        }

        const JsonValue& prefabsObj = root["prefabs"];
        bool allOk = true;

        for (const auto& id : prefabsObj.keys())
        {
            const JsonValue& def = prefabsObj[id];

            if (!def.has("components") || !def["components"].is_array())
            {
                LOG_ERROR("[PrefabCatalog] Prefab '%s' is missing a 'components' array", id.c_str());
                allOk = false;
                continue;
            }

            // Validate: no duplicate component ids, every component has id and type.
            const JsonValue& comps = def["components"];
            std::unordered_set<std::string> seen;
            bool valid = true;

            for (size_t i = 0; i < comps.size(); ++i)
            {
                const JsonValue& comp = comps[i];

                if (!comp.has("id") || comp["id"].as_string().empty())
                {
                    LOG_ERROR("[PrefabCatalog] Prefab '%s' component[%zu] missing 'id'",
                              id.c_str(), i);
                    valid = false;
                    break;
                }
                if (!comp.has("type") || comp["type"].as_string().empty())
                {
                    LOG_ERROR("[PrefabCatalog] Prefab '%s' component[%zu] missing 'type'",
                              id.c_str(), i);
                    valid = false;
                    break;
                }
                const std::string cid = comp["id"].as_string();
                if (!seen.insert(cid).second)
                {
                    LOG_ERROR("[PrefabCatalog] Prefab '%s' has duplicate component id '%s'",
                              id.c_str(), cid.c_str());
                    valid = false;
                    break;
                }
            }

            if (valid)
                m_prefabs[id] = def;
            else
                allOk = false;
        }

        return allOk;
    }

    const JsonValue* PrefabCatalog::find(const std::string& id) const
    {
        auto it = m_prefabs.find(id);
        return it != m_prefabs.end() ? &it->second : nullptr;
    }

} // namespace Pakal
