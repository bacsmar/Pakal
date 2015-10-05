////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "SpriteNode_Irrlicht.hpp"
#include "Sprite_Irrlicht.hpp"

using namespace irr;
using namespace scene;

SpriteNode_Irrlicht::SpriteNode_Irrlicht(ISceneNode* parent, ISceneManager* mgr, s32 id, unsigned frameTime, bool paused, bool looped) 
	: ISceneNode(parent,mgr, id),
    m_sprite(nullptr), m_frameTime(frameTime), m_currentTime(0), m_currentFrame(0), m_isPaused(paused), m_isLooped(looped), m_texture(nullptr),
	m_isFlipped(0)
{	
	video::SColor scolor(255,255,255,255);			
	m_material.Wireframe = false;
	m_material.Lighting = false;	
	m_material.AmbientColor = scolor;
	m_material.DiffuseColor = scolor;
	m_material.EmissiveColor  = scolor;
	m_material.BackfaceCulling = false;	

	m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;

	m_box.reset(-1.0f,-1.0f,0.0f);
	m_box.addInternalPoint(1.0f,1.0f,0.0f);

	m_indices[0] = 0;
	m_indices[1] = 2;
	m_indices[2] = 1;
	m_indices[3] = 0;
	m_indices[4] = 3;
	m_indices[5] = 2;		
}

void SpriteNode_Irrlicht::setAnimation(const SpriteIrrlicht& animation)
{
    m_sprite = &animation;
	m_texture = m_sprite->getSpriteSheet();
    m_currentFrame = 0;
    setFrame(m_currentFrame);
}

void SpriteNode_Irrlicht::setAnimation(const std::string& animationName)
{
}

void SpriteNode_Irrlicht::setFrameTime(unsigned time)
{
    m_frameTime = time;
}

void SpriteNode_Irrlicht::play()
{
    m_isPaused = false;
}

void SpriteNode_Irrlicht::play(const SpriteIrrlicht& animation)
{
    if (getAnimation() != &animation)
        setAnimation(animation);
    play();
}

void SpriteNode_Irrlicht::pause()
{
    m_isPaused = true;
}

void SpriteNode_Irrlicht::stop()
{
    m_isPaused = true;
    m_currentFrame = 0;
    setFrame(m_currentFrame);
}

void SpriteNode_Irrlicht::setLooped(bool looped)
{
    m_isLooped = looped;
}

void SpriteNode_Irrlicht::setColor(const video::SColor& color)
{
    // Update the vertices' color
    m_vertices[0].Color = color;
    m_vertices[1].Color = color;
    m_vertices[2].Color = color;
    m_vertices[3].Color = color;
}

const SpriteIrrlicht* SpriteNode_Irrlicht::getAnimation() const
{
    return m_sprite;
}

irr::core::rectf SpriteNode_Irrlicht::getLocalBounds() const
{
    irr::core::recti rect = m_sprite->getFrame(m_currentFrame);

    float width = static_cast<float>(std::abs(rect.getWidth()));
    float height = static_cast<float>(std::abs(rect.getHeight()));

    return irr::core::rectf(0.f, 0.f, width, height);
}

irr::core::rectf SpriteNode_Irrlicht::getGlobalBounds() const
{
    //return getTransform().transformRect(getLocalBounds());
	return getLocalBounds();
}
const core::aabbox3d<f32>& SpriteNode_Irrlicht::getBoundingBox() const
{
	return m_box;
}

bool SpriteNode_Irrlicht::isLooped() const
{
    return m_isLooped;
}

bool SpriteNode_Irrlicht::isPlaying() const
{
    return !m_isPaused;
}

unsigned SpriteNode_Irrlicht::getFrameTime() const
{
    return m_frameTime;
}

inline core::vector3df vector2Dto3D(const core::vector2df &v2d)
{
	return core::vector3df(v2d.X, v2d.Y, 0.0f);
}

void SpriteNode_Irrlicht::setFrame(std::size_t newFrame, bool resetTime)
{
    if (m_sprite)
    {
        //calculate new vertex positions and texture coordiantes
        irr::core::recti	rect = m_sprite->getFrame(newFrame);
		core::vector2di		offset_i (m_sprite->getOffset(newFrame));
		core::vector2df		offset;
		offset.X = (irr::f32)offset_i.X;
		offset.Y = (irr::f32)offset_i.Y;

		core::dimension2du d = m_sprite->getSpriteSheet()->getSize();		

		auto height = rect.LowerRightCorner.Y;
		auto width = rect.LowerRightCorner.X;		

		float left = static_cast<float>(rect.UpperLeftCorner.X) + 0.0001f;
		float right = left + static_cast<float>(width);
		float top = static_cast<float>(rect.UpperLeftCorner.Y);
		float bottom = top + static_cast<float>(height);

		left /= d.Width;
		right /= d.Width;
		top /= d.Height;
		bottom /= d.Height;

        m_vertices[0].Pos = vector2Dto3D(core::vector2df(0.f, 0.f) + offset);
        m_vertices[1].Pos = vector2Dto3D(core::vector2df(0.f, static_cast<float>(height)) + offset );
        m_vertices[2].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), static_cast<float>(height)) + offset );
        m_vertices[3].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), 0.f) + offset );        

        m_vertices[0].TCoords = core::vector2df(left, top);
        m_vertices[1].TCoords = core::vector2df(left, bottom);
        m_vertices[2].TCoords = core::vector2df(right, bottom);
        m_vertices[3].TCoords = core::vector2df(right, top);		

		m_box.reset(m_vertices[0].Pos);
		for (s32 i=1; i<4; ++i)
			m_box.addInternalPoint(m_vertices[i].Pos);

		m_material.setTexture(0, m_texture);
    }

    if (resetTime)
        m_currentTime = 0;
}

void SpriteNode_Irrlicht::update(unsigned deltaTime)
{
    // if not paused and we have a valid animation
    if (!m_isPaused && m_sprite)
    {
        // add delta time
        m_currentTime += deltaTime;

        // if current time is bigger then the frame time advance one frame
        if (m_currentTime >= m_frameTime)
        {
            // reset time, but keep the remainder
            //m_currentTime = sf::microseconds(m_currentTime.asMicroseconds() % m_frameTime.asMicroseconds());
			m_currentTime = ((int)(m_currentTime*1000) % (int)(m_frameTime));

            // get next Frame index
            if (m_currentFrame + 1 < m_sprite->getSize())
            {
				++m_currentFrame;
            }                
            else
            {
                // animation has ended
                m_currentFrame = 0; // reset to start

                if (!m_isLooped)
                {
                    m_isPaused = true;
                }
            }

            // set the current frame, not reseting the time
            setFrame(m_currentFrame, false);
        }
    }
}

void SpriteNode_Irrlicht::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();	

	driver->setMaterial(m_material);
	//float flip = (m_isFlipped -1.f)*m_isFlipped;
	core::vector3df scale( 1.f ,-1.f,1.f);
	AbsoluteTransformation.setScale(scale);

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);		
	//driver->draw2DVertexPrimitiveList(m_vertices, 4, m_indices, 2, video::EVT_STANDARD, scene::EPT_TRIANGLES);	
	driver->drawVertexPrimitiveList(m_vertices, 4, m_indices, 2);
}

void SpriteNode_Irrlicht::OnRegisterSceneNode()
{
	if (IsVisible)
	{		
		SceneManager->registerNodeForRendering(this, ESNRP_SOLID);		
	}

	ISceneNode::OnRegisterSceneNode();
}

SpriteNode_Irrlicht::~SpriteNode_Irrlicht()
{
	if( this->Parent)
	{
		Parent->removeChild(this);
	}
}
