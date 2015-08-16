#include "IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "IComponentFactory.h"

#include "IDebugDrawer.h"

#include "Components/MeshComponent.h"

using namespace irr;
using namespace irr::core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrGraphicsSystem(EventScheduler* scheduler)
	: GraphicsSystem(scheduler), 
	m_is_rendering(false),
	m_window(0),
	device(nullptr),
	driver(nullptr),
	smgr(nullptr),
	guienv(nullptr),	
	m_render_info(new RendererInfo()),
	m_showFps(false)
{}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::initWindow()
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	device =
		createDevice( EDT_OPENGL, dimension2d<u32>(640, 480), 32,
		false, false, false, nullptr);
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();	

	m_render_info->m_Device = device;
	m_render_info->m_Driver = driver;

	show_fps(m_showFps);

	smgr->addCameraSceneNode();	

#ifdef PAKAL_WIN32_PLATFORM
	m_window = reinterpret_cast<size_t>(driver->getExposedVideoData().OpenGLWin32.HWnd);
#else
	m_Window = (size_t)driver->getExposedVideoData().OpenGLLinux.HWnd;
#endif	

	LOG_INFO("[Graphic System] done");

	smgr->addCameraSceneNode(nullptr, vector3df(0,0,-100), vector3df(0,0,0));		

}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_init_graphics()
{
	initWindow();
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_terminate_graphics()
{
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");
	device->closeDevice();
	device->drop();
	device = nullptr;
	delete m_render_info;
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::beginScene()
{
	ASSERT(driver);
	driver->beginScene(true, true, SColor(255,0,0,0));
}
//////////////////////////////////////////////////////////////////////////
bool IrrGraphicsSystem::draw()
{
	smgr->drawAll();
	guienv->drawAll();

	for( auto &r : m_debug_renderers)
	{
		r->do_debug_draw();
	}

	auto isRunning = device->run();

	if( false == isRunning )
	{
		LOG_INFO("[Graphic System] Sending ET_DISPLAY_DESTROYED message");
	}	

	if( m_showFps)
	{		
		stringw str = L"FPS [";
		str += driver->getName();
		str += "] FPS:";
		str += driver->getFPS();

		device->setWindowCaption(str.c_str());            
	}

	return isRunning;
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::endScene()
{
	driver->endScene();
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_update_graphics()
{
	beginScene();
	bool result = draw();
	endScene();
	if (result == false) terminate();
}

void IrrGraphicsSystem::on_pause_graphics()
{
}

void IrrGraphicsSystem::on_resume_graphics()
{
}

//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::set_window_caption(const wchar_t* caption)
{
	ASSERT(device);
	device->setWindowCaption(caption);
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::show_fps( bool val )
{	
	m_showFps = val;	
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");

	factories.push_back( CreateComponentFactory<MeshComponent>(this));
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::add_debug_drawer(IDebugDrawerClient* debugDrawer)
{
		debugDrawer->set_drawer( m_render_info );
	m_debug_renderers.push_back( debugDrawer );	
}
