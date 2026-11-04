///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelObjects.h
// Purpose: The objects of a level document in the running level, by key: each one's model (from its
//          prefab or its own mesh, placed where the document says) and, when it carries components, its
//          actor (a prop with them, ComponentSchema). Built once from the document; then changed one
//          object at a time as the document is edited (moved, turned, scaled, added, removed, its
//          prefab or overrides changed) without building the level again. The game can handle an object
//          itself first (its occluders, its props by game id). Graphics thread, under the game's lock.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"
#include "PrefabLibrary.h"
#include "topdown/ComponentSchema.h"
#include "topdown/view/LevelPlacement.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class TopDownGame;

	class LevelObjects
	{
	public:
		// A model file (a path the level's folder starts) as a new instance; nullptr or not loaded when it fails
		using MeshFactory = std::function<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>(const std::string& file)>;
		// The game's own handling of an object, after its model is placed: true when it made the object's actor itself (its
		// components are then not added). 'actor' is set to the actor it made, if any.
		using Handle = std::function<bool(size_t index, const Pakal::LevelObject& object, const Pakal::Json& resolved,
			Pakal::MeshComponent3D_Bgfx& mesh, const Mat4& placement, uintptr_t& actor)>;

		struct Instance
		{
			std::string key;
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			uintptr_t actor = 0;      // the actor made for it (0: scenery only)
			bool ownActor = false;    // made here (from its components), not by the game's Handle
			bool handled = false;     // the game's Handle took it (it may keep its own records of it: an occluder, a prop by id)
			Pakal::Json resolved;     // what it was built from (a change of it rebuilds the object)
		};

		void setup(std::string levelDir, double unitsPerMeter, Pakal::PrefabLibrary* prefabs, const ComponentSchema* schema, MeshFactory factory);

		// Every object of the document. 'game' null: models only (an editor's view without a simulation).
		void build(const Pakal::LevelDocument& document, TopDownGame* game, const Handle& handle = {});
		// The document changed (an edit): what changed is brought in line, object by object. False when it cannot be done
		// in place (an object the game handled itself was removed or changed beyond its placement): build again.
		bool update(const Pakal::LevelDocument& document, TopDownGame* game, const Handle& handle = {});
		// Lets every model and actor made here go
		void clear(TopDownGame* game);

		const std::vector<Instance>& instances() const { return m_instances; }
		Instance* find(const std::string& key);
		// The model of the document's object 'index' (as built), or nullptr
		Pakal::MeshComponent3D_Bgfx* mesh_at(size_t index) const { return index < m_instances.size() ? m_instances[index].mesh.get() : nullptr; }
		size_t failed() const { return m_failed; }
		// Why the last update could not be done in place ("" when it could)
		const std::string& not_in_place() const { return m_notInPlace; }
		// Objects whose components the schema does not know, as "key: Name"
		const std::vector<std::string>& unknown() const { return m_unknown; }

		// Moves an object's model (and its actor, its footprint) where 'object' says
		void place(Instance& instance, const Pakal::LevelObject& object, TopDownGame* game);

	private:
		Instance make(size_t index, const Pakal::LevelObject& object, TopDownGame* game, const Handle& handle);
		void release(Instance& instance, TopDownGame* game);
		// Its footprint on the floor (meters) and its size across, from its model's box through its placement
		void footprint(const Pakal::MeshComponent3D_Bgfx& mesh, const Mat4& placement, b2Vec2& low, b2Vec2& high, float& size) const;

		std::string m_levelDir;
		double m_unitsPerMeter = 100.0;
		Pakal::PrefabLibrary* m_prefabs = nullptr;
		const ComponentSchema* m_schema = nullptr;
		MeshFactory m_factory;
		std::vector<Instance> m_instances;   // in the document's order
		std::map<std::string, Pakal::LevelObject> m_built;   // each key's object as last built or placed
		std::vector<std::string> m_unknown;
		std::string m_notInPlace;
		size_t m_failed = 0;
	};
}
