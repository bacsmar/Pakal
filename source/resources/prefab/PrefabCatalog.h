#pragma once

#include "resources/SimpleJsonParser.h"
#include <string>
#include <unordered_map>

namespace Pakal
{
    // Holds parsed prefab definitions loaded from a catalog JSON file.
    // The catalog format is:
    //   { "prefabs": { "<id>": { "entity_type": "...", "components": [ ... ] } } }
    //
    // This class has no engine dependency; load_from_string works in tests
    // without an initialized engine. load_from_file uses plain std::ifstream.
    class PrefabCatalog
    {
    public:
        // Load from file path using std::ifstream directly (no ResourceManager).
        // In Phase 2, SceneLoader reads the file and calls load_from_string.
        bool load_from_file(const std::string& path);

        // Parse a catalog from a JSON string.
        // Returns false and logs errors if the JSON is invalid or contains
        // duplicate component ids. Valid prefabs are added even if others fail.
        bool load_from_string(const std::string& json);

        // Returns pointer to the raw prefab definition, or nullptr if unknown.
        const JsonValue* find(const std::string& id) const;

        bool empty() const { return m_prefabs.empty(); }

    private:
        std::unordered_map<std::string, JsonValue> m_prefabs;
    };

} // namespace Pakal
