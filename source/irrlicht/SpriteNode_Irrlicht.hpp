////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#pragma once

#include "Config.h"
#include "irrlicht.h"
#include "ISceneNode.h"
#include "Components/Sprite.h"

namespace Pakal
{
	class MaterialManager;

	class SpriteNode_Irrlicht :  public irr::scene::ISceneNode	
	{
	public:    
		explicit SpriteNode_Irrlicht(ISceneNode* parent, irr::scene::ISceneManager* mgr);
		explicit SpriteNode_Irrlicht(ISceneNode* parent, irr::scene::ISceneManager* mgr, 
			MaterialManager* materialManager);

		virtual ~SpriteNode_Irrlicht();

		irr::core::rectf getLocalBounds() const;
		irr::core::rectf getGlobalBounds() const;
		void setColor(const irr::video::SColor& color);	

		virtual irr::u32 getMaterialCount() const override { return 1; }
		virtual irr::video::SMaterial& getMaterial(irr::u32 i) override { return m_buffer.Material; } 

		virtual void OnRegisterSceneNode() override;
		virtual void render() override;
		virtual const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;

		void detach();
		void set_texture(irr::video::ITexture *texture);
		void set_frame(std::size_t frameIndex, const Pakal::Sprite& sprite);

		inline const irr::scene::SMeshBuffer* getMeshBuffer() const
		{ return &m_buffer; }

	private:		
		irr::scene::SMeshBuffer			m_buffer;
		irr::video::ITexture*			m_texture;
		irr::core::recti				m_frame_rect;
	};
}