#include "IrrGraphicsSystem.h"

#include <memory>

#include "LogMgr.h"
#include "ComponentSystem.h"
#include "IComponentFactory.h"
#include "IComponent.h"
#include "components/RenderComponent.h"

#include "IDebugDrawer.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

using namespace Pakal;

Pakal::IrrGraphicsSystem::IrrGraphicsSystem()
	: mIsRendering(false),
	m_Window(0),
	device(nullptr),
	driver(nullptr),
	smgr(nullptr),
	guienv(nullptr),	
	m_renderInfo(nullptr)	
{
	m_showFps = false;
	m_renderInfo = new RendererInfo();
}
//////////////////////////////////////////////////////////////////////////
bool IrrGraphicsSystem::initialize()
{
	this->dispatchTasks();
	initWindow();
	return true;
}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::initWindow()
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	device =
		createDevice( video::EDT_OPENGL, dimension2d<u32>(640, 480), 32,
		false, false, false, nullptr);
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();	

	m_renderInfo->m_Device = device;
	m_renderInfo->m_Driver = driver;

	showFps(m_showFps);

	smgr->addCameraSceneNode();	

#ifdef PAKAL_WIN32_PLATFORM
	m_Window = reinterpret_cast<size_t>(driver->getExposedVideoData().OpenGLWin32.HWnd);
#else
	m_Window = (size_t)driver->getExposedVideoData().OpenGLLinux.HWnd;
#endif	

	LOG_INFO("[Graphic System] done");

	ISceneNode* cube = smgr->addCubeSceneNode();

	 cube->setPosition(vector3df(0,0,5));
	 
	//  cube->setScale(vector3df(25,1,1));

}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::beginScene()
{
	ASSERT(driver);
	driver->beginScene(true, true, SColor(255,0,0,0));
}
//////////////////////////////////////////////////////////////////////////
bool Pakal::IrrGraphicsSystem::draw()
{
	smgr->drawAll();
	guienv->drawAll();

	for( auto &r : m_debugRenderers)
	{
		r->doDebugDraw();
	}

	bool isRunning = device->run();

	if( false == isRunning )
	{
		LOG_INFO("[Graphic System] Sending ET_DISPLAY_DESTROYED message");		
	}	

	if( m_showFps)
	{		
		core::stringw str = L"FPS [";
		str += driver->getName();
		str += "] FPS:";
		str += driver->getFPS();

		device->setWindowCaption(str.c_str());            
	}


	return isRunning;
}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::endScene()
{
	driver->endScene();
}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::setWindowCaption( const char * caption )
{
	ASSERT(device);
	device->setWindowCaption(L"");
}
//////////////////////////////////////////////////////////////////////////
bool Pakal::IrrGraphicsSystem::update()
{
	beginScene();
	bool isDrawing = draw();
	endScene();

	return isDrawing;
}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::showFps( bool val )
{	
	m_showFps = val;	
}
//////////////////////////////////////////////////////////////////////////
void Pakal::IrrGraphicsSystem::registerComponentFactories( std::vector<IComponentFactory*> &factories )
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");

	class RenderComponentTest : public Pakal::RenderComponent
	{
		DECLARE_RTTI(RenderComponentTest);
		virtual void onInit(const GraphicsSystem &renderSystem) override{};
		virtual void onDestroy(const GraphicsSystem &pSystem) override {};
		//TestComponent() : RenderComponent(nullptr){}
		RenderComponentTest(IrrGraphicsSystem * irr) : RenderComponent(irr){}		
	};

	factories.push_back( Pakal::CreateComponentFactory<RenderComponentTest>(this) );
	//factories.push_back( Pakal::CreateComponentFactory<TestComponent>() );
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::addDebugDrawerClient(IDebugDrawerClient * debugDrawer)
{	
	debugDrawer->setDrawer( m_renderInfo );
	m_debugRenderers.push_back( debugDrawer );	
} 
//////////////////////////////////////////////////////////////////////////
IrrGraphicsSystem::~IrrGraphicsSystem()
{
	for( auto &r : m_debugRenderers)
	{
		//delete r;	// do not delete debugRenderers, they're read only
	}
	LOG_DEBUG("[Graphic System] Shutdown Irrlicht");
	device->closeDevice();
	device->drop();
	delete m_renderInfo;
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::processComponentUpdateList(std::unordered_set<RenderComponent*> &list)
{
	for( auto & renderComponent : list)
	{
	}
}
//////////////////////////////////////////////////////////////////////////
void IrrGraphicsSystem::processComponentInitList(std::unordered_set<RenderComponent*> &list)
{
	for( auto & renderComponent : list)
	{
		auto switch_on = renderComponent->getRenderType();

		switch ( switch_on )
		{
		case RenderComponent::RCT_NONE:
		case RenderComponent::RCT_SPECIAL:
			renderComponent->onInit(*this);
			break;
		case RenderComponent::RCT_MESH:
			// do things with mesh!
			// renderComponent->mesh = SceneManager->addMesh( renderComponent->meshName );
			break;		
		case RenderComponent::RCT_ANIMATED_MESH:
			break;
		case RenderComponent::RCT_CAMERA:
			break;
		case RenderComponent::RCT_LIGHT:
			break;
		case RenderComponent::RCT_BILLBOARD:
			break;			
		default:
			break;
		}
	}
	list.clear();
}