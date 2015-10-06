#include "OSManager.h"
#include "SingletonHolder.h"

#include "SFML/Window.hpp"

#if defined( PAKAL_WIN32_PLATFORM)
#elif defined( PAKAL_ANDROID_PLATFORM)
#endif

using namespace Pakal;

class WindowCreatorSFML : public OSManagerAbstract::WindowImpl
{
	OSManagerAbstract*	m_os_manager;
	void*		m_window_handle;	
	bool		m_window_created;
	sf::Window	m_window;
public:
	explicit WindowCreatorSFML(OSManagerAbstract* manager) : m_os_manager(manager), m_window_handle(nullptr), m_window_created(false) { }	

	unsigned setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel) override
	{
			ASSERT(m_window_created == false);
		
			auto windowScreenStyle = fullscreen ?  sf::Style::Fullscreen : sf::Style::Default;
			auto videoMode = sf::VideoMode( dimensions.x, dimensions.y, bitsPerPixel);
		
			m_window.create( videoMode,"", windowScreenStyle , sf::ContextSettings(), true);
		
			m_window_created = true;
			m_window_handle = m_window.getSystemHandle();
			return reinterpret_cast<unsigned>(m_window_handle);
	}	
	void process_os_events() override
	{		
		ASSERT(m_window_created);

		sf::Event e;
		while (m_window.pollEvent(e))
		{
			switch (e.type)
			{
			case sf::Event::Closed:	
				m_os_manager->on_window_destroyed(OSManagerAbstract::WindowArgs());
				m_os_manager->on_app_finished();
				break;
			case sf::Event::Resized:
			{
				OSManagerAbstract::WindowArgs args;
				args.size_x = e.size.width;
				args.size_y = e.size.height;
				m_os_manager->on_window_resized(args);
			}
			break;
			case sf::Event::LostFocus:	m_os_manager->on_window_focused(false);	break;
			case sf::Event::GainedFocus: m_os_manager->on_window_focused(true); break;
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
	void wait_for_os_events() override
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		process_os_events();
	}
};

void OSManagerAbstract::initialize()
{
	m_windowImpl = new WindowCreatorSFML(this);
}

void OSManagerAbstract::terminate()
{
	SAFE_DEL(m_windowImpl);
}

void OSManagerAbstract::wait_for_os_events()
{
	m_windowImpl->wait_for_os_events();
}

TaskPtr<OSManagerAbstract::WindowArgs> OSManagerAbstract::setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel)
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

void OSManagerAbstract::on_window_created(const WindowArgs& arg)
{
	event_window_created.notify(arg);
	m_windows_setup_task.set_completed(arg);
}

void OSManagerAbstract::on_window_destroyed(const WindowArgs& arg)
{
	m_windows_setup_task = TaskCompletionSource<WindowArgs>();
	event_window_destroyed.notify(arg);
}

void OSManagerAbstract::on_app_finished()
{
	event_app_finished.notify();
}

void OSManagerAbstract::on_window_redraw_needed(const WindowArgs& arg)
{
	event_window_redraw_needed.notify(arg);
}

void OSManagerAbstract::on_window_resized(const WindowArgs& arg)
{
	event_window_resized.notify(arg);
}

void OSManagerAbstract::on_window_focused(bool focused)
{
	event_window_focused.notify(focused);
}

void OSManagerAbstract::on_app_paused()
{
	event_app_paused.notify();
}

void OSManagerAbstract::on_app_resumed()
{
	event_app_resumed.notify();
}

void OSManagerAbstract::on_app_started()
{
	event_app_started.notify();
}

void OSManagerAbstract::on_app_stoped()
{
	event_app_stoped.notify();
}

void OSManagerAbstract::process_window_events()
{
	m_windowImpl->process_os_events();
}

OSManager& OSManager::instance()
{
	static SingletonHolder<OSManager> sh;
	return *sh.get();
}

OSManagerAbstract::~OSManagerAbstract()
{	
}

OSManagerAbstract::OSManagerAbstract()
{	
}