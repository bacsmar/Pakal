#include "AssetLoader.h"
#include "Engine.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "PakalMath.h"
#include "resources/prefab/PrefabCatalog.h"
#include "components/SpritePhysicsComponent.h"
#include "components/SpriteComponent.h"
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "components/SkeletalAnimationComponent.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>

namespace Pakal
{
	namespace
	{
		uint8_t color_channel_to_byte(float value)
		{
			const float clamped = std::max(0.0f, std::min(1.0f, value));
			return static_cast<uint8_t>(clamped * 255.0f);
		}

		uint32_t parse_rgba_color(const JsonValue& color, uint32_t defaultColor)
		{
			if (!color.is_object())
			{
				return defaultColor;
			}

			const auto r = color_channel_to_byte(color["r"].as_float(1.0f));
			const auto g = color_channel_to_byte(color["g"].as_float(1.0f));
			const auto b = color_channel_to_byte(color["b"].as_float(1.0f));
			const auto a = color_channel_to_byte(color["a"].as_float(1.0f));
			return (static_cast<uint32_t>(r) << 24) |
				(static_cast<uint32_t>(g) << 16) |
				(static_cast<uint32_t>(b) << 8) |
				static_cast<uint32_t>(a);
		}

		SpriteSheetPhysicsPtr create_box_physics(float width, float height, const SpritePhysicsComponent::Settings& settings, const JsonValue& data)
		{
			auto sheet = std::make_shared<SpriteSheetPhysics>();
			auto* body = new SpritePhysics();
			body->name = "body";
			body->dynamic = !settings.body_type.has_value() || settings.body_type.value() == SpritePhysicsComponent::DynamicBody;
			body->fixed_rotation = settings.fixed_rotation.value_or(false);
			body->gravity_scale = settings.gravity_scale.value_or(1.0f);

			SpritePhysics::Fixture fixture;
			fixture.type = "POLYGON";
			fixture.density = data.has("density") ? data["density"].as_float(1.0f) : 1.0f;
			fixture.friction = data.has("friction") ? data["friction"].as_float(0.0f) : 0.0f;
			fixture.restitution = data.has("restitution") ? data["restitution"].as_float(0.0f) : 0.0f;
			fixture.is_sensor = data.has("is_sensor") ? data["is_sensor"].as_bool(false) : false;

			const float halfWidth = width * 0.5f;
			const float halfHeight = height * 0.5f;
			SpritePhysics::Polygon polygon;
			polygon.m_vertices.emplace_back(-halfWidth, -halfHeight);
			polygon.m_vertices.emplace_back(halfWidth, -halfHeight);
			polygon.m_vertices.emplace_back(halfWidth, halfHeight);
			polygon.m_vertices.emplace_back(-halfWidth, halfHeight);
			fixture.m_polygons.emplace_back(std::move(polygon));

			body->m_fixtures.emplace_back(std::move(fixture));
			sheet->bodies.emplace_back(body);
			return sheet;
		}
	}

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

	GenericEntity* AssetLoader::resolve_entity(EntityHandle handle) const
	{
		if (!m_entityManager)
		{
			return nullptr;
		}

		return m_entityManager->resolve<GenericEntity>(handle);
	}


		EntityHandle AssetLoader::get_entity_handle(const std::string& descriptor) const
		{
			for (const auto& handle : m_loadedEntities) {
				auto* entity = resolve_entity(handle);
				if (entity && entity->get_descriptor() == descriptor) {
					return handle;
				}
			}
			return {};
		}

	GenericEntity* AssetLoader::get_entity(const std::string& descriptor)
	{
			return resolve_entity(get_entity_handle(descriptor));
		}

		const std::vector<GenericEntity*>& AssetLoader::get_loaded_entities() const
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

	void AssetLoader::clear()
	{
		m_loadedEntities.clear();
			m_loadedEntityCache.clear();
	}

	JsonValue AssetLoader::expand_entity(const std::string& name, const JsonValue& entityDef) const
	{
		if (!entityDef.has("prefab"))
			return entityDef; // inline path — return as-is

		if (!m_prefabResolver)
		{
			LOG_ERROR("[AssetLoader] Entity '%s' has 'prefab' but no resolver is set", name.c_str());
			return {};
		}

		const std::string prefabId = entityDef["prefab"].as_string();
		const JsonValue& overrides = entityDef.has("overrides") ? entityDef["overrides"] : JsonValue{};
		const JsonValue resolved = m_prefabResolver->resolve(prefabId, overrides);
		if (resolved.is_null())
		{
			LOG_ERROR("[AssetLoader] Failed to resolve prefab '%s' for entity '%s'",
					  prefabId.c_str(), name.c_str());
			return {};
		}

		// Convert prefab shape { entity_type, components:[{id,type,data}] }
		// to the inline shape { type, components:[{type, ...data fields...}] }
		// that the existing component creators expect.
		JsonValue concrete;
		concrete.set_object();

		const std::string entityType = resolved.has("entity_type")
			? resolved["entity_type"].as_string("Pakal::GenericEntity")
			: "Pakal::GenericEntity";
		concrete.insert("type", JsonValue(entityType));

		JsonValue flatComps;
		flatComps.set_array();

		// If the scene entity has a top-level "position" shorthand, inject it into
		// every component that already carries a "position" field in its data.
		// Convention: fields present in data take precedence; shorthand fills gaps only.
		const bool hasTopLevelPos = entityDef.has("position") && entityDef["position"].is_object();

		const JsonValue& comps = resolved["components"];
		for (size_t i = 0; i < comps.size(); ++i)
		{
			const JsonValue& comp = comps[i];
			JsonValue flat;
			flat.set_object();
			flat.insert("type", comp["type"]);
			// Promote all data fields to the component root level.
			if (comp.has("data") && comp["data"].is_object())
			{
				for (const auto& key : comp["data"].keys())
					flat.insert(key, comp["data"][key]);
			}
			// Apply top-level position shorthand if the component had a position
			// in its data and the scene entity provides a top-level position.
			if (hasTopLevelPos && !flat.has("position"))
				flat.insert("position", entityDef["position"]);
			flatComps.push_back(flat);
		}

		concrete.insert("components", flatComps);
		return concrete;
	}

	bool AssetLoader::create_entity_from_json(const std::string& entityName, const JsonValue& entityDefRaw)
	{
		if (!m_entityManager) return false;

		// Expand prefab if present; inline entities pass through unchanged.
		const JsonValue expanded = expand_entity(entityName, entityDefRaw);
		if (expanded.is_null()) return false;
		const JsonValue& entityDef = expanded;

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

		m_loadedEntities.push_back(entity->get_handle());
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
		else if (componentType == "SkeletalAnimationComponent") {
			return create_skeletal_animation_component(entity, componentDef);
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

		float width = data.has("width") ? data["width"].as_float(1.0f) : 1.0f;
		float height = data.has("height") ? data["height"].as_float(1.0f) : 1.0f;
		settings.sprite_physics = create_box_physics(width, height, settings, data);

		auto initializeTask = component->initialize(settings);
		if (initializeTask) {
			initializeTask->wait();
		}
		return true;
	}

	bool AssetLoader::create_skeletal_animation_component(GenericEntity* entity, const JsonValue& data)
	{
		LOG_INFO("[AssetLoader] Creating SkeletalAnimationComponent");

		if (!entity) return false;

		auto* component = entity->create_component<SkeletalAnimationComponent>();
		if (!component) {
			LOG_ERROR("[AssetLoader] Failed to create SkeletalAnimationComponent");
			return false;
		}

		const JsonValue& cfg = data.has("data") ? data["data"] : data;

		if (cfg.has("skeleton")) {
			component->set_skeleton_asset(cfg["skeleton"].as_string());
		}
		else if (cfg.has("skeleton_asset")) {
			component->set_skeleton_asset(cfg["skeleton_asset"].as_string());
		}
		else if (cfg.has("skeleton_file")) {
			component->set_skeleton_asset(cfg["skeleton_file"].as_string());
		}

		if (cfg.has("animation")) {
			component->set_animation(cfg["animation"].as_string());
		}
		else if (cfg.has("default_animation")) {
			component->set_animation(cfg["default_animation"].as_string());
		}
		else if (cfg.has("play_on_load")) {
			component->set_animation(cfg["play_on_load"].as_string());
		}

		if (cfg.has("skin")) {
			component->set_skin(cfg["skin"].as_string());
		}

		if (cfg.has("loop")) {
			component->set_looping(cfg["loop"].as_bool(true));
		}

		if (cfg.has("playback_speed")) {
			component->set_playback_speed(cfg["playback_speed"].as_float(1.0f));
		}

		if (cfg.has("time")) {
			component->set_time(cfg["time"].as_float(0.0f));
		}

		if (cfg.has("render_scale")) {
			component->set_render_scale(cfg["render_scale"].as_float(1.0f));
		}
		else if (cfg.has("scale")) {
			component->set_render_scale(cfg["scale"].as_float(1.0f));
		}

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

		const bool hasSolidColor = data.has("solid_color");
		if (hasSolidColor) {
			component->create_solid_color(parse_rgba_color(data["solid_color"], 0xFFFFFFFF), 1, 1);
		}
		else if (data.has("texture")) {
			const std::string texturePath = data["texture"].as_string();
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
		else if (data.has("width") || data.has("height")) {
			const float width = data.has("width") ? data["width"].as_float(1.0f) : 1.0f;
			const float height = data.has("height") ? data["height"].as_float(1.0f) : width;
			component->set_scale(width, height);
		}

		if (data.has("rotation")) {
			component->set_rotation(data["rotation"].as_float(0.0f));
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

		if (data.has("sprite_sheet")) {
			const JsonValue& ss = data["sprite_sheet"];
			const int fw = ss.has("frame_width")  ? ss["frame_width"].as_int(0)  : 0;
			const int fh = ss.has("frame_height") ? ss["frame_height"].as_int(0) : 0;
			if (fw > 0 && fh > 0) {
				component->set_frame_size(fw, fh);
			}
			if (ss.has("fps")) {
				component->set_animation_fps(ss["fps"].as_float(12.0f));
			}
			if (ss.has("start_frame") && ss.has("end_frame")) {
				const bool loop = ss.has("loop") ? ss["loop"].as_bool(true) : true;
				component->play_animation(ss["start_frame"].as_int(0), ss["end_frame"].as_int(0), loop);
			}
			else if (ss.has("frame")) {
				component->set_frame(ss["frame"].as_int(0));
			}
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

		// Set orthographic projection
		float orthoWidth = 1280.0f;
		float orthoHeight = 720.0f;
		float nearPlane = 0.1f;
		float farPlane = 100.0f;

		if (data.has("orthographic")) {
			const JsonValue& orthoObj = data["orthographic"];
			if (orthoObj.is_object()) {
				orthoWidth = orthoObj["width"].as_float(1280.0f);
				orthoHeight = orthoObj["height"].as_float(720.0f);
				nearPlane = orthoObj["near"].as_float(0.1f);
				farPlane = orthoObj["far"].as_float(100.0f);
			}
		}
		component->set_orthographic(orthoWidth, orthoHeight, nearPlane, farPlane);

		// Set viewport
		int viewportX = 0;
		int viewportY = 0;
		int viewportWidth = 1280;
		int viewportHeight = 720;

		if (data.has("viewport")) {
			const JsonValue& viewportObj = data["viewport"];
			if (viewportObj.is_object()) {
				viewportX = viewportObj["x"].as_int(0);
				viewportY = viewportObj["y"].as_int(0);
				viewportWidth = viewportObj["width"].as_int(1280);
				viewportHeight = viewportObj["height"].as_int(720);
			}
		}
		component->set_viewport(viewportX, viewportY, viewportWidth, viewportHeight);

		// Set initial position
		if (data.has("position")) {
			const JsonValue& posObj = data["position"];
			if (posObj.is_object()) {
				float x = posObj["x"].as_float(0.0f);
				float y = posObj["y"].as_float(0.0f);
				component->set_position(x, y);
			}
		}

		// Set zoom level
		if (data.has("zoom")) {
			component->set_zoom(data["zoom"].as_float(1.0f));
		}

		// Set camera bounds if specified
		if (data.has("bounds")) {
			const JsonValue& boundsObj = data["bounds"];
			if (boundsObj.is_object()) {
				float minX = boundsObj["minX"].as_float(0.0f);
				float minY = boundsObj["minY"].as_float(0.0f);
				float maxX = boundsObj["maxX"].as_float(0.0f);
				float maxY = boundsObj["maxY"].as_float(0.0f);
				component->set_bounds(minX, minY, maxX, maxY);
			}
		}

		return true;
	}
}
