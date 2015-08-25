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
		DECLARE_RTTI_WITH_BASE(PhysicComponent,IComponent)

		virtual ~PhysicComponent(){}
		explicit PhysicComponent(PhysicsSystem* system) : m_physics_system(system)	{ }

		BasicTaskPtr init() override final; 	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes

	protected:		
		virtual void on_init() = 0;
		virtual void on_destroy() = 0;

		PhysicsSystem* m_physics_system;				
	};
}