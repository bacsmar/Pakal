#pragma once

#include "PhysicsSystem.h"

namespace Pakal
{
	class _PAKALExport DummyPhysicsSystem : public PhysicsSystem
	{
	public:
		explicit DummyPhysicsSystem(const Settings& settings);
		~DummyPhysicsSystem() override = default;

		const char* get_system_name() override { return "DummyPhysicsSystem"; }
		void set_gravity(const tmath::vector3df& gravity) override {}
		void register_component_factories(std::vector<IComponentFactory*>& factories) override {}

	protected:
		void init_world() override {}
		void update_world(unsigned long dt) override {}
		void clear_world() override {}
		void pause_world() override {}
		void resume_world() override {}
	};
}
