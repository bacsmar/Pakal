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
	class IrrGraphicsSystem;	

	class _PAKALExport RenderComponent : public IComponent
	{
	public:
		virtual ~RenderComponent(){}
		RenderComponent(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }		
		void setSystem(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }

		virtual void internalInit() = 0;
		virtual void initAsync();

	protected:
		inline GraphicsSystem *getRenderSystem() { return m_RenderSystem; }				
	private:		
		GraphicsSystem *m_RenderSystem;		
	};
}