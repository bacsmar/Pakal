////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "SpriteNode_Irrlicht.hpp"
#include "Components/Sprite.h"
#include "MaterialManager.h"

using namespace irr;
using namespace scene;
using namespace Pakal;

SpriteNode_Irrlicht::SpriteNode_Irrlicht(ISceneNode* parent, ISceneManager* mgr)
	: ISceneNode(parent,mgr),
    m_texture(nullptr)
{
	m_buffer.Material.Lighting = false;
	m_buffer.Material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;	
	//m_buffer.Material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;			
	m_buffer.Material.FrontfaceCulling = false;		// enable both faces drawing
	m_buffer.Material.BackfaceCulling = false;

	//m_buffer.Material.ZBuffer = video::ECFN_LESSEQUAL;	//default value	
	m_buffer.Material.ZWriteEnable = true;
	//m_buffer.Material.setFlag(irr::video::EMF_BILINEAR_FILTER, true);
	//m_buffer.Material.ZWriteFineControl = video::EZI_ZBUFFER_FLAG;

	m_buffer.BoundingBox.reset(-1.0f, -1.0f, 0.0f);
	m_buffer.BoundingBox.addInternalPoint(1.0f, 1.0f, 0.0f);

	m_buffer.Vertices.set_used(4);
	m_buffer.Indices.set_used(6);
	m_buffer.Indices[0] = 0;
	m_buffer.Indices[1] = 2;
	m_buffer.Indices[2] = 1;
	m_buffer.Indices[3] = 0;
	m_buffer.Indices[4] = 3;
	m_buffer.Indices[5] = 2;

	video::SColor color(255, 255, 255, 255);
	setColor(color);	
}

SpriteNode_Irrlicht::SpriteNode_Irrlicht(ISceneNode* parent, irr::scene::ISceneManager* mgr, 
	MaterialManager* materialManager) : SpriteNode_Irrlicht(parent, mgr)
{
	//m_buffer.Material.MaterialType = materialManager->get_material(MaterialManager::MaterialType::EMT_TRANSPARENT_REF);
}

void SpriteNode_Irrlicht::setColor(const video::SColor& color)
{
    // Update the vertices' color
	m_buffer.Vertices[0].Color = color;
	m_buffer.Vertices[1].Color = color;
	m_buffer.Vertices[2].Color = color;
	m_buffer.Vertices[3].Color = color;
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
	return m_buffer.BoundingBox;
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
		
		m_buffer.Vertices[0].Pos = vector2Dto3D(core::vector2df(0.f, 0.f) + relativePos);
		m_buffer.Vertices[1].Pos = vector2Dto3D(core::vector2df(0.f, static_cast<float>(height)) + relativePos );
		m_buffer.Vertices[2].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), static_cast<float>(height)) + relativePos );
		m_buffer.Vertices[3].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), 0.f) + relativePos );
		
		// these are for texture coords (UV)
		core::dimension2du d = m_texture->getSize();
		left /= d.Width;
		right /= d.Width;
		top /= d.Height;
		bottom /= d.Height;	

		m_buffer.Vertices[0].TCoords = core::vector2df(left, top);
		m_buffer.Vertices[1].TCoords = core::vector2df(left, bottom);
		m_buffer.Vertices[2].TCoords = core::vector2df(right, bottom);
		m_buffer.Vertices[3].TCoords = core::vector2df(right, top);

		m_buffer.BoundingBox.reset(m_buffer.Vertices[0].Pos);
		for (s32 i=1; i<4; ++i)
			m_buffer.BoundingBox.addInternalPoint(m_buffer.Vertices[i].Pos);

		m_buffer.Material.setTexture(0, m_texture);		
    }    
}

void SpriteNode_Irrlicht::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();		

	driver->setMaterial(m_buffer.Material);

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);	
	driver->drawIndexedTriangleList(&m_buffer.Vertices[0], 4, &m_buffer.Indices[0], 2);
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
