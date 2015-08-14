///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: PhysicComponent.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IComponent.h"
#include "RTTI.h"
#include "TaskFwd.h"

namespace Pakal
{
	class PhysicsSystem;

	class _PAKALExport PhysicComponent : public IComponent
	{
		friend class PhysicsSystem;
	public:
		virtual ~PhysicComponent(){}
		PhysicComponent(PhysicsSystem* pSystem) : m_PhysicsSystem(pSystem)	{ }

		BasicTaskPtr init() override final; 	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes

	protected:		
		virtual void onInit() = 0;
		virtual void onDestroy() = 0;

		PhysicsSystem* m_PhysicsSystem;				
	};
}