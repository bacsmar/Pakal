#pragma once

#include "resources/SimpleJsonParser.h"
#include <string>

namespace Pakal
{
    class PrefabCatalog;

    // Resolves prefab ids to concrete entity definitions.
    //
    // resolve() takes a prefab id and an optional overrides object, performs
    // lookup + clone + deep-merge, and returns a concrete entity definition:
    //   { "entity_type": "...", "components": [ { "id", "type", "data" }, ... ] }
    //
    // Error policy:
    //   - Unknown prefab id             -> returns null JsonValue, logs error.
    //   - Override targets unknown id   -> returns null JsonValue, logs error.
    //   - Override tries to change type -> returns null JsonValue, logs error.
    //   - Override tries to change id   -> returns null JsonValue, logs error.
    //
    // The resolver has no I/O or engine dependency; safe to use in unit tests.
    class PrefabResolver
    {
    public:
        explicit PrefabResolver(const PrefabCatalog* catalog);

        // Returns concrete entity definition, or a null JsonValue on error.
        // Pass an empty/null JsonValue for overrides when none are needed.
        JsonValue resolve(const std::string& prefabId, const JsonValue& overrides) const;

    private:
        // Deep-merges patch into base. Scalars and arrays in patch fully replace.
        // Object keys in the result are sorted for deterministic stringify output.
        static JsonValue deep_merge_data(const JsonValue& base, const JsonValue& patch);

        const PrefabCatalog* m_catalog;
    };

} // namespace Pakal
