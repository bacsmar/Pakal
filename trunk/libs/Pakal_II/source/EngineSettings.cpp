#include "Engine.h"

#include "persist/Archive.h"

#if PAKAL_USE_SFML_WINDOW == 1
	#include "WindowCreatorSFML.h"
#endif

#if PAKAL_USE_BOX2D == 1
	#include "box2D/Box2DPhysicsSystem.h"
#endif

#if PAKAL_USE_IRRLICHT == 1
	#include "irrlicht/IrrGraphicsSystem.h"
#endif

#if PAKAL_USE_SFML_AUDIO == 1
	#include "sfml/SoundManagerSFML.h"
#else
	#include "SoundManager.h"
#endif

#if PAKAL_USE_SFML_INPUT == 1
	#include "InputManager_SFML.h"
#endif

#if PAKAL_USE_ROCKET == 1
	#include "rocketgui/IrrRocketGUI.h"
	#pragma comment(lib, "RocketControls.lib")
	#pragma comment(lib, "RocketCore.lib")
#endif

#if PAKAL_USE_SCRIPTS
#pragma comment(lib, "oolua.lib")
#pragma comment(lib, "lua.lib")
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


Engine::Settings::Settings()
{

#if PAKAL_USE_IRRLICHT == 1

	#if PAKAL_USE_ROCKET == 1
		graphic_system_settings.ui_manager_allocator = [](GraphicsSystem* gs, IInputManager * im) { return new IrrRocketUI(gs, im);  };
	#endif
	
	graphic_system_allocator = [](Engine* engine, const GraphicsSystem::Settings& settings) { return new IrrGraphicsSystem(settings, engine->os_manager(), engine->input_manager()); };
#endif

#if PAKAL_USE_BOX2D == 1
	physics_system_allocator = [](Engine* engine,const PhysicsSystem::Settings& settings) { return new Box2DPhysicsSystem(settings); };
#endif

#if PAKAL_USE_SFML_AUDIO == 1
	sound_manager_allocator = [](Engine* engine){ return new SoundManagerSFML(); };
#else
	sound_manager_allocator = [](Engine*) { return new NullSoundManager(); };
#endif

#if PAKAL_USE_SFML_INPUT == 1
	os_manager_settings.input_manager_allocator = [](OSManager* manager){ return new InputManager_SFML(manager); };
#endif

#if PAKAL_USE_SFML_WINDOW == 1
	os_manager_settings.window_manager_allocator = [](OSManager* manager) { return new WindowCreatorSFML(manager); };
#endif

}


void Engine::Settings::persist(Archive* archive)
{
	archive->value("uses_thread", uses_thread);
	archive->value("max_fps", max_fps);
	archive->value("physic_system_settings", physic_system_settings);
	archive->value("graphic_system_settings", graphic_system_settings);
}
