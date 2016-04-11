#pragma once
#include "Config.h"
#include "IManager.h"
#include "RTTI.h"
#include "persist/PersistUtils.h"
#include "ComponentManager.h"
#include <string>
#include <unordered_map>

namespace Pakal
{
	class Entity;
	class IEntityProvider;
	class EntityDescriptor;
	template<class T> class IFactory;

	class _PAKALExport EntityManager : public IManager, public IFactoryManager
	{
		ComponentManager* m_componentManager;
		std::unordered_map<std::string, EntityDescriptor*> m_descriptors;
		std::unordered_map<std::string, IFactory<Entity>*> m_entityFactories;
		std::unordered_map<std::string, IFactory<EntityDescriptor>*> m_descriptorFactories;

		void* create_object(const std::string& className) override;

	public:
		explicit EntityManager(ComponentManager* componentManager);
		virtual ~EntityManager();

		ComponentManager* get_component_manager() { return m_componentManager; };
		const EntityDescriptor* get_descriptor(const std::string& name);

		void initialize() override;
		void terminate() override;

		void load_descriptors(const std::string& source);
		void clear_descriptors();

		void add_descriptor(EntityDescriptor* descriptor);
		void remove_descriptor(const std::string& descriptor);

		void register_provider(IEntityProvider* provider);
		void add_entity_factory(IFactory<Entity>* factory);
		void add_descriptor_factory(IFactory<EntityDescriptor>* factory);


		Entity* create_entity(const std::string& name, const std::string& descriptor = "");
		template<class TEntity>
		TEntity* create_entity(const std::string& descriptor = "");

	};

	template <class TEntity>
	TEntity* EntityManager::create_entity(const std::string& descriptor)
	{
		Entity* entity = create_entity(TypeInfo::get<TEntity>().get_name(),descriptor);
		return static_cast<Entity*>(entity);
	}
}
