
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
		IrrRocketUI(GraphicsSystem* renderInterface, IInputManager* input_manager)
			: RocketUI(renderInterface, input_manager)
		{
		}	
		void setup_render_interface() override;
		void draw_ui() override;
	protected:
		irr::IrrlichtDevice*			m_irrlichtDevice = nullptr;		
		irr::scene::ICameraSceneNode*	GUI_Camera = nullptr;			
	};
}
