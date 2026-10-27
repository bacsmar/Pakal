// PrefabResolverTests.cpp
// Phase 1 unit tests for PrefabCatalog and PrefabResolver.
// No engine initialization required; logs go to stdout via LogMgr singleton.

#include "resources/prefab/PrefabCatalog.h"
#include "resources/prefab/PrefabResolver.h"
#include "resources/SimpleJsonParser.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace Pakal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const std::string CATALOG_JSON = R"({
  "prefabs": {
    "player": {
      "entity_type": "Pakal::GenericEntity",
      "components": [
        { "id": "physics", "type": "SpritePhysicsComponent", "data": {
            "body_type": "dynamic",
            "width": 0.8,
            "height": 1.6,
            "position": { "x": 0.0, "y": 0.0, "z": 0.0 }
          }
        },
        { "id": "sprite",  "type": "SpriteComponent2D", "data": {
            "width": 0.8,
            "height": 1.6,
            "layer": 10
          }
        }
      ]
    }
  }
})";

static PrefabCatalog make_catalog()
{
    PrefabCatalog cat;
    const bool ok = cat.load_from_string(CATALOG_JSON);
    assert(ok && "catalog must parse without errors");
    return cat;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_resolve_no_overrides()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    const JsonValue result = resolver.resolve("player", {});
    assert(!result.is_null());

    // entity_type preserved
    assert(result.has("entity_type"));
    assert(result["entity_type"].as_string() == "Pakal::GenericEntity");

    // two components, order preserved
    assert(result.has("components"));
    assert(result["components"].is_array());
    assert(result["components"].size() == 2);
    assert(result["components"][0]["id"].as_string() == "physics");
    assert(result["components"][1]["id"].as_string() == "sprite");

    // original data intact
    assert(result["components"][0]["data"]["body_type"].as_string() == "dynamic");
    assert(result["components"][1]["data"]["layer"].as_int() == 10);

    std::cout << "[PASS] test_resolve_no_overrides\n";
}

static void test_override_deep_merge()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    // Override only position.x inside physics.data
    const std::string overridesJson = R"({
      "physics": {
        "data": {
          "position": { "x": 5.0, "y": 3.0, "z": 0.0 }
        }
      }
    })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);
    const JsonValue result = resolver.resolve("player", overrides);
    assert(!result.is_null());

    const JsonValue& phys = result["components"][0]["data"];
    // overridden fields
    assert(phys["position"]["x"].as_float() == 5.0f);
    assert(phys["position"]["y"].as_float() == 3.0f);
    // non-overridden fields still present
    assert(phys["body_type"].as_string() == "dynamic");
    assert(phys["width"].as_float(0.0f) == 0.8f);

    // sprite component is untouched
    assert(result["components"][1]["data"]["layer"].as_int() == 10);

    std::cout << "[PASS] test_override_deep_merge\n";
}

static void test_unknown_component_id_fails()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    const std::string overridesJson = R"({ "nonexistent": { "data": {} } })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);
    const JsonValue result = resolver.resolve("player", overrides);

    assert(result.is_null() && "override with unknown id must fail");
    std::cout << "[PASS] test_unknown_component_id_fails\n";
}

static void test_override_type_change_fails()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    const std::string overridesJson = R"({ "physics": { "type": "AnotherType" } })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);
    const JsonValue result = resolver.resolve("player", overrides);

    assert(result.is_null() && "override that changes type must fail");
    std::cout << "[PASS] test_override_type_change_fails\n";
}

static void test_override_id_change_fails()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    const std::string overridesJson = R"({ "physics": { "id": "new_id" } })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);
    const JsonValue result = resolver.resolve("player", overrides);

    assert(result.is_null() && "override that changes id must fail");
    std::cout << "[PASS] test_override_id_change_fails\n";
}

static void test_determinism()
{
    const PrefabCatalog cat = make_catalog();
    const PrefabResolver resolver(&cat);

    const std::string overridesJson = R"({
      "physics": { "data": { "position": { "x": 2.0, "y": 4.0, "z": 0.0 } } }
    })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);

    const JsonValue r1 = resolver.resolve("player", overrides);
    const JsonValue r2 = resolver.resolve("player", overrides);

    const std::string s1 = SimpleJsonParser::stringify(r1);
    const std::string s2 = SimpleJsonParser::stringify(r2);
    assert(s1 == s2 && "same input must produce byte-identical output");

    std::cout << "[PASS] test_determinism\n";
}

static void test_catalog_duplicate_component_id_rejected()
{
    const std::string badCatalog = R"({
      "prefabs": {
        "bad": {
          "entity_type": "Pakal::GenericEntity",
          "components": [
            { "id": "dup", "type": "SpriteComponent2D", "data": {} },
            { "id": "dup", "type": "SpritePhysicsComponent", "data": {} }
          ]
        }
      }
    })";

    PrefabCatalog cat;
    const bool ok = cat.load_from_string(badCatalog);
    assert(!ok && "catalog with duplicate component id must fail");
    assert(cat.find("bad") == nullptr && "invalid prefab must not be stored");
    std::cout << "[PASS] test_catalog_duplicate_component_id_rejected\n";
}

static void test_array_override_replaces()
{
    // A prefab whose component data contains an array.
    const std::string catalogJson = R"({
      "prefabs": {
        "thing": {
          "entity_type": "Pakal::GenericEntity",
          "components": [
            { "id": "body", "type": "SpritePhysicsComponent", "data": {
                "tags": ["a", "b", "c"]
              }
            }
          ]
        }
      }
    })";

    PrefabCatalog cat;
    assert(cat.load_from_string(catalogJson));

    const PrefabResolver resolver(&cat);

    const std::string overridesJson = R"({
      "body": { "data": { "tags": ["x"] } }
    })";
    const JsonValue overrides = SimpleJsonParser::parse(overridesJson);
    const JsonValue result = resolver.resolve("thing", overrides);

    assert(!result.is_null());
    const JsonValue& tags = result["components"][0]["data"]["tags"];
    assert(tags.is_array());
    assert(tags.size() == 1);                            // replaced, not merged
    assert(tags[0].as_string() == "x");

    std::cout << "[PASS] test_array_override_replaces\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    test_resolve_no_overrides();
    test_override_deep_merge();
    test_unknown_component_id_fails();
    test_override_type_change_fails();
    test_override_id_change_fails();
    test_determinism();
    test_catalog_duplicate_component_id_rejected();
    test_array_override_replaces();

    std::cout << "\nAll prefab resolver tests passed.\n";
    return 0;
}
