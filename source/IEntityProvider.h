#pragma once
#include "Config.h"
#include "Factory.h"

#include <vector>

namespace Pakal
{
	class Entity;
	class EntityDescriptor;

	class _PAKALExport IEntityProvider
	{
	public:
		virtual ~IEntityProvider() {}
		virtual void register_entities(std::vector<IFactory<Entity>*>& factories) = 0;
		virtual void register_descriptors(std::vector<IFactory<EntityDescriptor>*>& factories) = 0;
	};
}
