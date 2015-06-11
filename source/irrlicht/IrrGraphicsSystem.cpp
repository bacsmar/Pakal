#include "IrrGraphicsSystem.h"

#include "LogMgr.h"

#include <assert.h>

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

void Pakal::IrrGraphicsSystem::initWindow()
{
	LOG_DEBUG("[Graphic System] Starting irrlicht");

	device =
		createDevice( video::EDT_OPENGL, dimension2d<u32>(640, 480), 32,
		false, false, false, 0);
	driver	= device->getVideoDriver();
	smgr	= device->getSceneManager();
	guienv	= device->getGUIEnvironment();	

	//

	LOG_INFO("[Graphic System] Sending message");
	/*
	SystemEvent *evt = EVENT_MANAGER->createEvent<SystemEvent>();
	evt->action = SystemEvent::ET_DISPLAY_CREATED;
	evt->displayEvent.width = driver->getScreenSize().Width;
	evt->displayEvent.height = driver->getScreenSize().Height;
	*/
#ifdef PAKAL_WIN32_PLATFORM
	m_Window = (size_t)driver->getExposedVideoData().OpenGLWin32.HWnd;
#else
	m_Window = (size_t)driver->getExposedVideoData().OpenGLLinux.HWnd;
#endif	

	//evt->displayEvent.windowHwnd = mWindow;
	//EVENT_MANAGER->raiseEvent(evt);

	LOG_INFO("[Graphic System] Registering Irrlicht Components");
	//registerIrrComponents(this);

	LOG_INFO("[Graphic System] done");
}

void Pakal::IrrGraphicsSystem::beginScene()
{
	//ASSERT(driver);
	driver->beginScene(true, true, SColor(255,0,0,0));
}

bool Pakal::IrrGraphicsSystem::draw( float time )
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

	fpsText->setText( core::stringw(driver->getFPS()).c_str());

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
