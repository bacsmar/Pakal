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
	fpsText(nullptr),
	m_renderInfo(nullptr),
	m_ActiveQueue(0)
{
	m_showFps = false;
	m_renderInfo = new RendererInfo();
}

bool IrrGraphicsSystem::initialize()
{
	initWindow();
	return true;
}

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

	fpsText = guienv->addStaticText(L"",
		rect<s32>(10,10,260,22), true);
	fpsText->setOverrideColor( video::SColor(255,255,255,255));
	showFps(m_showFps);

	smgr->addCameraSceneNode();	

	//

	LOG_INFO("[Graphic System] Sending message");
	/*
	SystemEvent *evt = EVENT_MANAGER->createEvent<SystemEvent>();
	evt->action = SystemEvent::ET_DISPLAY_CREATED;
	evt->displayEvent.width = driver->getScreenSize().Width;
	evt->displayEvent.height = driver->getScreenSize().Height;
	*/
#ifdef PAKAL_WIN32_PLATFORM
	m_Window = reinterpret_cast<size_t>(driver->getExposedVideoData().OpenGLWin32.HWnd);
#else
	m_Window = (size_t)driver->getExposedVideoData().OpenGLLinux.HWnd;
#endif	

	//evt->displayEvent.windowHwnd = mWindow;
	//EVENT_MANAGER->raiseEvent(evt);

	//registerIrrComponents(this);
	//Engine::instance().getComponentSystem()->registerFactory( new ComponentFactory<MeshComponent, IrrGraphicsSystem>(this) );

	LOG_INFO("[Graphic System] done");
}
/*
void Pakal::IrrGraphicsSystem::registerYourFuckingComponentsFuckingSystem( ve)
{

}
*/

void Pakal::IrrGraphicsSystem::beginScene()
{
	ASSERT(driver);
	driver->beginScene(true, true, SColor(255,0,0,0));
}

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
		/*
		SystemEvent *evt = EVENT_MANAGER->createEvent<SystemEvent>();
		evt->action = SystemEvent::ET_DISPLAY_DESTROYED;				
		evt->displayEvent.windowHwnd = mWindow;;
		EVENT_MANAGER->raiseEvent(evt);
		*/
	}	

	if( m_showFps)
	{
		fpsText->setText( core::stringw(driver->getFPS()).c_str());
	}

	return isRunning;
}

void Pakal::IrrGraphicsSystem::endScene()
{
	driver->endScene();
}

void Pakal::IrrGraphicsSystem::setWindowCaption( const char * caption )
{
	ASSERT(device);
	device->setWindowCaption(L"");
}

bool Pakal::IrrGraphicsSystem::update()
{
	initializeComponentsInQueue();

	beginScene();
	bool isDrawing = draw();
	endScene();

	return isDrawing;
}
void IrrGraphicsSystem::initializeComponentsInQueue()
{
	m_ComponentQueueMutex.lock();
	int queueToProcess = m_ActiveQueue;
	m_ActiveQueue = (m_ActiveQueue + 1) % MAX_INITIALIZATION_QUEUES;
	m_ComponentInitializationList[m_ActiveQueue].clear();
	m_ComponentQueueMutex.unlock();	
	

	for( auto& c : m_ComponentInitializationList[queueToProcess] )
	{
		c->internalInit();
	}	
}

void Pakal::IrrGraphicsSystem::showFps( bool val )
{
	ASSERT(fpsText);
	m_showFps = val;
	fpsText->setVisible(val);
}

void Pakal::IrrGraphicsSystem::registerComponentFactories( std::vector<IComponentFactory*> &factories )
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");


	class TestComponent : public Pakal::RenderComponent
	{
		DECLARE_RTTI(TestComponent);
		void internalInit()	{}
		//TestComponent() : RenderComponent(nullptr){}
		TestComponent(IrrGraphicsSystem * irr) : RenderComponent(irr){}
		virtual void initAsync(){}
	};
	
	factories.push_back( Pakal::CreateComponentFactory<TestComponent>(this) );
	//factories.push_back( Pakal::CreateComponentFactory<TestComponent>() );
}
BasicTask * IrrGraphicsSystem::initComponentAsync(IComponent *c)
{	
	std::lock_guard<std::mutex> lock(m_ComponentQueueMutex);
	m_ComponentInitializationList[m_ActiveQueue].push_back( c );	

	// TODO: Implement the task
	return nullptr;
}
BasicTask * IrrGraphicsSystem::terminateComponentAsync(IComponent *c)
{
	// TODO: Implement the task
	return nullptr;
}

void IrrGraphicsSystem::addDebugDrawerClient(IDebugDrawerClient * debugDrawer)
{	
	debugDrawer->setDrawer( m_renderInfo );
	m_debugRenderers.push_back( debugDrawer );	
} 

IrrGraphicsSystem::~IrrGraphicsSystem()
{
	for( auto &r : m_debugRenderers)
	{
		delete r;
	}
}