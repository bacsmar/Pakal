#pragma once
#include "Config.h"
#include "IInputManager.h"
#include "SFML/Window.hpp"

namespace Pakal
{
	class _PAKALExport InputManager_SFML : public IInputManager
	{		
		//sf::Window m_window;

	public:
		InputManager_SFML();

		void initialize() override;
		void terminate() override;		
	};
}