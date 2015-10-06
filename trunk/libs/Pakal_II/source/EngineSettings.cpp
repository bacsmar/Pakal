#include "Engine.h"

#include "SoundManager.h"

#if PAKAL_USE_BOX2D == 1
	#include "box2D/Box2DPhysicsSystem.h"
#endif

#if PAKAL_USE_IRRLICHT == 1
	#include "irrlicht/IrrGraphicsSystem.h"
#endif

#if PAKAL_USE_SFML_AUDIO == 1
	#include "sfml/SoundManagerSFML.h"
#endif

#if PAKAL_USE_SFML_INPUT == 1
	#include "InputManager_SFML.h"
#endif

#if defined(PAKAL_WIN32_PLATFORM )

		// pugixml
#if defined(_DEBUG)
	#pragma comment(lib, "pugixmlsd.lib")
#else
	#pragma comment(lib, "pugixmls.lib")
#endif	// pugixml

#if PAKAL_USE_SFML == 1
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
	#ifdef PAKAL_STATIC_LIB	//release & static lib
		#pragma comment(lib, "sfml-audio-s.lib") 
		#pragma comment(lib, "sfml-system-s.lib")
		#pragma comment(lib, "sfml-window-s.lib")
	#else
		#pragma comment(lib, "sfml-audio.lib") 
		#pragma comment(lib, "sfml-system.lib")
		#pragma comment(lib, "sfml-window.lib")
	#endif
#endif
#endif	//PAKAL_USE_SFML
#endif	//PAKAL_WIN32_PLATFORM

using namespace Pakal;


Engine::Settings::Settings()  : uses_thread(true)
{

#if PAKAL_USE_IRRLICHT == 1
	graphic_system_allocator = [](Engine* engine, const GraphicsSystem::Settings& settings) { return new IrrGraphicsSystem(settings, engine->os_manager()); };
#endif

#if PAKAL_USE_BOX2D == 1
	physics_system_allocator = [](Engine* engine,const PhysicsSystem::Settings& settings) { return new Box2DPhysicsSystem(settings); };
#endif

	sound_manager_allocator = [](Engine*) { return new NullSoundManager(); };

#if PAKAL_USE_SFML_AUDIO == 1
	sound_manager_allocator = [](Engine* engine){ return new SoundManagerSFML(); };
#endif

#if PAKAL_USE_SFML_INPUT == 1
	input_manager_allocator = [](Engine* engine){ return new InputManager_SFML(); };
#endif

}
