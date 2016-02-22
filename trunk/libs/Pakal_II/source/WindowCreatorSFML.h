#include  "Config.h"
#include "OSManager.h"
#include "SFML/Window.hpp"

namespace Pakal
{
	class WindowCreatorSFML : public OSManager::IWindowManager
	{
		OSManager* m_os_manager;
		sf::Window m_window;
		void* m_window_handle = nullptr;
		bool m_window_created = false;

	public:
		explicit WindowCreatorSFML(OSManager* manager) : m_os_manager(manager) {}

		unsigned setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel) override final;
		void close_window() override final;
		void process_window_events() override final;
	};
}