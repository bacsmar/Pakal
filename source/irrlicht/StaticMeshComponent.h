#pragma once
#include "config.h"

#include "components/GraphicComponent.h"

#include "irrlicht/IrrGraphicsSystem.h"

namespace Pakal
{	
	class _PAKALExport StaticMeshComponent : public GraphicComponent
	{	
	protected:
		DECLARE_RTTI(StaticMeshComponent);
		StaticMeshComponent(IrrGraphicsSystem * irr) : GraphicComponent(irr){}

		irr::video::ITexture		*m_texture;
		irr::scene::IMesh			*m_mesh ;
		irr::scene::IMeshSceneNode	*m_node;

		std::string m_modelName;
		std::string m_textureName;
	public:
		virtual void onInit() override;
		virtual void onDestroy() override;
		virtual void onUpdate() override;		

		inline void setModel( const char * modelName);
		inline void setTexure(const char* textureName);
		inline void setPosition( float x, float y, float z);
		void getPosition();
	};	
}