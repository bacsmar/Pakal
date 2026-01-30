///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Basic example demonstrating Pakal Engine initialization
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Engine.h>
#include <IPakalApplication.h>
#include <OSManager.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace Pakal;

class BasicApplication : public IPakalApplication
{
public:
	const char* get_name() override
	{
		return "BasicApplication";
	}

	void start(Engine& engine) override
	{
		std::cout << "Application started!" << std::endl;

		// Terminate after a short delay to allow systems to finish initializing
		std::thread([&engine]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			engine.os_manager()->event_app_finished();
		}).detach();
	}

	void end(Engine& engine) override
	{
		std::cout << "Application ended!" << std::endl;
	}
};

int main(int argc, char* argv[])
{
	std::cout << "Pakal Engine - Basic Example" << std::endl;
	
	try
	{
		// Create engine with default settings (DummyUIManager already configured)
		std::cout << "creating settings!" << std::endl;
		Engine::Settings settings;
		
		// Create engine
		std::cout << "creating engine!" << std::endl;
		// Note: Engine and physics use threads by default (uses_thread = true)
		Engine engine(settings);
		
		// Create and run application
		std::cout << "creating application!" << std::endl;
		BasicApplication app;
		std::cout << "running engine!" << std::endl;
		engine.run(&app);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
