///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SyncEntity.h
// Purpose: The synchronous entity model: entities whose components a SimWorld updates once per fixed
//          step, in the order they were added, on one thread. It is the model for a game's simulation
//          (the same result whatever the frame rate, for replays, bots and networking). The engine's
//          other model, AsyncEntity, has its components made and updated by the systems on their own
//          threads; the simulation never reads it. See docs/ARCHITECTURE.md.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <box2d/b2_math.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Pakal
{
	class SimWorld;
	class SyncEntity;

	class SyncComponent
	{
	public:
		virtual ~SyncComponent() = default;

		SyncEntity& entity() const { return *m_entity; }

		// Called once per fixed step, in the order the components were added
		virtual void update(SimWorld& world, float dt) { (void)world; (void)dt; }

	private:
		friend class SyncEntity;
		SyncEntity* m_entity = nullptr;
	};

	class SyncEntity
	{
	public:
		// 'kind' is the game's (what sort of thing it is); 'id' is unique in its SimWorld and never 0
		SyncEntity(int kind, std::string name, uintptr_t id) : kind(kind), name(std::move(name)), id(id) {}
		SyncEntity(const SyncEntity&) = delete;
		SyncEntity& operator=(const SyncEntity&) = delete;

		template <class T, class... Args>
		T& add(Args&&... args)
		{
			auto component = std::make_unique<T>(std::forward<Args>(args)...);
			component->m_entity = this;
			T& added = *component;
			m_components.push_back(std::move(component));
			m_found.clear();
			return added;
		}

		// The first component of type T (or derived from it), or nullptr. The answer is kept for the next time (until a
		// component is added), so asking each step costs a short search of the types asked for, not a cast of each
		// component. Like the rest of the entity, only one thread at a time may use it.
		template <class T>
		T* get() const
		{
			const void* key = type_key<T>();
			for (const auto& found : m_found)
			{
				if (found.key == key) return static_cast<T*>(found.component);
			}
			T* component = nullptr;
			for (const auto& candidate : m_components)
			{
				if ((component = dynamic_cast<T*>(candidate.get()))) break;
			}
			m_found.push_back({ key, component });
			return component;
		}

		void update(SimWorld& world, float dt)
		{
			for (auto& component : m_components)
			{
				component->update(world, dt);
			}
		}

		const int kind;
		const std::string name;
		const uintptr_t id;      // physics user data of its body: ray casts report it
		int gameId = -1;         // the game's object id, when it has one
		b2Vec2 position = b2Vec2(0.0f, 0.0f); // meters, on the simulation's plane
		b2Vec2 previousPosition = b2Vec2(0.0f, 0.0f); // where it was when the last simulation step began

		// Where to draw it 'alpha' (0..1) of the way from the last step to the next one: the frame falls between two
		// steps of the fixed-step simulation. A jump (a warp, a respawn) is not smoothed.
		b2Vec2 drawn_position(float alpha) const
		{
			const b2Vec2 moved = position - previousPosition;
			if (moved.LengthSquared() > 4.0f) return position;
			return previousPosition + alpha * moved;
		}
		float yaw = 0.0f;        // rotation about the vertical axis
		bool removed = false;    // taken out of the world at the end of the step

	private:
		// A key per type (per module: a type asked for from two modules just has two entries)
		template <class T>
		static const void* type_key()
		{
			static const char key = 0;
			return &key;
		}

		struct Found
		{
			const void* key;
			void* component;   // a T*, or nullptr: it has none
		};

		std::vector<std::unique_ptr<SyncComponent>> m_components;
		mutable std::vector<Found> m_found;   // what get() answered, by type
	};
}
