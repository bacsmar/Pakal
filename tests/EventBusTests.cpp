///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EventBusTests.cpp
// Purpose: The event bus: events wait for deliver(), reach every listener of their type in the order
//          they were emitted, events nobody listens to are dropped, and listeners can come and go
//          while events are delivered.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EventBus.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
	struct Shot { int n; };
	struct Sound { std::string name; };
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	{
		Pakal::EventBus bus;
		std::vector<std::string> heard;
		bus.subscribe<Shot>([&](const Shot& shot) { heard.push_back("a" + std::to_string(shot.n)); });
		bus.subscribe<Shot>([&](const Shot& shot) { heard.push_back("b" + std::to_string(shot.n)); });
		bus.subscribe<Sound>([&](const Sound& sound) { heard.push_back(sound.name); });
		bus.emit(Shot{ 1 });
		bus.emit(Sound{ "bang" });
		bus.emit(Shot{ 2 });
		check(heard.empty() && bus.queued() == 3, "events wait for deliver()");
		bus.deliver();
		check((heard == std::vector<std::string>{ "a1", "b1", "bang", "a2", "b2" }), "every listener hears them, in the order emitted");
		check(bus.queued() == 0, "and the queue is empty after");
	}
	{
		Pakal::EventBus bus;
		bus.emit(Shot{ 1 });
		check(bus.queued() == 0 && !bus.listened_to<Shot>(), "an event nobody listens to is dropped");
	}
	{
		Pakal::EventBus bus;
		int shots = 0, sounds = 0;
		Pakal::EventBus::Id id = bus.subscribe<Shot>([&](const Shot&) { ++shots; });
		bus.subscribe<Shot>([&](const Shot&)
		{
			bus.unsubscribe(id);                                  // the other one stops listening...
			bus.emit(Sound{ "later" });                           // ... and something new happens
		});
		bus.subscribe<Sound>([&](const Sound&) { ++sounds; });
		bus.emit(Shot{ 1 });
		bus.emit(Shot{ 2 });
		bus.deliver();
		check(shots == 1, "a listener unsubscribed while delivering hears nothing more");
		check(sounds == 2, "events emitted while delivering are delivered too");
		check(bus.listened_to<Shot>(), "the other listener is still there");
	}
	{
		Pakal::EventBus bus;
		int late = 0;
		bus.subscribe<Shot>([&](const Shot&) { bus.subscribe<Sound>([&](const Sound&) { ++late; }); });
		bus.emit(Shot{ 1 });
		bus.deliver();
		bus.emit(Sound{ "x" });
		bus.deliver();
		check(late == 1, "a listener subscribed while delivering hears the next events");
		bus.emit(Sound{ "y" });
		bus.clear();
		bus.deliver();
		check(late == 1, "clear() drops what was not delivered");
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
