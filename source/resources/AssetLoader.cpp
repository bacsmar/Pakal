#include "AssetLoader.h"
#include "Engine.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "PakalMath.h"
#include "components/SpritePhysicsComponent.h"
#include "components/SpriteComponent.h"
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include <fstream>
#include <sstream>

namespace Pakal
{
	AssetLoader::AssetLoader(Engine* engine)
		: m_engine(engine),
		  m_entityManager(nullptr),
		  m_componentManager(nullptr)
	{
		if (engine) {
			m_entityManager = engine->entity_manager();
			m_componentManager = engine->component_manager();
		}
		if (!m_engine || !m_entityManager || !m_componentManager) {
			LOG_ERROR("[AssetLoader] Engine, EntityManager, or ComponentManager is null!");
		}
	}

	AssetLoader::~AssetLoader()
	{
		// Note: We don't delete entities as they should be managed by EntityManager
	}

	size_t AssetLoader::load_from_file(const std::string& filePath)
	{
		LOG_INFO("[AssetLoader] Loading assets from file: %s", filePath.c_str());

		try {
			auto stream = ResourceMgr.open_read_resource(filePath);
			if (!stream) {
				LOG_ERROR("[AssetLoader] Failed to open file: %s", filePath.c_str());
				return 0;
			}

			std::stringstream buffer;
			buffer << stream->rdbuf();
			return load_from_string(buffer.str());
		}
		catch (const std::exception& e) {
			LOG_ERROR("[AssetLoader] Exception loading file %s: %s", filePath.c_str(), e.what());
			return 0;
		}
	}

	size_t AssetLoader::load_from_string(const std::string& jsonContent)
	{
		LOG_INFO("[AssetLoader] Parsing JSON asset definitions");

		JsonValue root = SimpleJsonParser::parse(jsonContent);
		if (root.is_null()) {
			LOG_ERROR("[AssetLoader] Failed to parse JSON");
			return 0;
		}

		size_t loadedCount = 0;

		// Look for "entities" object
		if (!root.has("entities")) {
			LOG_ERROR("[AssetLoader] JSON missing 'entities' object");
			return 0;
		}

		const JsonValue& entitiesObj = root["entities"];
		if (!entitiesObj.is_object()) {
			LOG_ERROR("[AssetLoader] 'entities' is not an object");
			return 0;
		}

		auto entityNames = entitiesObj.keys();
		for (const auto& entityName : entityNames) {
			if (create_entity_from_json(entityName, entitiesObj[entityName])) {
				loadedCount++;
				LOG_INFO("[AssetLoader] Created entity: %s", entityName.c_str());
			}
			else {
				LOG_WARNING("[AssetLoader] Failed to create entity: %s", entityName.c_str());
			}
		}

		LOG_INFO("[AssetLoader] Loaded %zu entities", loadedCount);
		return loadedCount;
	}

	GenericEntity* AssetLoader::get_entity(const std::string& descriptor)
	{
		for (auto entity : m_loadedEntities) {
			if (entity && entity->get_descriptor() == descriptor) {
				return entity;
			}
		}
		return nullptr;
	}

	void AssetLoader::clear()
	{
		m_loadedEntities.clear();
	}

	bool AssetLoader::create_entity_from_json(const std::string& entityName, const JsonValue& entityDef)
	{
		if (!m_entityManager) return false;

		// Get entity type (default to GenericEntity)
		std::string entityType = "Pakal::GenericEntity";
		if (entityDef.has("type")) {
			entityType = entityDef["type"].as_string();
		}

		// Create the entity
		GenericEntity* entity = dynamic_cast<GenericEntity*>(
			m_entityManager->create_entity(entityType, entityName)
		);

		if (!entity) {
			LOG_ERROR("[AssetLoader] Failed to create entity instance: %s", entityName.c_str());
			return false;
		}

// Note: Position should be set via component initialization, not at entity level

		// Create components
		if (entityDef.has("components")) {
			const JsonValue& componentsArray = entityDef["components"];
			if (componentsArray.is_array()) {
				for (size_t i = 0; i < componentsArray.size(); i++) {
					if (!create_component_from_json(entity, componentsArray[i])) {
						LOG_WARNING("[AssetLoader] Failed to create component %zu for entity %s", 
							i, entityName.c_str());
					}
				}
			}
		}

		m_loadedEntities.push_back(entity);
		return true;
	}

	bool AssetLoader::create_component_from_json(GenericEntity* entity, const JsonValue& componentDef)
	{
		if (!entity || !componentDef.is_object()) {
			return false;
		}

		if (!componentDef.has("type")) {
			LOG_ERROR("[AssetLoader] Component definition missing 'type' field");
			return false;
		}

		std::string componentType = componentDef["type"].as_string();

		if (componentType == "SpritePhysicsComponent") {
			return create_sprite_physics_component(entity, componentDef);
		}
		else if (componentType == "SpriteComponent") {
			return create_sprite_component(entity, componentDef);
		}
		else if (componentType == "SpriteComponent2D") {
			return create_sprite_2d_component(entity, componentDef);
		}
		else if (componentType == "CameraComponent2D") {
			return create_camera_component(entity, componentDef);
		}
		else {
			LOG_WARNING("[AssetLoader] Unknown component type: %s", componentType.c_str());
			return false;
		}
	}

	bool AssetLoader::create_sprite_physics_component(GenericEntity* entity, const JsonValue& data)
	{
		LOG_INFO("[AssetLoader] Creating SpritePhysicsComponent");

		if (!entity) return false;

		auto* component = entity->create_component<SpritePhysicsComponent>();
		if (!component) {
			LOG_ERROR("[AssetLoader] Failed to create SpritePhysicsComponent");
			return false;
		}

		// Create basic physics settings
		// In a full implementation, this would load from physics descriptor file
		SpritePhysicsComponent::Settings settings;

		// TODO: Load sprite_physics from file specified in data
		// For now, caller (ContraGame) will set up physics separately
		// This is just the placeholder architecture

		if (data.has("position")) {
			const JsonValue& posObj = data["position"];
			if (posObj.is_object()) {
				float x = posObj["x"].as_float(0.0f);
				float y = posObj["y"].as_float(0.0f);
				float z = posObj["z"].as_float(0.0f);
				settings.position = {x, y, z};
			}
		}

		if (data.has("scale")) {
			settings.scale = data["scale"].as_float(1.0f);
		}

		if (data.has("body_type")) {
			std::string bodyTypeStr = data["body_type"].as_string("dynamic");
			if (bodyTypeStr == "static") {
				settings.body_type = SpritePhysicsComponent::StaticBody;
			}
			else {
				settings.body_type = SpritePhysicsComponent::DynamicBody;
			}
		}

		if (data.has("fixed_rotation")) {
			settings.fixed_rotation = data["fixed_rotation"].as_bool(false);
		}

		if (data.has("gravity_scale")) {
			settings.gravity_scale = data["gravity_scale"].as_float(1.0f);
		}

		// Initialize with empty physics for now (caller will populate)
		// This ensures the component exists and can be referenced
		if (!settings.sprite_physics) {
			auto sheet = std::make_shared<SpriteSheetPhysics>();
			settings.sprite_physics = sheet;
		}

		component->initialize(settings);
		return true;
	}

	bool AssetLoader::create_sprite_component(GenericEntity* entity, const JsonValue& data)
	{
		LOG_INFO("[AssetLoader] Creating SpriteComponent");

		if (!entity) return false;

		auto* component = entity->create_component<SpriteComponent>();
		if (!component) {
			LOG_ERROR("[AssetLoader] Failed to create SpriteComponent");
			return false;
		}

		SpriteComponent::Settings settings;

		// TODO: Load sprite sheet from file
		if (data.has("sprite_sheet_file")) {
			// settings.sprite_sheet = load sprite sheet...
		}

		if (data.has("position")) {
			const JsonValue& posObj = data["position"];
			if (posObj.is_object()) {
				float x = posObj["x"].as_float(0.0f);
				float y = posObj["y"].as_float(0.0f);
				float z = posObj["z"].as_float(0.0f);
				settings.position = {x, y, z};
			}
		}

		if (data.has("size")) {
			settings.size = data["size"].as_float(1.0f);
		}

		component->initialize(settings);
		return true;
	}

	bool AssetLoader::create_sprite_2d_component(GenericEntity* entity, const JsonValue& data)
	{
		LOG_INFO("[AssetLoader] Creating SpriteComponent2D");

		if (!entity) return false;

		auto* component = entity->create_component<SpriteComponent2D>();
		if (!component) {
			LOG_ERROR("[AssetLoader] Failed to create SpriteComponent2D");
			return false;
		}

		if (data.has("texture")) {
			std::string texturePath = data["texture"].as_string();
			component->set_texture(texturePath);
		}

		if (data.has("position")) {
			const JsonValue& posObj = data["position"];
			if (posObj.is_object()) {
				float x = posObj["x"].as_float(0.0f);
				float y = posObj["y"].as_float(0.0f);
				component->set_position(x, y);
			}
		}

		if (data.has("scale")) {
			const JsonValue& scaleObj = data["scale"];
			if (scaleObj.is_object() || scaleObj.is_number()) {
				float sx = scaleObj.is_object() ? scaleObj["x"].as_float(1.0f) : scaleObj.as_float(1.0f);
				float sy = scaleObj.is_object() ? scaleObj["y"].as_float(1.0f) : scaleObj.as_float(1.0f);
				component->set_scale(sx, sy);
			}
		}

		if (data.has("color")) {
			const JsonValue& colorObj = data["color"];
			if (colorObj.is_object()) {
				float r = colorObj["r"].as_float(1.0f);
				float g = colorObj["g"].as_float(1.0f);
				float b = colorObj["b"].as_float(1.0f);
				float a = colorObj["a"].as_float(1.0f);
				component->set_color(r, g, b, a);
			}
		}

		if (data.has("layer")) {
			int layer = data["layer"].as_int(0);
			component->set_layer(layer);
		}

		return true;
	}

	bool AssetLoader::create_camera_component(GenericEntity* entity, const JsonValue& data)
	{
		LOG_INFO("[AssetLoader] Creating CameraComponent2D");

		if (!entity) return false;

		auto* component = entity->create_component<CameraComponent2D>();
		if (!component) {
			LOG_ERROR("[AssetLoader] Failed to create CameraComponent2D");
			return false;
		}

		// Set camera properties if specified
		if (data.has("viewport_width")) {
			// Settings would be applied here if component allows it
		}

		return true;
	}
}
