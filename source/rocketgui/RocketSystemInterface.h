#pragma once


#include <Rocket/Core/SystemInterface.h>

#include "Clock.h"

class RocketSystemInterface : public Rocket::Core::SystemInterface
{
    public:
        
		RocketSystemInterface(){}
        
        virtual ~RocketSystemInterface(){}

		float GetElapsedTime() override
		{
			return m_clock.getElapsedTime().asSeconds();
		}
    protected:
        Pakal::Clock m_clock;    
};
