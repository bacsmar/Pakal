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
#include "Component.h"
#include "LogMgr.h"


namespace Pakal
{
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class IComponentFactory
	{
	public:
		virtual Component* create() = 0;

		virtual const char* get_typename() = 0;

		virtual ~IComponentFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase,class TImplementation, class TInitializer = void>
	class ComponentFactory : public IComponentFactory
	{
	protected:
		
		template<class T>
		inline TImplementation*  _create (T* initializer) { return new TImplementation(initializer); }
		
		inline TImplementation*  _create (void*) { return new TImplementation(); }

	public:

		explicit ComponentFactory(TInitializer* intializer) : m_initializer(intializer)
		{			
			static_assert( (std::is_base_of<TBase, TImplementation>::value), "incompatible types");
			static_assert( (std::is_base_of<Component, TImplementation>::value), "incompatible types");
			ASSERT_MSG( TImplementation::is_RTTI_valid<TBase>(), "TImplementation's RTTI is missing");
		} 

		virtual ~ComponentFactory(){}

		virtual Component* create() override
		{
			return _create(m_initializer);
		}

		virtual const char* get_typename() override
		{
			return TBase::getRTTI().getName();
		}
				
	protected:
		TInitializer* m_initializer;
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase,class TImplementation, class TInitializer>
	IComponentFactory * CreateComponentFactory(TInitializer* initializer)
	{
		return new ComponentFactory<TBase,TImplementation,TInitializer>(initializer);		
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase,class TImplementation>
	IComponentFactory * CreateComponentFactory()
	{
		return new ComponentFactory<TBase,TImplementation,void>(nullptr);
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TImplementation, class TInitializer>
	IComponentFactory * CreateComponentFactory(TInitializer* initializer)
	{
		return new ComponentFactory<TImplementation,TImplementation,TInitializer>(initializer);		
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TImplementation>
	IComponentFactory * CreateComponentFactory()
	{
		return new ComponentFactory<TImplementation,TImplementation,void>(nullptr);
	};
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}