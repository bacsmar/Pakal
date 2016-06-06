#pragma once

#include "GridComponent.h"
#include <irrlicht.h>
#include <irrlicht/IrrGraphicsSystem.h>
#include "TerrainComponent.h"

namespace Pakal
{
	class _PAKALExport TerrainComponent_Irrlicht : public TerrainComponent
	{
		DECLARE_RTTI_WITH_BASE(TerrainComponent_Irrlicht, TerrainComponent);

		IrrGraphicsSystem				*m_system;
		irr::scene::ITerrainSceneNode	*m_node;
	public:

		explicit TerrainComponent_Irrlicht(IrrGraphicsSystem* irrGraphicsSystem);;		

		virtual ~TerrainComponent_Irrlicht();
		void show() override;
		void hide() override;		

		BasicTaskPtr initialize(const Settings& settings) override;
		BasicTaskPtr destroy() override;
		void  set_position(const tmath::vector3df& position) override;
	};
}
