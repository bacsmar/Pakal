#include "IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "ComponentFactory.h"

#include "IDebugDrawer.h"

#include "Components/MeshComponent.h"
#include "Components/MeshComponent_Irrlitch.h"
#include "ResourceManager.h"

#include <irrlicht/source/Irrlicht/CTimer.h>

#include "irrlicht/DirectorySourceIrrlitch.h"
#include "irrlicht/ZipSourceIrrlitch.h"


using namespace irr;
using namespace irr::core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrGraphicsSystem(const Settings& settings,OSManager* osManager)
	: GraphicsSystem(settings, osManager),
	device(nullptr),
	driver(nullptr),	
	smgr(nullptr),
	guienv(nullptr),
	m_render_info(new RendererInfo())
{}
//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::~IrrGraphicsSystem()
{
	SAFE_DEL(m_render_info);
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_init_graphics(const OSManager::WindowArgs& args)
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	SIrrlichtCreationParameters parameters;

	parameters.Bits = m_settings.bits;
	parameters.Fullscreen =  m_settings.full_screen;
	parameters.Vsync =  m_settings.vsync;
	parameters.DriverType = EDT_OPENGL;
	parameters.WindowId = reinterpret_cast<void*>(args.windowId);	
	parameters.WindowSize = dimension2di(args.size_x, args.size_y);	

#ifdef PAKAL_ANDROID_PLATFORM
	parameters.DriverType = EDT_OGLES2;	
	parameters.PrivateData = m_os_manager->activity;
	parameters.OGLES2ShaderPath = "";
#endif		
	device = createDeviceEx(parameters);
	
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();	

	auto& rmgr = ResourceManager::instance();

	rmgr.register_source<DirectorySource>([this]() { return new DirectorySourceIrrlitch(device->getFileSystem()); });
	rmgr.register_source<ZipSource>([this]() { return new ZipSourceIrrlitch(device->getFileSystem()); });
	
	m_render_info->m_Device = device;
	m_render_info->m_Driver = driver;

	LOG_INFO("[Graphic System] done");

	smgr->addCameraSceneNode(nullptr, vector3df(0,0,-30), vector3df(0,0,0));		

	// setting up events
	m_resized_callback_id = m_os_manager->event_window_resized.add_listener([this](OSManager::WindowArgs a)
	{
		device->getVideoDriver()->OnResize(dimension2du(a.size_x,a.size_y));
	},THIS_THREAD);
	
	m_destroyed_callback_id = m_os_manager->event_window_destroyed.add_listener([this](OSManager::WindowArgs a)
	{
		device->getContextManager()->destroySurface();
	}, THIS_THREAD);

	//// next time we only need to recreate the openGL context
	m_created_callback_id = m_os_manager->event_window_created.add_listener([this](OSManager::WindowArgs a)
	{
		SEvent event;
		event.EventType = irr::EET_USER_EVENT;
		event.UserEvent.UserData1 = 0;	// for pakal IrrDevice 0 means... restar the context...
		event.UserEvent.UserData2 = a.windowId;
		device->postEventFromUser(event);
	}, THIS_THREAD);

}

//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_terminate_graphics()
{
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");

	m_os_manager->event_window_resized.remove_listener(m_resized_callback_id);	
	m_os_manager->event_window_destroyed.remove_listener(m_destroyed_callback_id);
	m_os_manager->event_window_created.remove_listener(m_created_callback_id);

	device->closeDevice();
	device->drop();
	device = nullptr;
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::begin_scene()
{
	ASSERT(driver != nullptr);
	driver->beginScene(true, true, SColor(255,200,200,200));
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::draw()
{
	os::Timer::tick(); // do not remove
	smgr->drawAll();
	guienv->drawAll();

	for( auto &r : m_debug_renderers)
	{
		r->do_debug_draw();
	}


}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::end_scene()
{
	driver->endScene();
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_update_graphics(long long dt)
{
	begin_scene();
	draw();
	end_scene();
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
	ASSERT(device!= nullptr);
	device->setWindowCaption(caption);
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");

	factories.push_back( CreateComponentFactory<MeshComponent,MeshComponent_Irrlitch>(this));
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::add_debug_drawer(IDebugDrawerClient* debugDrawer)
{
	debugDrawer->set_drawer( m_render_info );
	m_debug_renderers.push_back( debugDrawer );	
}
