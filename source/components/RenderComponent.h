///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IComponent.h"
#include "RTTI.h"

namespace Pakal
{
	class GraphicsSystem;	

	class _PAKALExport RenderComponent : public IComponent
	{
	public:
		virtual ~RenderComponent(){}
		RenderComponent(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }		
		void setSystem(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }		

		virtual void onInit(const GraphicsSystem &pSystem) = 0;

	protected:
		inline GraphicsSystem *getRenderSystem() { return m_RenderSystem; }				
	private:		

		GraphicsSystem *m_RenderSystem;		
		BasicTaskPtr init() override;	// hide init From derivated classes
	};
}