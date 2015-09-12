#pragma once
#include "Config.h"
#include "IInputManager.h"
#include "SFML/Window.hpp"

namespace Pakal
{
	class _PAKALExport InputManager_SFML : public IInputManager
	{
		void*		m_window_handle;
		sf::Window m_window;
		bool m_window_created;

	public:
		InputManager_SFML();

		void initialize() override;
		void terminate() override;
		void* create_window(const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) override;
		void close_window() override;
		void process_os_events() override;
	};
}