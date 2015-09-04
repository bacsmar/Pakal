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
		#pragma comment(lib, "sfml-window-s-d.lib")
	#else
		#pragma comment(lib, "sfml-audio-d.lib")
		#pragma comment(lib, "sfml-system-d.lib")
		#pragma comment(lib, "sfml-window-d.lib")
	#endif
#else
	#ifdef PAKAL_STATIC_LIB	//debug & static lib
		#pragma comment(lib, "sfml-audio-s.lib") 
		#pragma comment(lib, "sfml-system-s.lib")
		#pragma comment(lib, "sfml-window-s.lib")
	#else
		#pragma comment(lib, "sfml-audio.lib") 
		#pragma comment(lib, "sfml-system.lib")
		#pragma comment(lib, "sfml-window.lib")
	#endif
#endif

#endif

using namespace Pakal;


Engine::Settings::Settings() 
{
	uses_thread = true;
	physic_system_settings.uses_thread = uses_thread;
	graphic_system_settings.uses_thread = false;

#if PAKAL_USE_IRRLICHT == 1
	graphic_system_allocator = [](Engine* engine, const GraphicsSystem::Settings& settings) { return new IrrGraphicsSystem(settings); };
#endif

#if PAKAL_USE_BOX2D == 1
	physics_system_allocator = [this](Engine* engine,const PhysicsSystem::Settings& settings) { return new Box2DPhysicsSystem(settings); };
#endif

#if PAKAL_USE_SFML_AUDIO == 1
	sound_manager_allocator = [](Engine* engine){ return new SoundManagerSFML(); };
#endif

}
