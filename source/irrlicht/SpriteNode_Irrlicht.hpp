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
	explicit SpriteNode_Irrlicht(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id,
		unsigned frameTime = 200.f, bool paused = false, bool looped = true);

	virtual ~SpriteNode_Irrlicht();

    void update(unsigned deltaTime);
	void setFrameTime(unsigned time);
	unsigned getFrameTime() const;

    void setAnimation(const SpriteIrrlicht& animation);
	void setAnimation(const std::string& animationName);
    void play();
    void play(const SpriteIrrlicht& animation);
    void pause();
    void stop();
    void setLooped(bool looped);	
    void setColor(const video::SColor& color);
    const SpriteIrrlicht* getAnimation() const;

	irr::core::rectf getLocalBounds() const;
	irr::core::rectf getGlobalBounds() const;

    bool isLooped() const;
    bool isPlaying() const;
    
    void setFrame(std::size_t newFrame, bool resetTime = true);


	virtual s32 getMaterialCount()
	{
		return 1;
	}

	virtual video::SMaterial& getMaterial(s32 i)
	{
		return m_material;
	} 
	virtual void OnRegisterSceneNode() override;

	virtual void render() override;

	virtual const core::aabbox3d<f32>& getBoundingBox() const override;

	inline bool getFlipped() const { return m_isFlipped; }
	inline void setFlipped(bool val) { m_isFlipped = val; }

private:
    const SpriteIrrlicht* m_sprite;

	core::aabbox3d<f32> m_box;

	unsigned			m_frameTime;
	unsigned			m_currentTime;

    std::size_t			m_currentFrame;
    bool				m_isPaused;
    bool				m_isLooped;
    
	video::ITexture*	m_texture;
    
	video::S3DVertex	m_vertices[4];
	u16					m_indices[6];
	video::SMaterial	m_material;

	bool				m_isFlipped;	
};
