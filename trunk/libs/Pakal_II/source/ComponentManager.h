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
#include "RTTI.h"
#include "ComponentFactory.h"

namespace Pakal
{
	class Component;
	class IComponentProvider;

	class _PAKALExport ComponentManager : IManager
	{
	public:
		void initialize() override {};
		void terminate() override {};

		void register_factory(IComponentFactory* factory, bool replacePreviousFactory = false);

		template <class TBase>
		inline void register_factory(const std::function<Component*()>& fn , bool replacePreviousFactory = false)
		{
			register_factory(CreateFactory<Component, TBase>(fn), replacePreviousFactory);
		}

		void register_provider(IComponentProvider &provider);

		Component* create_component(const std::string& componentName);

		template <class T>
		T* create_component()
		{
 			Component* ic = create_component( TypeInfo::get<T>().get_name());
			return static_cast<T*>(ic);
		}

	protected:
		friend class Engine;
		virtual ~ComponentManager();
		ComponentManager(){}
	private:
		std::unordered_map<std::string, IComponentFactory*> m_factories;
	};
}