#include "EntitySystem.h"
#include "IEntity.h"

#include "Task.h"

using namespace Pakal;

void EntitySystem::updateSimulation()
{
	dispatchTasks();

	for( auto & entity: m_SimulatedEntities)
	{
		//entity->
	}
}
void EntitySystem::addEntityToSimulation( IEntity *e)
{
	std::function<int()> lambda = [=] (void) { m_SimulatedEntities.push_back(e); return 0; };

	getInbox()->pushTask( lambda );
}
void EntitySystem::removeEntityFromSimulation( IEntity *e)
{
	std::function<int()> lambda = [=] (void) { m_SimulatedEntities.remove(e); return 0; };

	getInbox()->pushTask( lambda );
}