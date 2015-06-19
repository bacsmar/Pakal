///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: ComponentSystem.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Class to control creation IComponents using factories.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Config.h"

#include <unordered_map>
#include <string>

namespace Pakal
{
	class IComponentFactory;
	class IComponent;
	class IComponentProvider;

	class _PAKALExport ComponentSystem
	{
	public:
		void registerFactory( IComponentFactory *factory);
		void registerFactory( IComponentFactory* factory, const std::string & name );

		void registerFactories( IComponentProvider *provider);

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
		std::unordered_map<std::string, IComponentFactory*> m_ComponentFactories;
	};
}