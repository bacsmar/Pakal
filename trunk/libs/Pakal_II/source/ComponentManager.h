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

#include "IManager.h"

namespace Pakal
{
	class IComponentFactory;
	class IComponent;
	class IComponentProvider;

	class _PAKALExport ComponentManager : IManager
	{
	public:
		void initialize() override {};
		void terminate() override {};

		void register_factory(IComponentFactory* factory, const std::string& name = "");
		void register_provider(IComponentProvider &provider);

		IComponent* create_component(const char* componentName);
		void drop_component(IComponent* component);

		template <class T>
		T* create_component()
		{
 			IComponent* ic = create_component(T::getRTTI().getName());
			return static_cast<T*>(ic);
		}

	protected:
		friend class Engine;
		virtual ~ComponentManager();
		ComponentManager(){}
	private:
		std::unordered_map<std::string, IComponentFactory*> m_ComponentFactories;
	};
}