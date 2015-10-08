////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#pragma once

#include "Config.h"
#include "irrlicht.h"
#include "ISceneNode.h"
#include "Components/Sprite.h"

class SpriteNode_Irrlicht :  public irr::scene::ISceneNode	
{
public:    
	explicit SpriteNode_Irrlicht(ISceneNode* parent, irr::scene::ISceneManager* mgr, irr::s32 id);

	virtual ~SpriteNode_Irrlicht();

	irr::core::rectf getLocalBounds() const;
	irr::core::rectf getGlobalBounds() const;
	void setColor(const irr::video::SColor& color);	

	virtual irr::u32 getMaterialCount() const override { return 1; }
	virtual irr::video::SMaterial& getMaterial(irr::u32 i) override { return m_material; } 

	virtual void OnRegisterSceneNode() override;
	virtual void render() override;
	virtual const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;

	void detach();
	void set_texture(irr::video::ITexture *texture);
	void set_frame(std::size_t frameIndex, const Pakal::Sprite& sprite, bool flipped);

private:	
	char m_flip_factor = 1;
	irr::core::aabbox3d<irr::f32>	m_box;
	irr::video::ITexture*			m_texture;
	irr::video::S3DVertex			m_vertices[4];
	irr::u16						m_indices[6];
	irr::video::SMaterial			m_material;
	irr::core::recti				m_frame_rect;	
};
