////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#pragma once


#include "irrlicht.h"
#include "ISceneNode.h"
#include "Sprite_Irrlicht.hpp"

using namespace irr;

class SpriteNode_Irrlicht :  public irr::scene::ISceneNode	
{
public:    
	explicit SpriteNode_Irrlicht(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id);

	virtual ~SpriteNode_Irrlicht();

	irr::core::rectf getLocalBounds() const;
	irr::core::rectf getGlobalBounds() const;
	void setColor(const video::SColor& color);

	void set_texture(irr::video::ITexture *texture);

	void setFrame(std::size_t frameIndex, const SpriteIrrlicht* sprite);

	virtual s32 getMaterialCount(){ return 1; }

	virtual video::SMaterial& getMaterial(s32 i){ return m_material; } 
	virtual void OnRegisterSceneNode() override;

	virtual void render() override;

	virtual const core::aabbox3d<f32>& getBoundingBox() const override;

	void detach();

private:    
    
	core::aabbox3d<f32> m_box;
	video::ITexture*	m_texture;    
	video::S3DVertex	m_vertices[4];
	u16					m_indices[6];
	video::SMaterial	m_material;
	irr::core::recti	m_frame_rect;
	
};
