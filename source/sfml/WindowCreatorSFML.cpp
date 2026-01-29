#include "WindowCreatorSFML.h"
#include "IInputManager.h"

using namespace Pakal;

MouseButton map_sfml_to_pakal(sf::Mouse::Button btn)
{
	switch (btn)
	{
	case sf::Mouse::Left: return MouseButton::Left;
	case sf::Mouse::Right: return MouseButton::Right;
	case sf::Mouse::Middle: return MouseButton::Middle;
	case sf::Mouse::XButton1: return MouseButton::XButton1;
	case sf::Mouse::XButton2: return MouseButton::XButton2;
	default: break;
	}

	return static_cast<MouseButton>(-1);
}

unsigned WindowCreatorSFML::setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel)
{
	ASSERT(m_window_created == false);

	auto windowScreenStyle = fullscreen ? sf::Style::Fullscreen : sf::Style::Default;
	auto videoMode = sf::VideoMode(dimensions.x, dimensions.y, bitsPerPixel);
	auto contextSettings = sf::ContextSettings(8,8);	// z buffer, stencil buffer
	
	m_window = new sf::Window();
	m_window->create(videoMode, "", windowScreenStyle, contextSettings);
	m_window->setSize({ static_cast<unsigned>(dimensions.x), static_cast<unsigned>(dimensions.y) });	

	m_window_created = true;
	m_window_handle = reinterpret_cast<void*>(m_window->getSystemHandle());
	return static_cast<unsigned>(reinterpret_cast<uintptr_t>(m_window_handle));

}

void WindowCreatorSFML::close_window()
{
	m_window->close();
	SAFE_DEL(m_window);
	m_window_created = false;
}

void WindowCreatorSFML::process_window_events() 
{		
	ASSERT(m_window_created);
	
	sf::Event e;
	while (m_window->pollEvent(e))
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
			case sf::Event::LostFocus:	
				m_os_manager->event_window_focused(false);	break;
			case sf::Event::GainedFocus: 
				m_os_manager->event_window_focused(true); break;
			case sf::Event::TextEntered: 
			{
				TextArgs args;
				args.unicode = e.text.unicode;
				m_os_manager->get_input_manager()->event_text(args);
			}
				break;
			case sf::Event::KeyPressed: 
			{
				KeyArgs args;
				args.key = static_cast<Key>(e.key.code);
				args.key_modifier |= e.key.alt ? KeyArgs::KM_ALT : 0;
				args.key_modifier |= e.key.control ? KeyArgs::KM_CTRL : 0;
				args.key_modifier |= e.key.shift ? KeyArgs::KM_SHIFT : 0;
				m_os_manager->get_input_manager()->event_key_down(args);
			}
				break;
			case sf::Event::KeyReleased: 
			{
				KeyArgs args;
				args.key = static_cast<Key>(e.key.code);
				args.key_modifier |= e.key.alt ? KeyArgs::KM_ALT : 0;
				args.key_modifier |= e.key.control ? KeyArgs::KM_CTRL : 0;
				args.key_modifier |= e.key.shift ? KeyArgs::KM_SHIFT : 0;
				m_os_manager->get_input_manager()->event_key_up(args);
			}
				break;
			case sf::Event::MouseWheelMoved: break;
			case sf::Event::MouseWheelScrolled: 
			{
				MouseArgs args;
				args.delta = e.mouseWheelScroll.delta;
				args.x = e.mouseWheelScroll.x;
				args.y = e.mouseWheelScroll.y;
				m_os_manager->get_input_manager()->event_mouse_wheel(args);
			}
				break;
			case sf::Event::MouseButtonPressed: 
			{
				MouseArgs args;
				args.button_id = map_sfml_to_pakal(e.mouseButton.button);
				args.x = e.mouseButton.x;
				args.y = e.mouseButton.y;
				m_os_manager->get_input_manager()->event_mouse_pressed(args);
			}				
				break;
			case sf::Event::MouseButtonReleased: 
			{
				MouseArgs args;
				args.button_id = map_sfml_to_pakal(e.mouseButton.button);
				args.x = e.mouseButton.x;
				args.y = e.mouseButton.y;
				m_os_manager->get_input_manager()->event_mouse_released(args);
			}
				break;
			case sf::Event::MouseMoved: 
			{
				MouseArgs args;
				args.button_id = static_cast<MouseButton>(-1);
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
			case sf::Event::TouchBegan: {

				//TODO: change this method  to an specialized class this was only for debug purposes
				//TouchArgs args;
				//args.x = e.touch.x;
				//args.y = e.touch.y;
				//args.finger = e.touch.finger;
				//m_os_manager->get_input_manager()->event_touch_began(args);
				MouseArgs args;
				args.button_id = static_cast<MouseButton> (e.touch.finger);
				args.x = e.touch.x;
				args.y = e.touch.y;
				m_os_manager->get_input_manager()->event_mouse_pressed(args);
			} break;
			case sf::Event::TouchMoved: {
				//TODO: change this method  to an specialized class this was only for debug purposes
				//TouchArgs args;
				//args.x = e.touch.x;
				//args.y = e.touch.y;
				//args.finger = e.touch.finger;
				//m_os_manager->get_input_manager()->event_touch_moved(args);

				MouseArgs args;
				args.button_id = static_cast<MouseButton> (e.touch.finger);
				args.x = e.touch.x;
				args.y = e.touch.y;
				m_os_manager->get_input_manager()->event_mouse_moved(args);

			} break;
			case sf::Event::TouchEnded: 
			{
				//TODO: change this method  to an specialized class this was only for debug purposes
				//TouchArgs args;
				//args.x = e.touch.x;
				//args.y = e.touch.y;
				//args.finger = e.touch.finger;
				//m_os_manager->get_input_manager()->event_touch_ended(args);
				MouseArgs args;
				args.button_id =static_cast<MouseButton> (e.touch.finger);
				args.x = e.touch.x;
				args.y = e.touch.y;
				//m_os_manager->get_input_manager()->event_mouse_pressed(args);
				m_os_manager->get_input_manager()->event_mouse_released(args);
			} break;
			case sf::Event::SensorChanged: break;
			default: break;
		}
	}
}

void WindowCreatorSFML::set_title(const std::string& title)
{	
	m_window->setTitle(title);
}

void WindowCreatorSFML::flush()
{
	if(m_manage_context)
		m_window->display();
}

tmath::vector2di WindowCreatorSFML::get_position()
{
	auto pos = m_window->getPosition();
	return{ pos.x,pos.y };
}

tmath::vector2di WindowCreatorSFML::get_size()
{
	auto pos = m_window->getSize();
	return{ (int)pos.x,(int)pos.y };
}
