#include "SceneLoader.h"
#include "AssetLoader.h"
#include "Engine.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include <fstream>
#include <sstream>

namespace Pakal
{
	SceneLoader::SceneLoader(Engine* engine)
		: m_engine(engine)
	{
		if (!engine) {
			LOG_ERROR("[SceneLoader] Engine is null!");
		}
	}

	SceneLoader::~SceneLoader()
	{
	}

	GenericEntity* SceneLoader::resolve_entity(EntityHandle handle) const
	{
		if (!m_engine || !m_engine->entity_manager())
		{
			return nullptr;
		}

		return m_engine->entity_manager()->resolve<GenericEntity>(handle);
	}

	bool SceneLoader::load_scene(const std::string& scenePath)
	{
		LOG_INFO("[SceneLoader] Loading scene from file: %s", scenePath.c_str());

		try {
			auto stream = ResourceMgr.open_read_resource(scenePath);
			if (!stream) {
				LOG_ERROR("[SceneLoader] Failed to open scene file: %s", scenePath.c_str());
				return false;
			}

			std::stringstream buffer;
			buffer << stream->rdbuf();
			return load_scene_from_string(buffer.str());
		}
		catch (const std::exception& e) {
			LOG_ERROR("[SceneLoader] Exception loading scene %s: %s", scenePath.c_str(), e.what());
			return false;
		}
	}

	bool SceneLoader::load_scene_from_string(const std::string& jsonContent)
	{
		LOG_INFO("[SceneLoader] Parsing scene JSON");

		JsonValue root = SimpleJsonParser::parse(jsonContent);
		if (root.is_null()) {
			LOG_ERROR("[SceneLoader] Failed to parse scene JSON");
			return false;
		}

		// Look for "scene" object
		if (!root.has("scene")) {
			LOG_ERROR("[SceneLoader] JSON missing 'scene' object");
			return false;
		}

		const JsonValue& sceneObj = root["scene"];
		if (!sceneObj.is_object()) {
			LOG_ERROR("[SceneLoader] 'scene' is not an object");
			return false;
		}

		// Clear previous scene
		unload_scene();

		// Parse the complete scene
		if (!parse_scene_json(sceneObj)) {
			LOG_ERROR("[SceneLoader] Failed to parse scene JSON");
			return false;
		}

		LOG_INFO("[SceneLoader] Scene loaded successfully: %s (%zu entities)", 
			m_metadata.scene_name.c_str(), m_loadedEntities.size());
		return true;
	}

	bool SceneLoader::parse_scene_json(const JsonValue& sceneObj)
	{
		// Parse metadata (name, difficulty, etc.)
		if (!parse_metadata(sceneObj)) {
			return false;
		}

		// Parse spawn points
		if (sceneObj.has("spawn_points")) {
			if (!parse_spawn_points(sceneObj["spawn_points"])) {
				LOG_WARNING("[SceneLoader] Failed to parse some spawn points");
			}
		}

		// Parse entities (using AssetLoader logic)
		if (!sceneObj.has("entities")) {
			LOG_ERROR("[SceneLoader] Scene missing 'entities' object");
			return false;
		}

		if (!parse_entities(sceneObj["entities"])) {
			LOG_ERROR("[SceneLoader] Failed to parse scene entities");
			return false;
		}

		return true;
	}

	bool SceneLoader::parse_metadata(const JsonValue& sceneObj)
	{
		if (sceneObj.has("name")) {
			m_metadata.scene_name = sceneObj["name"].as_string("Unnamed Scene");
		}

		if (sceneObj.has("difficulty")) {
			m_metadata.difficulty = sceneObj["difficulty"].as_int(1);
		}

		if (sceneObj.has("expected_enemy_count")) {
			m_metadata.expected_enemy_count = sceneObj["expected_enemy_count"].as_int(0);
		}

		return true;
	}

	bool SceneLoader::parse_spawn_points(const JsonValue& spawnPointsObj)
	{
		if (!spawnPointsObj.is_object()) {
			LOG_WARNING("[SceneLoader] spawn_points is not an object");
			return false;
		}

		auto spawnNames = spawnPointsObj.keys();
		for (const auto& spawnName : spawnNames) {
			const JsonValue& spawnObj = spawnPointsObj[spawnName];
			if (!spawnObj.is_object()) {
				LOG_WARNING("[SceneLoader] Spawn point '%s' is not an object", spawnName.c_str());
				continue;
			}

			SpawnPoint sp;
			sp.name = spawnName;
			sp.position = tmath::vector3df(
				spawnObj["x"].as_float(0.0f),
				spawnObj["y"].as_float(0.0f),
				spawnObj["z"].as_float(0.0f)
			);

			m_metadata.spawn_points[spawnName] = sp;
			LOG_INFO("[SceneLoader] Spawn point registered: %s (%.1f, %.1f, %.1f)",
				spawnName.c_str(), sp.position.x, sp.position.y, sp.position.z);
		}

		return true;
	}

	bool SceneLoader::parse_entities(const JsonValue& entitiesObj)
	{
		if (!entitiesObj.is_object()) {
			LOG_ERROR("[SceneLoader] 'entities' is not an object");
			return false;
		}

		if (!m_engine) {
			LOG_ERROR("[SceneLoader] Engine is null, cannot load entities");
			return false;
		}

		// Use AssetLoader to load all entities
		AssetLoader assetLoader(m_engine);
		
		// Create a temporary JSON wrapper for AssetLoader
		std::string entitiesJson = SimpleJsonParser::stringify(entitiesObj);
		std::string fullJson = "{\"entities\":" + entitiesJson + "}";

		size_t loaded = assetLoader.load_from_string(fullJson);
		if (loaded == 0) {
			LOG_WARNING("[SceneLoader] No entities loaded from scene");
			return false;
		}

		// Transfer loaded entities to our list
		for (const auto& handle : assetLoader.get_loaded_entity_handles()) {
			m_loadedEntities.push_back(handle);
		}

		LOG_INFO("[SceneLoader] Loaded %zu entities for scene", m_loadedEntities.size());
		return true;
	}

	SceneLoader::SpawnPoint SceneLoader::get_spawn_point(const std::string& name) const
	{
		auto it = m_metadata.spawn_points.find(name);
		if (it != m_metadata.spawn_points.end()) {
			return it->second;
		}
		
		// Return default spawn point at origin if not found
		LOG_WARNING("[SceneLoader] Spawn point '%s' not found in scene", name.c_str());
		return SceneLoader::SpawnPoint{"", tmath::vector3df(0, 0, 0)};
	}

	EntityHandle SceneLoader::get_entity_handle(const std::string& descriptor) const
	{
		for (const auto& handle : m_loadedEntities) {
			auto* entity = resolve_entity(handle);
			if (entity && entity->get_descriptor() == descriptor) {
				return handle;
			}
		}
		return {};
	}

	GenericEntity* SceneLoader::get_entity(const std::string& descriptor)
	{
		return resolve_entity(get_entity_handle(descriptor));
	}

	const std::vector<GenericEntity*>& SceneLoader::get_loaded_entities() const
	{
		m_loadedEntityCache.clear();
		m_loadedEntityCache.reserve(m_loadedEntities.size());

		for (const auto& handle : m_loadedEntities)
		{
			if (auto* entity = resolve_entity(handle))
			{
				m_loadedEntityCache.push_back(entity);
			}
		}

		return m_loadedEntityCache;
	}

	bool SceneLoader::validate_required_entities(const std::vector<std::string>& required_entities) const
	{
		for (const auto& requiredName : required_entities) {
			bool found = get_entity_handle(requiredName).is_valid();
			
			if (!found) {
				LOG_ERROR("[SceneLoader] Required entity not found in scene: %s", requiredName.c_str());
				return false;
			}
		}

		LOG_INFO("[SceneLoader] All required entities validated successfully");
		return true;
	}

	void SceneLoader::unload_scene()
	{
		m_loadedEntities.clear();
		m_loadedEntityCache.clear();
		m_metadata.spawn_points.clear();
		m_metadata.scene_name = "";
		m_metadata.difficulty = 1;
		m_metadata.expected_enemy_count = 0;
	}
}
