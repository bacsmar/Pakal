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

	class _PAKALExport RenderComponent : public IComponent
	{
	public:
		enum RenderComponentType
		{
			RCT_NONE,
			RCT_ANIMATED_MESH,
			RCT_MESH,
			RCT_CAMERA,
			RCT_LIGHT,
			RCT_BILLBOARD,
			RCT_SPECIAL,
		};

		virtual ~RenderComponent();
		RenderComponent(GraphicsSystem *renderSystem) : m_RenderSystem(renderSystem), m_type(RCT_NONE)	{}		
		void setSystem(GraphicsSystem *renderSystem)	{ m_RenderSystem = renderSystem; }		

		// is called when the component was initialized by the parent System
		virtual void onInit(const GraphicsSystem &pSystem) = 0;
		// is called when the component was destroyed by the parent System
		virtual void onDestroy(const GraphicsSystem &pSystem) = 0;
		// notifies the parent System of work to do
		virtual void notify();

		inline RenderComponentType getRenderType(){ return m_type; }

	protected:
		inline GraphicsSystem *getRenderSystem() { return m_RenderSystem; }
		RenderComponentType m_type;
	private:		

		GraphicsSystem *m_RenderSystem;		
		BasicTaskPtr init() override final;	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes
	};
}