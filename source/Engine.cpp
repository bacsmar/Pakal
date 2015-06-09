#include "Engine.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Logger.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/WindowsConsoleChannel.h"

using namespace Pakal;

void Pakal::Engine::run( IPakalApplication *application )
{
	//Poco::Logger::root().setChannel( new Poco::ColorConsoleChannel() );
	Poco::Logger::root().setChannel( new Poco::WindowsColorConsoleChannel() );
	//Poco::Logger::root().setProperty("enableColors", "true");
	Poco::Logger::root().information("Initializing Pakal Engine Version " );
	Poco::Logger::root().warning("outasd");
	mApplication = application;
}

Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}
