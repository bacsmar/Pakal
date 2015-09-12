#include  "InputManager_SFML.h"
#include "LogMgr.h"


using namespace Pakal;

InputManager_SFML::InputManager_SFML() : m_window_handle(nullptr), m_window_created(false)
{
	
}

void InputManager_SFML::initialize()
{
	
}

void InputManager_SFML::terminate()
{
	if (m_window_created) close_window();
}

void* InputManager_SFML::create_window(const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel)
{
	ASSERT(m_window_created == false);
	m_window.create(sf::VideoMode(dimensions.x,dimensions.y,bitsPerPixel),"",fullscreen ?  sf::Style::Fullscreen : sf::Style::Default,sf::ContextSettings(),true);

	m_window_created = true;
	return m_window_handle = m_window.getSystemHandle();
}

void InputManager_SFML::close_window()
{
	m_window_created = false;
	m_window.close();
	m_window_handle = nullptr;
	closed_event.notify();
}

void InputManager_SFML::process_os_events()
{
	if (!m_window_created) return;
	
	sf::Event e;
	while (m_window.pollEvent(e))
	{
		switch (e.type)
		{
			case sf::Event::Closed: 
				close_window();
				break;
			case sf::Event::Resized: 
				resized_event.notify(tmath::vector2di(e.size.width,e.size.height));
				break;
			case sf::Event::LostFocus: break;
			case sf::Event::GainedFocus: break;
			case sf::Event::TextEntered: 
				LOG_ERROR("text entered: %c",e.text.unicode);
				break;
			case sf::Event::KeyPressed: break;
			case sf::Event::KeyReleased: break;
			case sf::Event::MouseWheelMoved: break;
			case sf::Event::MouseWheelScrolled: break;
			case sf::Event::MouseButtonPressed: break;
			case sf::Event::MouseButtonReleased: break;
			case sf::Event::MouseMoved: break;
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