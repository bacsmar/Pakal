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

namespace Pakal
{
	class PhysicsSystem;

	class _PAKALExport PhysicComponent : public IComponent
	{
	public:
		virtual ~PhysicComponent(){}
		PhysicComponent(PhysicsSystem *pSystem)	{ m_PhysicsSystem = pSystem; }		
		void setSystem(PhysicsSystem *pSystem)	{ m_PhysicsSystem = pSystem; }				

		virtual void onInit(const PhysicsSystem &pSystem) = 0;
		virtual void onDestroy(const PhysicsSystem &pSystem) = 0;

	protected:
		inline PhysicsSystem *getPhysicsSystem() { return m_PhysicsSystem; }				
	private:		

		PhysicsSystem *m_PhysicsSystem;		
		BasicTaskPtr init() override final; 	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes
	};
}