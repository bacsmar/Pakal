#pragma once
#include "Config.h"
#include "IInputManager.h"
#include "SFML/Window.hpp"

namespace Pakal
{
	class _PAKALExport InputManager_SFML : public IInputManager
	{
	public:
		InputManager_SFML();

		void initialize() override;
		void terminate() override;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
	};
}