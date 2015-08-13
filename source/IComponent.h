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
#include "BasicTask.h"

namespace Pakal
{	
	class IEntity;	
	class BasicTask;
	//--------------------------------- IComponent ---------------------------------------------------
	class _PAKALExport IComponent
	{
		friend class IEntity;
	public:
		REQUIRE_RTTI()

		virtual ~IComponent(void){}	
		
		IComponent() : 
			m_ParentEntity(nullptr),
			m_isInitialized(false)
		{}		

		inline const IEntity* getParentEntity() const		{ return m_ParentEntity; };
		inline void setParentEntity(const IEntity *parent)	{ m_ParentEntity = parent; };		
		inline bool isInitialized()	const					{ return m_isInitialized; }
		

		virtual BasicTaskPtr init()	= 0;
		virtual BasicTaskPtr destroy()	= 0;

	protected:		

		const IEntity* m_ParentEntity;		
		bool		   m_isInitialized;		

	};
}

