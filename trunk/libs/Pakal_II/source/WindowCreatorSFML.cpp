#include "WindowCreatorSFML.h"
#include "IInputManager.h"

using namespace Pakal;

unsigned WindowCreatorSFML::setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel)
{
	ASSERT(m_window_created == false);

	auto windowScreenStyle = fullscreen ? sf::Style::Fullscreen : sf::Style::Default;
	auto videoMode = sf::VideoMode(dimensions.x, dimensions.y, bitsPerPixel);

	m_window.create(videoMode, "", windowScreenStyle, sf::ContextSettings(), true);
	m_window.setSize({ static_cast<unsigned>(dimensions.x), static_cast<unsigned>(dimensions.y) });

	m_window_created = true;
	m_window_handle = m_window.getSystemHandle();
	return reinterpret_cast<unsigned>(m_window_handle);

}

void WindowCreatorSFML::close_window()
{
	m_window.close();
	m_window_created = false;
}

void WindowCreatorSFML::process_window_events() 
{		
	ASSERT(m_window_created);

	sf::Event e;
	while (m_window.pollEvent(e))
	{
		switch (e.type)
		{
			case sf::Event::Closed:	
				m_os_manager->on_window_destroyed(WindowArgs());
				m_os_manager->event_app_finished();
				break;
			case sf::Event::Resized:
			{
				WindowArgs args;
				args.size_x = e.size.width;
				args.size_y = e.size.height;
				m_os_manager->event_window_resized(args);
			}
				break;
			case sf::Event::LostFocus:	m_os_manager->event_window_focused(false);	break;
			case sf::Event::GainedFocus: m_os_manager->event_window_focused(true); break;
			case sf::Event::TextEntered: 
			{
				TextArgs args;
				args.unicode = e.text.unicode;
				m_os_manager->get_input_manager()->event_text(args);
			}
				break;
			case sf::Event::KeyPressed: break;
			case sf::Event::KeyReleased: break;
			case sf::Event::MouseWheelMoved: break;
			case sf::Event::MouseWheelScrolled: break;
			case sf::Event::MouseButtonPressed: 
			{
				MouseArgs args;
				args.button_id = e.mouseButton.button;
				args.x = e.mouseButton.x;
				args.y = e.mouseButton.y;
				m_os_manager->get_input_manager()->event_mouse_pressed(args);
			}				
				break;
			case sf::Event::MouseButtonReleased: 
			{
				MouseArgs args;
				args.button_id = e.mouseButton.button;
				args.x = e.mouseButton.x;
				args.y = e.mouseButton.y;
				m_os_manager->get_input_manager()->event_mouse_released(args);
			}
				break;
			case sf::Event::MouseMoved: 
			{
				MouseArgs args;
				args.button_id = 0;
				args.x = e.mouseMove.x;
				args.y = e.mouseMove.y;
				m_os_manager->get_input_manager()->event_mouse_moved(args);
			}
				break;
			case sf::Event::MouseEntered: break;
			case sf::Event::MouseLeft: break;
			case sf::Event::JoystickButtonPressed: break;
			case sf::Event::JoystickButtonReleased: break;
			case sf::Event::JoystickMoved: break;
			case sf::Event::JoystickConnected: break;
			case sf::Event::JoystickDisconnected: break;
			case sf::Event::TouchBegan: break;
			case sf::Event::TouchMoved: break;
			case sf::Event::TouchEnded: break;
			case sf::Event::SensorChanged: break;
			default: break;
		}
	}
}

