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
#include <functional>


namespace Pakal
{
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class T>
	class IFactory
	{
	public:
		virtual T* create() = 0;
		//std::function<T*()> create;

		virtual const char* get_typename() = 0;

		virtual ~IFactory() {}
	};

	class IComponentFactory : public IFactory<Component>
	{
	public:
		virtual ~IComponentFactory(){}
	};	

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase, class TImplementation, class TInitializer>
	IComponentFactory * CreateComponentFactory(TInitializer* initializer)
	{
		struct CF : public IComponentFactory
		{
			Component* create() override
			{
				TBase *b = new TImplementation(m_initializer);
				return b;
			}

			explicit CF(TInitializer* initializer) : m_initializer(initializer){}
			TInitializer* m_initializer;

			virtual const char* get_typename() override { return TypeInfo::get<TBase>().getName(); };
		};		

		return new CF(initializer);
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase, class TImplementation>
	IComponentFactory * CreateComponentFactory()
	{
		class CF : public IComponentFactory
		{		
			Component* create() override 
			{ 
				TBase *b = new TImplementation();
				return b;
			}
		
			virtual const char* get_typename() override { return TypeInfo::get<TBase>().getName(); };
		};
		return new CF;		
	};	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}