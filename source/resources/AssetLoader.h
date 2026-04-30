#pragma once

#include "Config.h"
#include "EntityHandle.h"
#include <string>
#include <vector>
#include <memory>
#include "SimpleJsonParser.h"

namespace Pakal
{
	class Engine;
	class Entity;
	class GenericEntity;
	class EntityManager;
	class ComponentManager;

	/**
	 * AssetLoader - Loads game entities and components from JSON asset definition files
	 * 
	 * This is a general-purpose loader that can be used by any game built with Pakal.
	 * 
	 * JSON Structure:
	 * {
	 *   "entities": {
	 *     "player": {
	 *       "type": "Pakal::GenericEntity",
	 *       "position": { "x": 10.0, "y": 5.0, "z": 0.0 },
	 *       "components": [
	 *         {
	 *           "type": "SpritePhysicsComponent",
	 *           "physics_data": { "sprite": "path/to/physics.def", ... }
	 *         }
	 *       ]
	 *     }
	 *   }
	 * }
	 */
	class _PAKALExport AssetLoader
	{
	public:
		explicit AssetLoader(Engine* engine);
		~AssetLoader();

		/**
		 * Load entities from JSON file
		 * @param filePath Path to JSON asset definition file
		 * @return Number of entities successfully loaded
		 */
		size_t load_from_file(const std::string& filePath);

		/**
		 * Load entities from JSON string
		 * @param jsonContent JSON asset definition as string
		 * @return Number of entities successfully loaded
		 */
		size_t load_from_string(const std::string& jsonContent);

		/**
		 * Get all loaded entities by descriptor
		 * @param descriptor Entity descriptor from JSON definition
		 * @return Pointer to loaded entity, or nullptr if not found
		 */
		EntityHandle get_entity_handle(const std::string& descriptor) const;
		GenericEntity* get_entity(const std::string& descriptor);

		/**
		 * Get all loaded entity handles
		 */
		const std::vector<EntityHandle>& get_loaded_entity_handles() const { return m_loadedEntities; }

		/**
		 * Get all loaded entities
		 */
		const std::vector<GenericEntity*>& get_loaded_entities() const;

		/**
		 * Clear all loaded entities
		 */
		void clear();

	private:
		Engine* m_engine;
		EntityManager* m_entityManager;
		ComponentManager* m_componentManager;
		std::vector<EntityHandle> m_loadedEntities;
		mutable std::vector<GenericEntity*> m_loadedEntityCache;

		// Internal parsing and creation methods
		GenericEntity* resolve_entity(EntityHandle handle) const;
		bool create_entity_from_json(const std::string& entityName, const JsonValue& entityDef);
		bool create_component_from_json(GenericEntity* entity, const JsonValue& componentDef);

		// Component-specific creators (extensible pattern)
		bool create_sprite_physics_component(GenericEntity* entity, const JsonValue& data);
		bool create_sprite_component(GenericEntity* entity, const JsonValue& data);
		bool create_sprite_2d_component(GenericEntity* entity, const JsonValue& data);
		bool create_camera_component(GenericEntity* entity, const JsonValue& data);
	};
}
