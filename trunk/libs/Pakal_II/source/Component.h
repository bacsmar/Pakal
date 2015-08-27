#pragma once

#include "Config.h"
#include "IComponent.h"
#include "System.h"

namespace Pakal
{

	class _PAKALExport Component : public IComponent
	{
		IEntity* m_parent;

	protected:
		System* m_system;

	public:
		DECLARE_RTTI_WITH_BASE(Component,IComponent)

		explicit Component(System* owner) : m_parent(nullptr), m_system(owner) {}

		BasicTaskPtr initialize() override final;
		BasicTaskPtr destroy() override final;

		virtual void on_initialize() = 0;
		virtual void on_destroy() = 0;

		inline void set_parent_entity(IEntity* entity) override final { m_parent = entity; };
		inline IEntity* get_parent_entity() override { return m_parent; };
	};

}