# Synthetic data only: a translated/scaled/rotated logical group must not
# relocate spawn points, physics, meshes, or runtime-dump spawners.
file(MAKE_DIRECTORY "${TEST_DIR}/imported")
file(WRITE "${TEST_DIR}/imported/prop.dae" "<COLLADA/>")
file(WRITE "${TEST_DIR}/imported/zombiesdemo_nav.dae" "<COLLADA/>")
file(WRITE "${TEST_DIR}/level.json" [=[{"nodes":[{
 "name":"Group", "position":{"hasValue":true,"x":3400,"y":5200},
 "rotation":{"hasValue":true,"z":90}, "scale":{"hasValue":true,"x":2},
 "children":[{"name":"Spawnpoint","legacyId":"45036",
 "position":{"hasValue":true,"x":2200,"y":3700},
 "components":[{"name":"Spawnpoint"},{"name":"BuiltinCollision2DPolygon",
 "properties":[{"name":"Shape","value":"Rect"},{"name":"Size","value":"100,200"}]}]},
 {"name":"AmbientActor","legacyId":"7","components":[{"name":"Idiot"}]},
 {"name":"DropSource","legacyId":"8","components":[{"name":"PickupSpawn"}]},
 {"name":"LevelNav","components":[{"name":"Mesh","properties":[
 {"name":"meshFile","value":"zombiesdemo_nav.bdae"}]}]}]}]}]=])
file(WRITE "${TEST_DIR}/library.json" "{}")
file(WRITE "${TEST_DIR}/collision.json" [=[{"format":"zombies-compiled-collision","version":1,
 "units_per_meter":100,"segments":[{"layer":"solid","legacy_type":-1,
 "points":[[10000,10000],[10100,10000],[10100,10100],[10000,10100]]}]}]=])
file(WRITE "${TEST_DIR}/dump.json" [=[{"objects":[
 {"id":1,"templateName":"Group","childIds":[2,3],"position":{"x":3400,"y":5200},
 "rotationEulerDegrees":{"z":90},"scale":{"x":2,"y":2,"z":2}},
 {"id":2,"templateName":"ZombieSpawner","position":{"x":100,"y":200}},
 {"id":3,"templateName":"Prop","meshFile":"prop.bdae","position":{"x":300,"y":400},
 "rotationEulerDegrees":{"x":0,"y":0,"z":15},"scale":{"x":1,"y":1,"z":1},"hasBase":true},
 {"id":4,"templateName":"LevelNav","meshFile":"zombiesdemo_nav.bdae"}]}]=])
execute_process(COMMAND "${CONVERTER}" --level "${TEST_DIR}/level.json"
 --library "${TEST_DIR}/library.json" --imported "${TEST_DIR}/imported"
 --runtime-dump "${TEST_DIR}/dump.json" --collision "${TEST_DIR}/collision.json" --out "${TEST_DIR}/out"
 RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
 message(FATAL_ERROR "Conversion failed: ${output} ${errors}")
endif()
file(READ "${TEST_DIR}/out/level.json" level)
foreach(field IN ITEMS enemies pickups)
 string(JSON count LENGTH "${level}" ${field})
 if(NOT count EQUAL 0)
  message(FATAL_ERROR "Ambient actors/drop generators must not become live ${field}")
 endif()
endforeach()
foreach(field IN ITEMS ambientActors pickupSources)
 string(JSON count LENGTH "${level}" ${field})
 if(NOT count EQUAL 1)
  message(FATAL_ERROR "Authored ${field} must be preserved separately")
 endif()
endforeach()
string(JSON collision_layer GET "${level}" collision segments 0 layer)
string(JSON collision_filled GET "${level}" collision segments 0 filled)
if(NOT collision_layer STREQUAL "solid" OR NOT collision_filled)
 message(FATAL_ERROR "Compiled solid polygon must remain filled and solid")
endif()
string(JSON object_count LENGTH "${level}" objects)
if(NOT object_count EQUAL 1)
 message(FATAL_ERROR "Expected one visible prop, without navigation geometry; got ${object_count}")
endif()
foreach(spec IN ITEMS "script;points;45036;0|2200" "script;points;45036;1|3700"
 "script;anchors;45036;0|2200" "script;anchors;45036;1|3700" "script;anchors;3;0|300"
 "walls;0;x|2200" "walls;0;y|3700" "walls;0;w|100" "walls;0;h|200"
 "spawners;0;x|100" "spawners;0;y|200"
 "objects;0;x|300" "objects;0;y|400" "objects;0;rot;2|15" "objects;0;scale|1")
 string(REPLACE "|" ";" parts "${spec}")
 list(POP_BACK parts expected)
 string(JSON actual GET "${level}" ${parts})
 if(NOT actual EQUAL expected)
  message(FATAL_ERROR "${parts}: expected ${expected}, got ${actual}")
 endif()
endforeach()
