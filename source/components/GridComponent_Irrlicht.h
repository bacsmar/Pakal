#pragma once

#include "GridComponent.h"
#include <irrlicht/GridSceneNode.h>
#include <irrlicht/IrrGraphicsSystem.h>

namespace Pakal
{
	class _PAKALExport GridComponent_Irrlicht : public GridComponent
	{
		DECLARE_RTTI_WITH_BASE(GridComponent_Irrlicht, GridComponent);

		IrrGraphicsSystem	*m_system;
		CGridSceneNode		*m_node;
	public:

		explicit GridComponent_Irrlicht(IrrGraphicsSystem* irrGraphicsSystem);;		

		virtual ~GridComponent_Irrlicht();
		void show() override;
		void hide() override;

		BasicTaskPtr initialize(const Settings& settings) override;
		BasicTaskPtr destroy() override;
	};
}
