#pragma once
#include "GraphicsSystem.h"

#include <irrlicht.h>
#include <string>

#ifdef _IRR_WINDOWS_
	#pragma comment(lib, "Irrlicht.lib")
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

namespace Pakal
{
	class _PAKALExport IrrGraphicsSystem : public GraphicsSystem
	{
	public:
		inline irr::IrrlichtDevice		* getDevice() const { return device;	}		
		inline irr::video::IVideoDriver	* getDriver() const { return driver;	}		
		inline irr::scene::ISceneManager* getSmgr() const	{ return smgr;		}		
		inline irr::gui::IGUIEnvironment* getGuienv() const { return guienv;	}
		IrrGraphicsSystem();
	protected:

		bool mIsRendering;				

		std::string m_HomePath;
		std::string m_ConfigPath;
		size_t		m_Window;

		irr::IrrlichtDevice			* device;		
		irr::video::IVideoDriver	* driver;		
		irr::scene::ISceneManager	* smgr;		
		irr::gui::IGUIEnvironment	* guienv;

		irr::gui::IGUIStaticText	* fpsText;

		virtual ~IrrGraphicsSystem(){}

		virtual void initWindow();

		virtual void beginScene();

		virtual bool draw(  );

		virtual void endScene();

		virtual void setWindowCaption( const char* caption );

		virtual bool update();

		virtual void showFps( bool val );

	};
}