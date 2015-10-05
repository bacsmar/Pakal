////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "SpriteNode_Irrlicht.hpp"
#include "Components/Sprite.h"

using namespace irr;
using namespace scene;
using namespace Pakal;

SpriteNode_Irrlicht::SpriteNode_Irrlicht(ISceneNode* parent, ISceneManager* mgr, s32 id)
	: ISceneNode(parent,mgr, id),
    m_texture(nullptr)
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

void SpriteNode_Irrlicht::setColor(const video::SColor& color)
{
    // Update the vertices' color
    m_vertices[0].Color = color;
    m_vertices[1].Color = color;
    m_vertices[2].Color = color;
    m_vertices[3].Color = color;
}

void SpriteNode_Irrlicht::set_texture(irr::video::ITexture* texture)
{
	m_texture = texture;
}

irr::core::rectf SpriteNode_Irrlicht::getLocalBounds() const
{	
    float width = static_cast<float>(std::abs(m_frame_rect.getWidth()));
    float height = static_cast<float>(std::abs(m_frame_rect.getHeight()));

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

void SpriteNode_Irrlicht::detach()
{
	if (this->Parent)
	{
		Parent->removeChild(this);
	}
}

inline core::vector3df vector2Dto3D(const core::vector2df &v2d)
{
	return core::vector3df(v2d.X, v2d.Y, 0.0f);
}

inline irr::core::vector2di tmath_vect_to_irrlicht(const Pakal::tmath::vector2di& v)
{
	return irr::core::vector2di(v.x, v.y);
}

void SpriteNode_Irrlicht::set_frame(std::size_t frameIndex, const Sprite* sprite)
{
    if (sprite)
    {
        //calculate new vertex positions and texture coordiantes
        auto	rect = sprite->get_frame_offset(frameIndex);		

		m_frame_rect.LowerRightCorner.X = rect.LowerRightCorner.x;
		m_frame_rect.LowerRightCorner.Y = rect.LowerRightCorner.y;
		m_frame_rect.UpperLeftCorner.X = rect.UpperLeftCorner.x;
		m_frame_rect.UpperLeftCorner.Y = rect.UpperLeftCorner.y;

		auto relativepos_i = sprite->get_frame_pos(frameIndex);		
		core::vector2df		relativePos = { static_cast<irr::f32>(relativepos_i.x) , static_cast<irr::f32>(relativepos_i.y) };

		//core::dimension2du d = sprite->get_sprite_sheet()->getSize();
		core::dimension2du d = m_texture->getSize();

		auto height = m_frame_rect.LowerRightCorner.Y;
		auto width = m_frame_rect.LowerRightCorner.X;

		float left = static_cast<float>(m_frame_rect.UpperLeftCorner.X) + 0.0001f;
		float right = left + static_cast<float>(width);
		float top = static_cast<float>(m_frame_rect.UpperLeftCorner.Y);
		float bottom = top + static_cast<float>(height);

		left /= d.Width;
		right /= d.Width;
		top /= d.Height;
		bottom /= d.Height;

        m_vertices[0].Pos = vector2Dto3D(core::vector2df(0.f, 0.f) + relativePos);
        m_vertices[1].Pos = vector2Dto3D(core::vector2df(0.f, static_cast<float>(height)) + relativePos );
        m_vertices[2].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), static_cast<float>(height)) + relativePos );
        m_vertices[3].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), 0.f) + relativePos );        

        m_vertices[0].TCoords = core::vector2df(left, top);
        m_vertices[1].TCoords = core::vector2df(left, bottom);
        m_vertices[2].TCoords = core::vector2df(right, bottom);
        m_vertices[3].TCoords = core::vector2df(right, top);		

		m_box.reset(m_vertices[0].Pos);
		for (s32 i=1; i<4; ++i)
			m_box.addInternalPoint(m_vertices[i].Pos);

		m_material.setTexture(0, m_texture);
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
	detach();
}
