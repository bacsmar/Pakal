#include "IrrRocketGUI.h"

#include "irrlicht.h"
#include "irrlicht/IrrGraphicsSystem.h"

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Core/String.h>

#include "rocketgui/RocketSystemInterface.h"
#include "rocketgui/PakalRocketFileInterface.h"
#include "rocketgui/IrrRocketRenderer.h"

using namespace Pakal;

void IrrRocketUI::setup_render_interface()
{
	//ctor
	auto gs = static_cast<IrrGraphicsSystem*>(m_graphics_system);

	irr::video::IVideoDriver* driver = gs->get_device()->getVideoDriver();	
	//auto swidth = driver->getScreenSize().Width;
	auto swidth = driver->getViewPort().getWidth();
	//auto sheight = driver->getScreenSize().Height;	
	auto sheight = driver->getViewPort().getHeight();	
	m_renderInterface = new IrrRocketRenderer(gs->get_device()->getVideoDriver());
	
	Rocket::Core::SetRenderInterface(m_renderInterface);

	GUI_Camera = gs->get_device()->getSceneManager()->addCameraSceneNode(nullptr, irr::core::vector3df(swidth / 2.f, sheight / 2.f, 5), irr::core::vector3df(swidth / 2.f, sheight / 2.f, 0), -1, false);
	GUI_Camera->setUpVector(irr::core::vector3df(0, -1, 0));
	irr::core::matrix4 mat;
	mat.buildProjectionMatrixOrthoLH(static_cast<irr::f32>(swidth), static_cast<irr::f32>(sheight), 0.1f, 10.f);
	GUI_Camera->setProjectionMatrix(mat);

	m_screen_width = swidth;
	m_screen_height = sheight;
}

void IrrRocketUI::draw_ui()
{
	RocketContext->Update();
	GUI_Camera->render();
	RocketContext->Render();
}
