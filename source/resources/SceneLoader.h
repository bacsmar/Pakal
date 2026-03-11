#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "SimpleJsonParser.h"
#include "PakalMath.h"

namespace Pakal
{
	class Engine;
	class GenericEntity;

	/**
	 * SceneLoader - Loads complete game scenes from JSON definition files
	 * 
	 * A scene is a complete level/stage definition including:
	 * - All entities (player, enemies, platforms, decorations)
	 * - Spawn points (player start, goal, checkpoints)
	 * - Level metadata (name, difficulty, etc.)
	 * 
	 * This is more specialized than AssetLoader:
	 * - Validates required entities (player, goal)
	 * - Provides convenient access to spawn points
	 * - Manages complete level state
	 * 
	 * JSON Structure:
	 * {
	 *   "scene": {
	 *     "name": "Level 1",
	 *     "difficulty": 1,
	 *     "spawn_points": {
	 *       "player_start": { "x": 5, "y": 10, "z": 0 },
	 *       "goal": { "x": 40, "y": 5, "z": 0 }
	 *     },
	 *     "entities": {
	 *       "player": { ... },
	 *       "enemy_1": { ... },
	 *       ...
	 *     }
	 *   }
	 * }
	 */
	class _PAKALExport SceneLoader
	{
	public:
		struct SpawnPoint
		{
			std::string name;
			tmath::vector3df position;
		};

		struct SceneMetadata
		{
			std::string scene_name;
			int difficulty = 1;
			int expected_enemy_count = 0;
			std::unordered_map<std::string, SpawnPoint> spawn_points;
		};

		explicit SceneLoader(Engine* engine);
		~SceneLoader();

		/**
		 * Load a complete scene from JSON file
		 * @param scenePath Path to scene JSON file (e.g., "scenes/level1.json")
		 * @return true if scene loaded and validated successfully
		 */
		bool load_scene(const std::string& scenePath);

		/**
		 * Load a scene from JSON string
		 * @param jsonContent Scene definition as JSON string
		 * @return true if scene loaded and validated successfully
		 */
		bool load_scene_from_string(const std::string& jsonContent);

		/**
		 * Get scene metadata (name, difficulty, spawn points)
		 */
		const SceneMetadata& get_metadata() const { return m_metadata; }

		/**
		 * Get spawn point by name (e.g., "player_start", "goal")
		 * @param name Spawn point name
		 * @return SpawnPoint structure, position is (0,0,0) if not found
		 */
		SpawnPoint get_spawn_point(const std::string& name) const;

		/**
		 * Get loaded entity by descriptor
		 * @param descriptor Entity descriptor name from scene JSON
		 * @return Pointer to entity, or nullptr if not found
		 */
		GenericEntity* get_entity(const std::string& descriptor);

		/**
		 * Get all loaded entities
		 */
		const std::vector<GenericEntity*>& get_loaded_entities() const { return m_loadedEntities; }

		/**
		 * Validate that required entities exist in the scene
		 * @param required_entities List of entity names that must exist
		 * @return true if all required entities are present
		 */
		bool validate_required_entities(const std::vector<std::string>& required_entities) const;

		/**
		 * Unload current scene (clear all entities)
		 * Note: Entities are managed by EntityManager, not destroyed here
		 */
		void unload_scene();

	private:
		Engine* m_engine;
		std::vector<GenericEntity*> m_loadedEntities;
		SceneMetadata m_metadata;

		bool parse_scene_json(const JsonValue& root);
		bool parse_metadata(const JsonValue& sceneObj);
		bool parse_spawn_points(const JsonValue& spawnPointsObj);
		bool parse_entities(const JsonValue& entitiesObj);
	};
}
