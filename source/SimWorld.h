///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SimWorld.h
// Purpose: The world of a fixed-step simulation: it owns the SyncEntities, gives them stable ids, and
//          lets a game run its step in order. An entity made during a step joins the others between
//          steps (lists may be walked meanwhile); one marked 'removed' leaves at the end of the step.
//          A game derives from it (TopDownGame) and writes its own step with the helpers below.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "SyncEntity.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class SimWorld
	{
	public:
		SimWorld() = default;
		SimWorld(const SimWorld&) = delete;
		SimWorld& operator=(const SimWorld&) = delete;
		// The game calls remove_all() in its own destructor (onRemove may still need it)
		virtual ~SimWorld() = default;

		// A new entity: it joins the others at the next join_added()
		SyncEntity& add_entity(int kind, const std::string& name)
		{
			auto entity = std::make_unique<SyncEntity>(kind, name, m_nextId++);
			SyncEntity& added = *entity;
			m_byId[added.id] = &added;
			m_added.push_back(std::move(entity));
			return added;
		}

		// The entity with that id (also one not joined yet), or nullptr
		SyncEntity* find(uintptr_t id) const
		{
			auto found = m_byId.find(id);
			return found != m_byId.end() ? found->second : nullptr;
		}

		const std::vector<std::unique_ptr<SyncEntity>>& entities() const { return m_entities; }

		// Called for each entity right before it is deleted (its model can go back to a pool)
		std::function<void(SyncEntity&)> onRemove;

	protected:
		// The ones made since the last join (not stepped yet)
		const std::vector<std::unique_ptr<SyncEntity>>& added() const { return m_added; }

		void join_added()
		{
			for (auto& entity : m_added)
			{
				entity->previousPosition = entity->position; // drawn where it appears, not slid in from the origin
				m_entities.push_back(std::move(entity));
			}
			m_added.clear();
		}

		// Where each one was when this step began (for drawing between steps)
		void remember_positions()
		{
			for (auto& entity : m_entities)
			{
				entity->previousPosition = entity->position;
			}
		}

		// Each entity not removed, in the order they joined
		void update_entities(float dt)
		{
			for (auto& entity : m_entities)
			{
				if (!entity->removed)
				{
					entity->update(*this, dt);
				}
			}
		}

		// The ones marked 'removed' leave: onRemove, then entity_removed(), then they are deleted
		void remove_removed()
		{
			for (auto& entity : m_entities)
			{
				if (entity->removed)
				{
					if (onRemove) onRemove(*entity);
					m_byId.erase(entity->id);
					entity_removed(*entity);
				}
			}
			m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
				[](const std::unique_ptr<SyncEntity>& entity) { return entity->removed; }), m_entities.end());
		}

		// Every entity leaves (the end of the game): onRemove for each, then they are deleted
		void remove_all()
		{
			for (auto& entity : m_entities)
			{
				if (onRemove) onRemove(*entity);
			}
			m_entities.clear();
			m_added.clear();
			m_byId.clear();
		}

		// A game forgets what it kept about an entity that leaves
		virtual void entity_removed(SyncEntity& entity) { (void)entity; }

	private:
		std::vector<std::unique_ptr<SyncEntity>> m_entities;
		std::vector<std::unique_ptr<SyncEntity>> m_added; // made since the last join (see add_entity)
		std::map<uintptr_t, SyncEntity*> m_byId;
		uintptr_t m_nextId = 1;
	};
}
