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
#include "TaskFwd.h"


namespace Pakal
{
	class GraphicsSystem;	

	class _PAKALExport GraphicComponent : public IComponent
	{
		friend class GraphicsSystem;
	public:

		virtual ~GraphicComponent();
		GraphicComponent(GraphicsSystem *renderSystem) : m_RenderSystem(renderSystem)	{}		
		void setSystem(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }		

		BasicTaskPtr init() override final;	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes	


	protected:
		inline GraphicsSystem *getRenderSystem() { return m_RenderSystem; }
		// is called when the component was initialized by the parent System
		virtual void onInit(const GraphicsSystem &pSystem) = 0;
		// is called when the component was destroyed by the parent System
		virtual void onDestroy(const GraphicsSystem &pSystem) = 0;
		// notifies the parent System of work to do
		virtual void notify();
	private:		

		GraphicsSystem *m_RenderSystem;		
	
	};
}