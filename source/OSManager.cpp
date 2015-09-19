#include "OSManager.h"

#include "SingletonHolder.h"

#include "SFML/Window.hpp"

using namespace Pakal;

class WindowCreatorSFML : public OSManager::WindowImpl
{
public:
	void*		m_window_handle;
	sf::Window m_window;
	bool m_window_created;

	unsigned setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel) override
	{
			ASSERT(m_window_created == false);
		
			auto windowScreenStyle = fullscreen ?  sf::Style::Fullscreen : sf::Style::Default;
			auto videoMode = sf::VideoMode( dimensions.x, dimensions.y, bitsPerPixel);
		
			m_window.create( videoMode,"", windowScreenStyle , sf::ContextSettings(), true);
		
			m_window_created = true;
			m_window_handle = m_window.getSystemHandle();
			return (unsigned)m_window_handle;
	}	
};

BasicTaskPtr OSManager::setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel)
{	
	WindowArgs args;
	args.windowId = m_windowImpl->setup_window(windowId, dimensions, fullscreen, bitsPerPixel);
	if(args.windowId)
	{		
		args.size_x = dimensions.x;
		args.size_y = dimensions.y;
		on_window_created(args);
		
	}	
	return m_windows_setup_task.get_task();
}

void OSManager::on_window_created(const WindowArgs& arg)
{
	event_window_created.notify(arg);
	m_windows_setup_task.set_completed();
}

void OSManager::on_window_destroyed(const WindowArgs& arg)
{
	m_windows_setup_task = TaskCompletionSource();	// restart task
	event_window_destroyed.notify(arg);
}

void OSManager::close_window()
{
	//	m_window_created = false;
	//	m_window.close();
	//	m_window_handle = nullptr;
	//	closed_event.notify();
}

void OSManager::process_os_events()
{
	auto window = (WindowCreatorSFML*)m_windowImpl;
	if (!window->m_window_created) return;
		
		sf::Event e;
		while (window->m_window.pollEvent(e))
		{
			switch (e.type)
			{
			case sf::Event::Closed:
				close_window();
				break;
			case sf::Event::Resized:
				//resized_event.notify(tmath::vector2di(e.size.width,e.size.height));
				break;
			case sf::Event::LostFocus:
				//focused_event.notify(false);
				break;
			case sf::Event::GainedFocus:
				//focused_event.notify(true);
				break;
			case sf::Event::TextEntered: break;
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

OSManager& OSManager::instance()
{
	static SingletonHolder<OSManager> sh;
	return *sh.get();
}

OSManager::~OSManager()
{
	SAFE_DEL(m_windowImpl);
}

OSManager::OSManager()
{
	m_windowImpl = new WindowCreatorSFML();
}