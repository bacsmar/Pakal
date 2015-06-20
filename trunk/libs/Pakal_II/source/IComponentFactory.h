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

namespace Pakal
{
	class IComponent;	

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class IComponentFactory
	{
	public:
		virtual IComponent* create() = 0;

		virtual const char* getTypeName() = 0;

		virtual ~IComponentFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class componentType, class ComponentInitializer = void>
	class ComponentFactory : public IComponentFactory
	{
	protected:
		
		template<class T>
		inline IComponent *  _create (T * system)	
		{
			componentType * c = new componentType(system); 
			//componentType * c = new componentType(); 
			//c->setSystem(m_System);
			//m_system->addToUpdateList( c );
			return c;
		}
		
		inline IComponent *  _create (void * dummy) { return new componentType(); }

	public:

		ComponentFactory(ComponentInitializer *s) : m_System(s) {}

		virtual ~ComponentFactory(){}

		virtual IComponent* create() override //const override
		{
			return _create(m_System);			
		}

		virtual const char* getTypeName() override
		{
			return componentType::getRTTI().getName();
		}
				
	protected:
		ComponentInitializer *m_System;
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class componentType, class T>
	IComponentFactory * CreateComponentFactory(T *t)
	{
		return new ComponentFactory<componentType, T>(t);		
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class componentType>
	IComponentFactory * CreateComponentFactory()
	{
		return new ComponentFactory<componentType, void>(nullptr);
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}