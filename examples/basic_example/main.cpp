///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Basic example demonstrating Pakal Engine initialization
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Engine.h>
#include <IPakalApplication.h>
#include <iostream>

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
		Engine::Settings settings;
		
		// Create engine
		Engine engine(settings);
		
		// Create and run application
		BasicApplication app;
		engine.run(&app);
		
		std::cout << "Engine finished successfully!" << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
