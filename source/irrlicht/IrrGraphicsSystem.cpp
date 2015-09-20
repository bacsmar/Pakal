#include "IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "ComponentFactory.h"

#include "IDebugDrawer.h"

#include "Components/MeshComponent.h"
#include "Components/MeshComponent_Irrlitch.h"
#include "ResourceManager.h"
#include "StreamFileIrrlicht.h"

#include <irrlicht/source/Irrlicht/CTimer.h>


using namespace irr;
using namespace irr::core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrFileSystemProvider::~IrrFileSystemProvider()
{
	for(auto i = m_irr_fs->getFileArchiveCount(); i > 0; i-- )
	{
		m_irr_fs->removeFileArchive(i - 1);
	}
}

//////////////////////////////////////////////////////////////////////////
IStreamPtr IrrGraphicsSystem::IrrFileSystemProvider::open_reader(const std::string& fname)
{	
	auto fileName = fname.c_str();
	if( m_irr_fs->existFile(fileName) )
	{
		IReadFile* file =  m_irr_fs->createAndOpenFile(fileName);
		return (std::make_shared<StreamFileIrrlicht>(file));
	}
	return nullptr;
}
//////////////////////////////////////////////////////////////////////////
ResourceManager::IFileArchive* IrrGraphicsSystem::IrrFileSystemProvider::add_file_archive(IStreamPtr file)
{
	auto pakalReader = new IrrReadPakalFile(file);
	auto fileArchive =   m_irr_fs->addFileArchive( pakalReader ) ? this : nullptr;
	pakalReader->drop();
	return fileArchive;
}
//////////////////////////////////////////////////////////////////////////
ResourceManager::IFileArchive* IrrGraphicsSystem::IrrFileSystemProvider::add_data_dir(const std::string& fname)
{		
	return m_irr_fs->addFileArchive(fname.c_str()) ? this : nullptr;
}

//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrGraphicsSystem(const Settings& settings,OSManager* windowManager)
	: GraphicsSystem(settings), 
	m_os_manager(windowManager),
	m_window_initialized(false),
	m_is_paused(false),
	device(nullptr),
	driver(nullptr),	
	smgr(nullptr),
	guienv(nullptr),
	m_fs_provider(nullptr),
	m_render_info(new RendererInfo()),
	m_show_fps(false)
{}
//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::~IrrGraphicsSystem()
{
	SAFE_DEL(m_render_info);
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::setup_window(const OSManager::WindowArgs& args)
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	SIrrlichtCreationParameters parameters;

	parameters.Bits = m_settings.bits;
	parameters.Fullscreen =  m_settings.full_screen;
	parameters.Vsync =  m_settings.vsync;
	parameters.DriverType = EDT_OPENGL;
	parameters.WindowId = (void*)args.windowId;	
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

	m_fs_provider = new IrrFileSystemProvider(device->getFileSystem() );
	ResourceManager::instance().register_reader(m_fs_provider);
	
	m_render_info->m_Device = device;
	m_render_info->m_Driver = driver;

	show_fps(m_show_fps);	

	LOG_INFO("[Graphic System] done");

	smgr->addCameraSceneNode(nullptr, vector3df(0,0,-3), vector3df(0,0,0));		

	// setting up events
	m_resized_callback_id = m_os_manager->event_window_resized.add_listener([this](OSManager::WindowArgs args)
	{
		device->getVideoDriver()->OnResize(dimension2du(args.size_x,args.size_y));
	},THIS_THREAD);
	
	m_destroyed_callback_id = m_os_manager->event_window_destroyed.add_listener([this](OSManager::WindowArgs args)
	{
		device->getContextManager()->destroySurface();
		m_window_initialized = false;
	}
	);

	//// next time we only need to recreate the openGL context
	m_created_callback_id = m_os_manager->event_window_created.add_listener([this](OSManager::WindowArgs args)
	{
		SEvent event;
		event.EventType = irr::EET_USER_EVENT;
		event.UserEvent.UserData1 = 0;	// for pakal IrrDevice 0 means... restar the context...
		event.UserEvent.UserData2 = args.windowId;
		device->postEventFromUser(event);
		m_window_initialized = true;
	}
	, THIS_THREAD);

	m_window_initialized = true;

}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_init_graphics()
{
	auto listenerId = m_os_manager->event_window_created.add_listener( [this](OSManager::WindowArgs args)
		{
			setup_window( args );
		}
	, THIS_THREAD);	
	//, NULL_THREAD);

	m_os_manager->setup_window(0, m_settings.resolution, m_settings.full_screen, m_settings.bits)->continue_with([](){})-> wait();

	m_os_manager->event_window_created.remove_listener(listenerId);	

}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_terminate_graphics()
{
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");

	m_os_manager->event_window_resized.remove_listener(m_resized_callback_id);	
	m_os_manager->event_window_destroyed.remove_listener(m_destroyed_callback_id);
	m_os_manager->event_window_created.remove_listener(m_created_callback_id);

	ResourceManager::instance().remove_reader(m_fs_provider);
	SAFE_DEL(m_fs_provider);
	device->closeDevice();
	device->drop();
	device = nullptr;
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::begin_scene()
{
	ASSERT(driver);
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

	if( m_show_fps)
	{		
		stringw str = L"FPS [";
		str += driver->getName();
		str += "] FPS:";
		str += driver->getFPS();

		device->setWindowCaption(str.c_str());            
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
	m_is_paused = true;
}

void IrrGraphicsSystem::on_resume_graphics()
{
	m_is_paused = false;
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
	m_show_fps = val;	
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
