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
	protected:

		bool mIsRendering;				

		std::string mHomePath;
		std::string mConfigPath;
		size_t		mWindow;

		irr::IrrlichtDevice			* device;		
		irr::video::IVideoDriver	* driver;		
		irr::scene::ISceneManager	* smgr;		
		irr::gui::IGUIEnvironment	* guienv;

		irr::gui::IGUIStaticText	* fpsText;

		virtual ~IrrGraphicsSystem(){}

		virtual void initWindow();

		virtual void beginScene();

		virtual bool draw( float time );

		virtual void endScene();

		virtual void setWindowCaption( const char* caption );

	};
}