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

namespace Pakal
{
	class IComponent;	

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class IComponentFactory
	{
	public:
		virtual IComponent* create() const = 0 ;
		virtual void		inityAsync(IComponent *c) const = 0;
		virtual void		terminateAsync(IComponent *c) const = 0;

		virtual const char* getTypeName() = 0;

		virtual ~IComponentFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////

	class DefaultComponentInitializer
	{
	public:
		void initComponentAsync(IComponent *c){}
		void terminateComponentAsync(IComponent *c){}
	};

	template <class componentType, class ComponentInitializer = DefaultComponentInitializer>
	class ComponentFactory : public IComponentFactory
	{
	public:

		ComponentFactory(ComponentInitializer *s) : m_System(s) {}

		virtual ~ComponentFactory(){}

		virtual IComponent* create() const
		{
			// if you want to use this template factory, your component class should define 
			// a constructor similar to the one used in IComponent.h
			return new componentType(this);
		}

		virtual void inityAsync( IComponent *c ) const
		{
			// set an async task to redirect the initialization to the correct thread		?
			// when if the component is ready the Initializer should call component::internalInit()	?
			if(m_System) 
			{ 
				m_System->initComponentAsync(c);
			}
			else // null Initializer?
			{
				c->internalInit();
			}			
		}

		virtual void terminateAsync( IComponent *c ) const
		{
			if(m_System) 
			{ 
				m_System->terminateComponentAsync(c);
			}
			else // null Initializer?
			{
				//c->terminate();
			}			
		}

		virtual const char* getTypeName() override
		{
			return componentType::getRTTI().getName();
		}

	protected:
		ComponentInitializer *m_System;
	};

	template <class componentType, class T>
	IComponentFactory * CreateComponentFactory(T *t)
	{
		return new ComponentFactory<componentType, T>(t);		
	};
	
	template <class componentType>
	IComponentFactory * CreateComponentFactory()
	{
		return new ComponentFactory<componentType>(nullptr);
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}