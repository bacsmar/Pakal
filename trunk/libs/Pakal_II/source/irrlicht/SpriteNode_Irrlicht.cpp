////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "SpriteNode_Irrlicht.hpp"
#include "Components/Sprite.h"

using namespace irr;
using namespace scene;
using namespace Pakal;

SpriteNode_Irrlicht::SpriteNode_Irrlicht(ISceneNode* parent, ISceneManager* mgr)
	: ISceneNode(parent,mgr),
    m_texture(nullptr)
{
	m_material.Lighting = true;	
	//m_material.AmbientColor = scolor;
	//m_material.DiffuseColor = scolor;	
	m_material.EmissiveColor = m_material.AmbientColor;
	m_material.GouraudShading = false;
	m_material.ZBuffer = video::ECFN_DISABLED;	// disable Z buffer test...	
	//m_material.ZBuffer = video::ECFN_LESSEQUAL;	// 
	m_material.FrontfaceCulling = false;		// enable both faces drawing
	m_material.BackfaceCulling = false;

	//-Material renderers which offers blending feature(eg.EMT_TRANSPARENT_ALPHA_CHANNEL, EMT_ONETEXTURE_BLEND etc.) require SMaterial::BlendOperation set to other value than EBO_NONE.
	m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	//m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
	//m_material.MaterialType = video::EMT_SOLID;	

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

core::rectf SpriteNode_Irrlicht::getLocalBounds() const
{
    return irr::core::rectf(0.f, 0.f, (f32)m_frame_rect.LowerRightCorner.X, (f32)m_frame_rect.LowerRightCorner.Y); 
}

irr::core::rectf SpriteNode_Irrlicht::getGlobalBounds() const
{
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
	return core::vector3df(v2d.X, -v2d.Y, 0.0f);
}

void SpriteNode_Irrlicht::set_frame(std::size_t frameIndex, const Sprite& sprite)
{
    {
        //calculate new vertex positions and texture coordiantes
        auto rect = sprite.get_frame(frameIndex).texture_rect;		

		m_frame_rect.UpperLeftCorner.X = rect.left_corner.x;
		m_frame_rect.UpperLeftCorner.Y = rect.left_corner.y;

		m_frame_rect.LowerRightCorner.X = rect.size.x;
		m_frame_rect.LowerRightCorner.Y = rect.size.y;


		tmath::vector2df rpos = sprite.get_frame(frameIndex).relative_pos();		
		core::vector2df	 relativePos = { rpos.x , rpos.y };

		auto height = m_frame_rect.LowerRightCorner.Y;
		auto width = m_frame_rect.LowerRightCorner.X;

		float left = static_cast<float>(m_frame_rect.UpperLeftCorner.X) +0.0001f;
		float right = left + static_cast<float>(width);
		float top = static_cast<float>(m_frame_rect.UpperLeftCorner.Y);
		float bottom = top + static_cast<float>(height);		
		
        m_vertices[0].Pos = vector2Dto3D(core::vector2df(0.f, 0.f) + relativePos);
        m_vertices[1].Pos = vector2Dto3D(core::vector2df(0.f, static_cast<float>(height)) + relativePos );
        m_vertices[2].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), static_cast<float>(height)) + relativePos );
        m_vertices[3].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), 0.f) + relativePos );
		
		// these are for texture coords (UV)
		core::dimension2du d = m_texture->getSize();
		left /= d.Width;
		right /= d.Width;
		top /= d.Height;
		bottom /= d.Height;	

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

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	//driver->drawVertexPrimitiveList(m_vertices, 4, m_indices, 2);
	driver->drawIndexedTriangleList(m_vertices, 4, m_indices, 2);
}

void SpriteNode_Irrlicht::OnRegisterSceneNode()
{
	if (IsVisible)
	{
		SceneManager->registerNodeForRendering(this);
	}

	ISceneNode::OnRegisterSceneNode();
}

SpriteNode_Irrlicht::~SpriteNode_Irrlicht()
{
	detach();
}
