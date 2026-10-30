///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelObjects.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/LevelObjects.h"

#include "bgfx/MeshComponent3D_Bgfx.h"
#include "topdown/Actor.h"
#include "topdown/TopDownComponents.h"
#include "topdown/TopDownGame.h"
#include "topdown/view/PropMesh.h"

#include <algorithm>
#include <set>

namespace Pakal::TopDown
{
	using Pakal::Json;
	using Pakal::LevelObject;

	namespace
	{
		bool same_placement(const LevelObject& a, const LevelObject& b)
		{
			return a.x == b.x && a.y == b.y && a.z == b.z && a.rot == b.rot && a.scale == b.scale && a.lightmap == b.lightmap && a.baked == b.baked;
		}

		std::string key_of(const LevelObject& object, size_t index)
		{
			return object.key.empty() ? "#" + std::to_string(index) : object.key;
		}
	}

	void LevelObjects::setup(std::string levelDir, double unitsPerMeter, Pakal::PrefabLibrary* prefabs, const ComponentSchema* schema, MeshFactory factory)
	{
		m_levelDir = std::move(levelDir);
		m_unitsPerMeter = unitsPerMeter;
		m_prefabs = prefabs;
		m_schema = schema;
		m_factory = std::move(factory);
	}

	void LevelObjects::footprint(const Pakal::MeshComponent3D_Bgfx& mesh, const Mat4& placement, b2Vec2& low, b2Vec2& high, float& size) const
	{
		float lo[3], hi[3];
		mesh.get_bounds(lo, hi);
		low = b2Vec2(1e9f, 1e9f);
		high = b2Vec2(-1e9f, -1e9f);
		for (int corner = 0; corner < 8; ++corner)
		{
			const float px = (corner & 1) ? hi[0] : lo[0], py = (corner & 2) ? hi[1] : lo[1], pz = (corner & 4) ? hi[2] : lo[2];
			const float wx = placement[0] * px + placement[4] * py + placement[8] * pz + placement[12];
			const float wz = placement[2] * px + placement[6] * py + placement[10] * pz + placement[14];
			// level x, y: render x, -z
			low = b2Vec2(std::min(low.x, wx), std::min(low.y, -wz));
			high = b2Vec2(std::max(high.x, wx), std::max(high.y, -wz));
		}
		size = std::clamp(std::max(high.x - low.x, high.y - low.y), 0.3f, 6.0f);
	}

	LevelObjects::Instance LevelObjects::make(size_t index, const LevelObject& object, TopDownGame* game, const Handle& handle)
	{
		Instance instance;
		instance.key = key_of(object, index);
		std::string error;
		if (m_prefabs) instance.resolved = m_prefabs->resolve(object, error);
		else instance.resolved = Json{ { "mesh", object.mesh }, { "role", object.role }, { "components", object.overrides.value("components", Json::object()) } };
		const std::string mesh = instance.resolved.is_object() ? instance.resolved.value("mesh", std::string()) : std::string();
		if (!error.empty() || mesh.empty() || !m_factory)
		{
			++m_failed;
			return instance;
		}
		instance.mesh = m_factory(m_levelDir + mesh);
		if (!instance.mesh || !instance.mesh->is_loaded())
		{
			instance.mesh.reset();
			++m_failed;
			return instance;
		}
		const Mat4 placement = level_placement(object, m_unitsPerMeter);
		instance.mesh->set_matrix(placement.data());
		if (object.lightmap[0] > 0.0)
		{
			instance.mesh->set_lightmap_rect(float(object.lightmap[0]), float(object.lightmap[1]), float(object.lightmap[2]));
		}
		instance.mesh->set_shadow_baked(object.baked);

		uintptr_t actor = 0;
		if (handle && handle(index, object, instance.resolved, *instance.mesh, placement, actor))
		{
			instance.actor = actor;
			instance.handled = true;
			return instance;
		}
		const Json components = instance.resolved.value("components", Json::object());
		if (!game || !m_schema || components.empty()) return instance;
		Actor& prop = game->add_actor(ActorKind::Prop, object.name);
		prop.gameId = object.id;
		prop.position = b2Vec2(static_cast<float>(object.x / m_unitsPerMeter), static_cast<float>(object.y / m_unitsPerMeter));
		prop.add<PropComponent>(std::make_unique<PropMesh>(instance.mesh.get(), placement));
		PlacedObject placed{ *game, prop, object, instance.resolved };
		footprint(*instance.mesh, placement, placed.low, placed.high, placed.size);
		for (const auto& name : m_schema->add_components(placed)) m_unknown.push_back(instance.key + ": " + name);
		instance.actor = prop.id;
		instance.ownActor = true;
		return instance;
	}

	void LevelObjects::release(Instance& instance, TopDownGame* game)
	{
		if (instance.ownActor && game)
		{
			if (auto* actor = game->find(instance.actor)) actor->removed = true;
		}
		instance.actor = 0;
		instance.mesh.reset();
	}

	void LevelObjects::build(const Pakal::LevelDocument& document, TopDownGame* game, const Handle& handle)
	{
		clear(game);
		m_instances.reserve(document.objects.size());
		for (size_t index = 0; index < document.objects.size(); ++index)
		{
			const LevelObject& object = document.objects[index];
			m_instances.push_back(make(index, object, game, handle));
			m_built[m_instances.back().key] = object;
		}
	}

	bool LevelObjects::update(const Pakal::LevelDocument& document, TopDownGame* game, const Handle& handle)
	{
		std::map<std::string, Instance> old;
		for (auto& instance : m_instances) old[instance.key] = std::move(instance);
		std::vector<Instance> next;
		next.reserve(document.objects.size());
		std::set<std::string> seen;
		bool inPlace = true;
		m_notInPlace.clear();
		for (size_t index = 0; index < document.objects.size(); ++index)
		{
			const LevelObject& object = document.objects[index];
			const std::string key = key_of(object, index);
			seen.insert(key);
			auto found = old.find(key);
			auto built = m_built.find(key);
			if (found == old.end() || built == m_built.end())
			{
				next.push_back(make(index, object, game, handle));
				m_built[key] = object;
				continue;
			}
			Instance instance = std::move(found->second);
			old.erase(found);
			std::string error;
			const Json resolved = m_prefabs ? m_prefabs->resolve(object, error) : instance.resolved;
			const bool sameThing = resolved == instance.resolved && built->second.id == object.id && built->second.name == object.name;
			if (sameThing)
			{
				if (!same_placement(built->second, object)) place(instance, object, game);
				built->second = object;
				next.push_back(std::move(instance));
				continue;
			}
			// what it is changed: made again (the game's own objects cannot be)
			if (instance.handled)
			{
				inPlace = false;
				m_notInPlace = key + " changed";
			}
			release(instance, game);
			next.push_back(make(index, object, game, handle));
			built->second = object;
		}
		for (auto& [key, instance] : old)
		{
			if (instance.handled)
			{
				inPlace = false;
				m_notInPlace = key + " was removed";
			}
			release(instance, game);
			m_built.erase(key);
		}
		m_instances = std::move(next);
		return inPlace;
	}

	void LevelObjects::place(Instance& instance, const LevelObject& object, TopDownGame* game)
	{
		if (!instance.mesh) return;
		const Mat4 placement = level_placement(object, m_unitsPerMeter);
		instance.mesh->set_matrix(placement.data());
		if (object.lightmap[0] > 0.0)
		{
			instance.mesh->set_lightmap_rect(float(object.lightmap[0]), float(object.lightmap[1]), float(object.lightmap[2]));
		}
		instance.mesh->set_shadow_baked(object.baked);
		Actor* actor = game && instance.actor ? game->find(instance.actor) : nullptr;
		if (!actor) return;
		actor->position = actor->previousPosition = b2Vec2(static_cast<float>(object.x / m_unitsPerMeter), static_cast<float>(object.y / m_unitsPerMeter));
		if (auto* prop = actor->get<PropComponent>()) prop->set_placement(placement);
		if (auto* breakable = actor->get<DestructibleComponent>())
		{
			b2Vec2 low, high;
			float size = 1.0f;
			footprint(*instance.mesh, placement, low, high, size);
			breakable->set_box(low, high);
			breakable->set_size(size);
		}
	}

	LevelObjects::Instance* LevelObjects::find(const std::string& key)
	{
		auto found = std::find_if(m_instances.begin(), m_instances.end(), [&](const Instance& i) { return i.key == key; });
		return found != m_instances.end() ? &*found : nullptr;
	}

	void LevelObjects::clear(TopDownGame* game)
	{
		for (auto& instance : m_instances) release(instance, game);
		m_instances.clear();
		m_built.clear();
		m_unknown.clear();
		m_failed = 0;
	}
}
