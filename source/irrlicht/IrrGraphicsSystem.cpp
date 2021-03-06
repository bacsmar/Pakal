#include "IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "ComponentFactory.h"

#include "IDebugDrawer.h"
#include "IUIManager.h"

#include "Components/MeshComponent_Irrlitch.h"
#include "Components/SpriteComponent_Irrlicht.h"
#include "Components/CameraComponent_Irrlicht.h"
#include "components/TileMapComponent_Irrlicht.h"
#include "components/DynamicMesh/DynamicMeshComponent_Irrlicht.h"
#include "ResourceManager.h"

#include <irrlicht/source/Irrlicht/CTimer.h>

#include "irrlicht/DirectorySourceIrrlitch.h"
#include "irrlicht/ZipSourceIrrlitch.h"

#include "MaterialManager.h"
#include <components/AnimatedMeshComponent.h>
#include <components/AnimatedMeshComponent_Irrlitch.h>
#include <components/GridComponent_Irrlicht.h>
#include <components/TerrainComponent_Irrlicht.h>
#include "SceneNodeBatcher.h"
#include "SpriteBatcher.hpp"

#ifdef PAKAL_ANDROID_PLATFORM
#include "android/osWrapperAndroid.h"
#endif


using namespace irr;
using namespace irr::core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::IrrGraphicsSystem(const Settings& settings)
	: GraphicsSystem(settings), m_resized_callback_id(0), m_destroyed_callback_id(0), m_created_callback_id(0),
	device(nullptr),
	driver(nullptr),	
	smgr(nullptr),
	guienv(nullptr),
	m_render_info(new RendererInfo()),
	m_batch_scene(nullptr), m_sprite_batcher(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::~IrrGraphicsSystem()
{
	SAFE_DEL(m_render_info);	
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_init_graphics(const WindowArgs& args)
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	SIrrlichtCreationParameters parameters;

	parameters.Bits = m_settings.bits;
	parameters.Fullscreen =  m_settings.full_screen;
	parameters.Vsync =  m_settings.vsync;
	parameters.DriverType = EDT_OPENGL;
	//parameters.DriverType = EDT_OPENGL_NO_FIXED;
	parameters.WindowId = reinterpret_cast<void*>(args.windowId);	
	parameters.WindowSize = dimension2di(args.size_x, args.size_y);	
	parameters.OGLES2ShaderPath = m_settings.resources_dir.c_str();	

#ifdef PAKAL_ANDROID_PLATFORM
	parameters.DriverType = EDT_OGLES2;		
	auto os_manger = &static_cast<OsWrapperAndroid&>(OsWrapperAndroid::instance());	// set OSManager Instance
	parameters.PrivateData = os_manger->activity;
	parameters.OGLES2ShaderPath = "";
#endif		
	device = createDeviceEx(parameters);
	
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();

	smgr->getParameters()->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

	m_batch_scene = new SceneNodeBatcher(smgr);
	m_sprite_batcher = new SpriteBatcher(smgr);

	LOG_DEBUG("[Graphic System] adding file sources");

	ResourceMgr.register_source<DirectorySource>([this]() { return new DirectorySourceIrrlitch(device->getFileSystem()); });
	ResourceMgr.register_source<ZipSource>([this]() { return new ZipSourceIrrlitch(device->getFileSystem()); });
	
	m_render_info->m_Device = device;
	m_render_info->m_Driver = driver;
	m_material_manager = new MaterialManager(this);

	for (auto &r : m_debug_renderers)
	{
		r->set_drawer(m_render_info);
	}

	LOG_INFO("[Graphic System] Starting irrlicht...done");

	// setting up events
	OSMgr.event_window_resized += {m_resized_callback_id, [this](const WindowArgs& a)
	{
		device->getVideoDriver()->OnResize({ a.size_x, a.size_y });
	}, THIS_THREAD};

	OSMgr.event_window_destroyed += {m_destroyed_callback_id, [this](const WindowArgs& a)
	{
		LOG_INFO("[Graphic System] window destroyed -> destroying surface");
		//device->getContextManager()->destroyContext();
		device->getContextManager()->destroySurface();
		device->getContextManager()->terminate();
	}, THIS_THREAD};

	//// next time we only need to recreate the openGL context
	OSMgr.event_window_created += {m_created_callback_id, [this](const WindowArgs& a)	
	{
		LOG_INFO("[Graphic System] window %X created -> recreating surface", a.windowId);
		SEvent event;
		event.EventType = irr::EET_USER_EVENT;
		event.UserEvent.UserData1 = 0;	// for pakal IrrDevice 0 means... restart the context...
		event.UserEvent.UserData2 = a.windowId;		
		device->postEventFromUser(event);
	}, THIS_THREAD};
}

//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_terminate_graphics()
{
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");

	OSMgr.event_window_resized	 -= m_resized_callback_id;
	OSMgr.event_window_created	 -= m_created_callback_id;
	OSMgr.event_window_destroyed -= m_destroyed_callback_id;

	device->closeDevice();
	device->drop();
	device = nullptr;
	SAFE_DEL(m_material_manager);
	SAFE_DEL(m_batch_scene);
	SAFE_DEL(m_sprite_batcher);
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::begin_scene()
{
	ASSERT(driver != nullptr);
	driver->beginScene(ECBF_COLOR | ECBF_DEPTH, SColor(255,100,100,100));
}

void IrrGraphicsSystem::draw_axis()
{
	video::SMaterial m_material;
	m_material.Thickness = 3;
	m_material.Lighting = false;
	m_material.FrontfaceCulling = false;		// enable both faces drawing
	m_material.BackfaceCulling = false;
	m_material.MaterialType = video::EMT_SOLID;
	driver->setMaterial(m_material);
	matrix4 tmat;
	driver->setTransform(ETS_WORLD, tmat);

	float scale = 10.f;

	// x axis is red
	driver->draw3DLine(core::vector3df(-0.0, 0.0, 0.0) * scale,
		core::vector3df(1.0, 0.0, 0.0) *scale,
		video::SColor(255, 255, 0, 0));

	// y axis is green
	driver->draw3DLine(core::vector3df(0.0, -0.0, 0.0) *scale,
		core::vector3df(0.0, 1.0, 0.0) *scale,
		video::SColor(255, 0, 255, 0));

	// z axis is blue
	driver->draw3DLine(core::vector3df(0.0, 0.0, -0.0) *scale,
		core::vector3df(0.0, 0.0, 1.0) *scale,
		video::SColor(255, 0, 0, 255));
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::draw()
{
	os::Timer::tick(); // do not remove
	if (smgr->getActiveCamera() == nullptr)
		return;
	smgr->drawAll();
	guienv->drawAll();	

	for (auto &r : m_debug_renderers)
	{
		r->do_debug_draw();
	}

	//if (m_draw_axis)
	//{
		draw_axis();
	//}	
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
	m_ui_manager->draw_ui();
	end_scene();
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_pause_graphics()
{
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::on_resume_graphics()
{
}
//////////////////////////////////////////////////////////////////////////
tmath::vector2du IrrGraphicsSystem::get_screen_resolution()
{
	ASSERT(device != nullptr);
	auto screenSize = device->getVideoDriver()->getScreenSize();
	return { screenSize.Width , screenSize.Height };
}

//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");

	factories.emplace_back(CreateComponentFactory<MeshComponent, MeshComponent_Irrlitch>(this));
	factories.emplace_back(CreateComponentFactory<AnimatedMeshComponent, AnimatedMeshComponent_Irrlitch>(this));
	factories.emplace_back(CreateComponentFactory<GridComponent, GridComponent_Irrlicht>(this));
	factories.emplace_back(CreateComponentFactory<TerrainComponent, TerrainComponent_Irrlicht>(this));
	factories.emplace_back(CreateComponentFactory<SpriteComponent, SpriteComponent_Irrlicht>(this));
	factories.emplace_back(CreateComponentFactory<CameraComponent, CameraComponent_Irrlicht>(this));
	factories.emplace_back(CreateComponentFactory<TileMapComponent, TileMapComponent_Irrlicht>(this));
	factories.emplace_back(CreateComponentFactory<DynamicMeshComponent, DynamicMeshComponent_Irrlicht>(this));
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::add_debug_drawer(IDebugDrawerClient* debugDrawer)
{
	m_debug_renderers.push_back( debugDrawer );	
}
