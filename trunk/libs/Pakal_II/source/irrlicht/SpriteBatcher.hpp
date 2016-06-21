////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#pragma once

#include "Config.h"
#include "irrlicht.h"
#include "ISceneNode.h"
#include "Components/Sprite.h"


namespace irr {namespace scene {
	class CBatchingMesh;
}
}

namespace Pakal
{
	class SpriteNode_Irrlicht;
	class MaterialManager;

	class SpriteBatcher : public irr::scene::ISceneNode
	{		
	public:		
		SpriteBatcher(irr::scene::ISceneManager* mgr, irr::s32 id = -1);

		~SpriteBatcher();
		void add_sprite(SpriteNode_Irrlicht* sprite);
		void remove_sprite(SpriteNode_Irrlicht *sprite);

		void set_dirty();
		void render() override;
		const irr::core::aabbox3d<float>& getBoundingBox() const override;
		virtual void OnRegisterSceneNode() override;
	private:
		void update_mesh_buffer();
		bool m_dirty = true;
		std::vector<SpriteNode_Irrlicht*> m_sprites;
		irr::scene::ISceneManager* m_scene_manager = nullptr;
		irr::scene::ISceneNode*	m_node = nullptr;
		irr::scene::CBatchingMesh* m_batching_mesh = nullptr;
	};	
}
