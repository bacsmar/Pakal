#include "SystemComponent.h"
#include "EventScheduler.h"

Pakal::BasicTaskPtr Pakal::SystemComponent::initialize()
{
	return EventScheduler::instance().execute_in_thread(std::bind(&SystemComponent::on_initialize,this),m_system->get_thread_id());
}

Pakal::BasicTaskPtr Pakal::SystemComponent::destroy()
{
	return EventScheduler::instance().execute_in_thread(std::bind(&SystemComponent::on_destroy,this),m_system->get_thread_id());	
}