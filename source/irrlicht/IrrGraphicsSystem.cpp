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
	return  m_irr_fs->addFileArchive( new IrrReadPakalFile(file) ) ? this : nullptr;
}
//////////////////////////////////////////////////////////////////////////
ResourceManager::IFileArchive* IrrGraphicsSystem::IrrFileSystemProvider::add_data_dir(const std::string& fname)
{		
	return m_irr_fs->addFileArchive(fname.c_str()) ? this : nullptr;
}

//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrGraphicsSystem(const Settings& settings,IWindowManager* windowManager)
	: GraphicsSystem(settings), 
	m_window_manager(windowManager),
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
void IrrGraphicsSystem::init_window()
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");


	SIrrlichtCreationParameters parameters;

	parameters.Bits = m_settings.bits;
	parameters.Fullscreen =  m_settings.full_screen;
	parameters.Vsync =  m_settings.vsync;
	parameters.DriverType = EDT_OPENGL;
	parameters.WindowId = m_window_manager->create_window(m_settings.resolution,m_settings.full_screen,m_settings.bits);

	device = createDeviceEx(parameters);
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();	

	m_fs_provider = new IrrFileSystemProvider(device->getFileSystem() );
	ResourceManager::instance().register_reader(m_fs_provider);
	
	m_render_info->m_Device = device;
	m_render_info->m_Driver = driver;

	show_fps(m_show_fps);

	smgr->addCameraSceneNode();

	LOG_INFO("[Graphic System] done");

	smgr->addCameraSceneNode(nullptr, vector3df(0,0,-3), vector3df(0,0,0));		


	m_resized_callback_id = m_window_manager->resized_event.add_listener([this](tmath::vector2di dimensions)
	{
		device->getVideoDriver()->OnResize(dimension2du(dimensions.x,dimensions.y));
	},THIS_THREAD);

}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_init_graphics()
{
	init_window();
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_terminate_graphics()
{
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");

	m_window_manager->resized_event.remove_listener(m_resized_callback_id);

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
