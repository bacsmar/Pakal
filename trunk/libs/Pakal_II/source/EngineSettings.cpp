#include "Engine.h"

#if PAKAL_USE_BOX2D == 1
#include "box2D/Box2DPhysicsSystem.h"
#endif

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

#if PAKAL_USE_SFML_AUDIO == 1

#include "sfml/SoundManager_SFML.h"
#if defined( _DEBUG) 
	#ifdef PAKAL_STATIC_LIB	//debug & static lib
		#pragma comment(lib, "sfml-audio-s-d.lib")
		#pragma comment(lib, "sfml-system-s-d.lib")
	#else
		#pragma comment(lib, "sfml-audio-d.lib")
		#pragma comment(lib, "sfml-system-d.lib")
	#endif
#else
	#ifdef PAKAL_STATIC_LIB	//debug & static lib
		#pragma comment(lib, "sfml-audio-s.lib") 
		#pragma comment(lib, "sfml-system-s.lib")
	#else
		#pragma comment(lib, "sfml-audio.lib") 
		#pragma comment(lib, "sfml-system.lib")
	#endif
#endif

#endif
#include "ResourceManager.h"

using namespace Pakal;


Engine::Settings::Settings() 
{
	use_threads = true;

#if PAKAL_USE_IRRLICHT == 1
	graphic_system_allocator = [](Engine* engine)
	{
		IrrGraphicsSystem* irrlicht = new IrrGraphicsSystem(false);

		ResourceManager::StreamReaderFactory factory;		
		factory.open_reader = [irrlicht](const std::string& fname){ return irrlicht->open_reader(fname); };
		factory.add_file_archive = [irrlicht](const std::string& fname){ return irrlicht->add_file_archive(fname); };

		engine->get_resource_manager()->register_reader( factory);
		return irrlicht;
	};
#endif

#if PAKAL_USE_BOX2D == 1
	physics_system_allocator = [this](Engine* engine) { return new Box2DPhysicsSystem(use_threads); };
#endif

#if PAKAL_USE_SFML_AUDIO == 1
	sound_manager_allocator = [](Engine* engine){ return new SoundManagerSFML(); };
#endif

}
