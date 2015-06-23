///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
// last modification: 13 jun 2015
// Purpose: Component Interface.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"

#include "RTTI.h"
#include "IComponentFactory.h"

namespace Pakal
{	
	class IEntity;	
	//--------------------------------- IComponent ---------------------------------------------------
	class _PAKALExport IComponent
	{
		friend class IEntity;
	public:
		virtual ~IComponent(void){}	
		
		IComponent() : 
			m_isInitialized(0),
			m_ParentEntity(0)
		{}		

		inline const IEntity * getParentEntity() const		{ return m_ParentEntity; };

		inline void setParentEntity(const IEntity *parent )	{ m_ParentEntity = parent; };
		
		inline bool isInitialized()	const					{ return m_isInitialized; }

		virtual void internalInit() = 0;		
		virtual const Pakal::RTTI &getType() = 0;
		virtual void initAsync()							= 0;

	protected:		

		const IEntity			*m_ParentEntity;		
		bool m_isInitialized;		

		//virtual void showYourSelfClass() = 0;
	};
}
