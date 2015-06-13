#include "IrrGraphicsSystem.h"

#include "LogMgr.h"

#include "Engine.h"
#include "ComponentSystem.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

Pakal::IrrGraphicsSystem::IrrGraphicsSystem()
	: mIsRendering(false),
	m_Window(0),
	device(nullptr),
	driver(nullptr),
	smgr(nullptr),
	guienv(nullptr),
	fpsText(nullptr)
{
	m_showFps = false;
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
	beginScene();
	bool isDrawing = draw();
	endScene();
	return isDrawing;
}

void Pakal::IrrGraphicsSystem::showFps( bool val )
{
	ASSERT(fpsText);
	m_showFps = val;
	fpsText->setVisible(val);
}

void Pakal::IrrGraphicsSystem::registerComponentFactories( std::vector<IComponentFactory*> &componentVector )
{
	LOG_INFO("[Graphic System] Registering Irrlicht Components");
	//componentVector.push_back( new ComponentFactory<MeshComponent, IrrGraphicsSystem>(this) );	
}
