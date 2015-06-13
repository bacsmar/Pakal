///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: IComponentFactory.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: Interface to create Icomponent
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

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
	template <class componentType, class SubSystemType>
	class ComponentFactory : public IComponentFactory
	{
	public:

		ComponentFactory(SubSystemType *s) : m_Subsystem(s) {}

		virtual ~ComponentFactory(){}

		virtual IComponent* create() const
		{
			return new componentType(this);
		}

		virtual void inityAsync( IComponent *c ) const
		{
			// set an async task to redirect the initialization to the correct thread		?
			// when if the component is ready the subsystem should call component::init()	?
			m_Subsystem->initComponentAsync(c);
		}

		virtual void terminateAsync( IComponent *c ) const
		{
			m_Subsystem->terminateComponentAsync(c);
		}

		virtual const char* getTypeName()
		{
			return componentType::getRTTI().getName();
		}

	protected:
		SubSystemType *m_Subsystem;
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}