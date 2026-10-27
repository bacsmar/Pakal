///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SimWorldTests.cpp
// Purpose: The synchronous entities and their world: ids, entities made during a step join after it,
//          components run in order, removed ones leave at the end of the step through onRemove.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SimWorld.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
	std::vector<std::string> g_log;

	// Writes its name in the log each step; the spawner also makes a new entity on its first step
	struct Recorder : Pakal::SyncComponent
	{
		explicit Recorder(std::string name) : name(std::move(name)) {}
		void update(Pakal::SimWorld& world, float dt) override
		{
			(void)world;
			(void)dt;
			g_log.push_back(name);
		}
		std::string name;
	};

	// Components found through a base type: get<Shape>() finds a Circle
	struct Shape : Pakal::SyncComponent {};
	struct Circle : Shape {};
	struct Other : Pakal::SyncComponent {};

	class TestWorld : public Pakal::SimWorld
	{
	public:
		~TestWorld() override { remove_all(); }
		void step(float dt)
		{
			join_added();
			remember_positions();
			update_entities(dt);
			join_added();
			remove_removed();
		}
		size_t pending() const { return added().size(); }
		std::vector<uintptr_t> forgotten;

	protected:
		void entity_removed(Pakal::SyncEntity& entity) override { forgotten.push_back(entity.id); }
	};
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	TestWorld world;
	std::vector<uintptr_t> removedByCallback;
	world.onRemove = [&](Pakal::SyncEntity& entity) { removedByCallback.push_back(entity.id); };

	Pakal::SyncEntity& a = world.add_entity(1, "a");
	Pakal::SyncEntity& b = world.add_entity(2, "b");
	a.add<Recorder>("a1");
	a.add<Recorder>("a2");
	b.add<Recorder>("b1");
	check(a.id != 0 && b.id != 0 && a.id != b.id, "ids are unique and never 0");
	check(world.find(a.id) == &a && world.entities().empty() && world.pending() == 2, "a new entity is found at once, and joins at the next step");
	check(a.get<Recorder>() != nullptr && a.get<Recorder>()->name == "a1", "get() gives the first component of the type");
	{
		Pakal::SyncEntity shapes(0, "shapes", 99);
		shapes.add<Other>();
		check(shapes.get<Shape>() == nullptr && shapes.get<Shape>() == nullptr, "get() of a type it does not have gives nullptr (also asked again)");
		Circle& circle = shapes.add<Circle>();
		check(shapes.get<Shape>() == &circle, "a component added later is found (get() forgets its answers)");
		check(shapes.get<Circle>() == &circle && shapes.get<Shape>() == &circle, "by its own type and by its base");
	}

	a.position = b2Vec2(3.0f, 4.0f);
	world.step(1.0f / 60.0f);
	check(g_log == std::vector<std::string>({ "a1", "a2", "b1" }), "components run in the order the entities joined and they were added");
	check(a.previousPosition.x == 3.0f && a.previousPosition.y == 4.0f, "an entity joins where it is (not slid in from the origin)");

	a.position = b2Vec2(5.0f, 4.0f);
	world.step(1.0f / 60.0f);
	a.position = b2Vec2(7.0f, 4.0f);
	const b2Vec2 half = a.drawn_position(0.5f);
	check(a.previousPosition.x == 5.0f && half.x == 6.0f, "drawn halfway between the last step and now");

	g_log.clear();
	const uintptr_t bId = b.id;
	b.removed = true;
	world.step(1.0f / 60.0f);
	check(g_log == std::vector<std::string>({ "a1", "a2" }), "a removed entity does not run");
	check(removedByCallback == std::vector<uintptr_t>({ bId }) && world.forgotten == std::vector<uintptr_t>({ bId }),
		"it leaves through onRemove and entity_removed");
	check(world.find(bId) == nullptr && world.entities().size() == 1, "and it is gone");

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
