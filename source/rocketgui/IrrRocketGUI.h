
#pragma once

#include "RocketGUI.h"

namespace irr
{
    class IrrlichtDevice;
    class IEventReceiver;	
    namespace scene
    {
        class ICameraSceneNode;
    }
}

class IrrRocketRenderer;
class RocketSystemInterface;

namespace Pakal 
{
	
	class _PAKALExport IrrRocketUI : public RocketUI
	{			
	public:
		explicit IrrRocketUI(GraphicsSystem* renderInterface) : RocketUI(renderInterface)
		{
			
		}	

		void setup_render_interface() override;
		void draw_ui() override;
		void update_ui(long long dt) override;
	protected:
		irr::IrrlichtDevice*			m_irrlichtDevice = nullptr;		
		irr::scene::ICameraSceneNode*	GUI_Camera = nullptr;			
	};
}
