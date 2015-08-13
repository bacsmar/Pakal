///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: IComponentFactory.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: Interface to create Icomponent
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
//#include <type_traits>
#include "IComponent.h"
#include "LogMgr.h"


namespace Pakal
{
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class IComponentFactory
	{
	public:
		virtual IComponent* create() = 0;

		virtual const RTTI& getComponentType() = 0;

		virtual ~IComponentFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TComponent, class TInitializer = void>
	class ComponentFactory : public IComponentFactory
	{
	protected:
		
		template<class T>
		inline IComponent *  _create (T* initializer) { return new TComponent(initializer); }
		
		inline IComponent *  _create (void*) { return new TComponent(); }

	public:

		ComponentFactory(TInitializer* intializer) : m_Initializer(intializer) {} 

		virtual ~ComponentFactory(){}

		virtual IComponent* create() override
		{
			return nullptr;
		}

		virtual const RTTI& getComponentType() override
		{
			return TComponent::getRTTI();
		}
				
	protected:
		TInitializer* m_Initializer;
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TComponent, class TInitializer>
	IComponentFactory * CreateComponentFactory(TInitializer* initializer)
	{
		return new ComponentFactory<TComponent, TInitializer>(initializer);		
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class componentType>
	IComponentFactory * CreateComponentFactory()
	{
		return new ComponentFactory<componentType, void>(nullptr);
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}