#pragma once
#include "Config.h"

#include <hash_map>

namespace Pakal
{
	class IComponentFactory;
	class IComponent;

	class _PAKALExport ComponentSystem
	{
	public:
		void registerFactory( IComponentFactory *factory);
		void registerFactory( IComponentFactory* factory, const std::string & name );

		IComponent * createComponent(const char * componentName);

		template <class T>
		T * createComponent()
		{
 			const char * componentTypeName = T::getRTTI().getName();
 			IComponent * ic = createComponent(componentTypeName);
			return static_cast<T*>(ic);
		}

	protected:
		friend class Engine;
		virtual ~ComponentSystem();
		ComponentSystem(){}
	private:
		std::hash_map<std::string, IComponentFactory*> m_ComponentFactories;
	};
}