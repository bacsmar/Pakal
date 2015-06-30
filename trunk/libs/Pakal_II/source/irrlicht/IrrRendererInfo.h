#pragma once
#include "config.h"

#ifdef PAKAL_USE_IRRLICHT 

namespace irr { class IrrlichtDevice; namespace video { class IVideoDriver; } }

namespace Pakal
{	
	struct _PAKALExport IrrRendererInfo
	{
	public:
		irr::IrrlichtDevice			* m_Device;
		irr::video::IVideoDriver	* m_Driver;
		IrrRendererInfo() : m_Device(nullptr), m_Driver(nullptr) {};	
		virtual ~IrrRendererInfo() {};
	};	
}
#else

#error "sorry, but This Header is intended to use with irrlicht compilation only"

#endif