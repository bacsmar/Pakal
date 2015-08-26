#include "Engine.h"

#if PAKAL_USE_BOX2D == 1
#include "box2D/Box2DPhysicsSystem.h"
#endif

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

using namespace Pakal;


Engine::Settings::Settings() 
{
	use_threads = true;

#if PAKAL_USE_IRRLICHT == 1
	graphic_system_allocator = [](Engine* engine) { return new IrrGraphicsSystem(engine->get_scheduler(),false); };
#endif

#if PAKAL_USE_BOX2D == 1
	physics_system_allocator = [this](Engine* engine) { return new Box2DPhysicsSystem(engine->get_scheduler(),use_threads); };
#endif


}
