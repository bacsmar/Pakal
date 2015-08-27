#include "Component.h"
#include "EventScheduler.h"

Pakal::BasicTaskPtr Pakal::Component::initialize()
{
	return EventScheduler::instance().execute_in_thread(std::bind(&Component::on_initialize,this),m_system->get_thread_id());
}

Pakal::BasicTaskPtr Pakal::Component::destroy()
{
	return EventScheduler::instance().execute_in_thread(std::bind(&Component::on_destroy,this),m_system->get_thread_id());	
}