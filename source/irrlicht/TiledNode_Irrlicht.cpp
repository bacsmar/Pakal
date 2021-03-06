////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "TiledNode_Irrlicht.hpp"
#include "Components/Sprite.h"
#include "MaterialManager.h"

using namespace irr;
using namespace scene;
using namespace Pakal;

TiledNode_Irrlicht::TiledNode_Irrlicht(ISceneNode* parent, ISceneManager* mgr)
	: ISceneNode(parent,mgr),
    m_texture(nullptr)
{
	m_vertices.reserve(4);
	m_material.Lighting = true;	
	//m_material.AmbientColor = scolor;
	//m_material.DiffuseColor = scolor;	
	m_material.EmissiveColor = m_material.AmbientColor;
	m_material.GouraudShading = false;
	m_material.ZBuffer = video::ECFN_DISABLED;	// disable Z buffer test...	
	m_material.FrontfaceCulling = false;		// enable both faces drawing
	m_material.BackfaceCulling = false;

	//m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	m_material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
	m_material.MaterialTypeParam = 0.5f;
	//m_material.MaterialType = video::EMT_SOLID;	

	m_box.reset(-1.0f,-1.0f,0.0f);
	m_box.addInternalPoint(1.0f,1.0f,0.0f);
}

TiledNode_Irrlicht::TiledNode_Irrlicht(ISceneNode* parent, irr::scene::ISceneManager* mgr, MaterialManager* materialManager) : TiledNode_Irrlicht(parent, mgr)
{
	m_material.MaterialType = static_cast<video::E_MATERIAL_TYPE>(materialManager->get_material(MaterialManager::MaterialType::EMT_TRANSPARENT_REF));;
}

void TiledNode_Irrlicht::setColor(const video::SColor& color)
{
    // Update the vertices' color
	for( auto & vertex : m_vertices)
	{
		vertex.Color = color;
	}
}

void TiledNode_Irrlicht::set_texture(irr::video::ITexture* texture)
{
	m_texture = texture;
}

core::rectf TiledNode_Irrlicht::getLocalBounds() const
{
    return irr::core::rectf(0.f, 0.f, (f32)m_frame_rect.LowerRightCorner.X, (f32)m_frame_rect.LowerRightCorner.Y); 
}

irr::core::rectf TiledNode_Irrlicht::getGlobalBounds() const
{
	return getLocalBounds();
}
const core::aabbox3d<f32>& TiledNode_Irrlicht::getBoundingBox() const
{
	return m_box;
}

void TiledNode_Irrlicht::detach()
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

void TiledNode_Irrlicht::set_tile(std::size_t frameIndex, const Sprite& sprite, const tmath::vector2df& offset)
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
		
		irr::video::S3DVertex			vertices[4];		

		vertices[0].Pos = vector2Dto3D(core::vector2df(0.f, 0.f) + relativePos);
		vertices[1].Pos = vector2Dto3D(core::vector2df(0.f, static_cast<float>(height)) + relativePos );
		vertices[2].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), static_cast<float>(height)) + relativePos );
		vertices[3].Pos = vector2Dto3D(core::vector2df(static_cast<float>(width), 0.f) + relativePos );
		
		// these are for texture coords (UV)
		core::dimension2du d = m_texture->getSize();
		left /= d.Width;
		right /= d.Width;
		top /= d.Height;
		bottom /= d.Height;	

		vertices[0].TCoords = core::vector2df(left, top);
		vertices[1].TCoords = core::vector2df(left, bottom);
		vertices[2].TCoords = core::vector2df(right, bottom);
		vertices[3].TCoords = core::vector2df(right, top);			

		irr::u16 indices[6];
		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 0;
		indices[4] = 3;
		indices[5] = 2;
		/*
		   v0-----v3
		  /  \   /
		 /    \ /
		v1-----v2
		*/

		m_box.reset(vertices[0].Pos);
		for (s32 i=1; i<4; ++i)
			m_box.addInternalPoint(vertices[i].Pos);

		for (int i= 0; i < 4; i++)
		{
			vertices[i].Pos.X += offset.x;
			vertices[i].Pos.Y += offset.y;
			m_vertices.push_back(vertices[i]);;
		}			

		irr::u16 indexSize = ((u16)m_indices.size()/6) * 4; // 4  vertices every iteration....
		for (int i = 0; i < 6; i++)
		{			
			m_indices.push_back( indices[i] + indexSize );
		}

		m_material.setTexture(0, m_texture);
    }
}

void TiledNode_Irrlicht::clear_draw_surface()
{
	m_vertices.clear();
}

void TiledNode_Irrlicht::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();		

	driver->setMaterial(m_material);

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	//driver->drawVertexPrimitiveList(&m_vertices[0], m_vertices.size(), &m_indices[0], (m_vertices.size()/4)*2);	
	driver->drawIndexedTriangleList(&m_vertices[0], m_vertices.size(), &m_indices[0], (m_vertices.size()/4)*2);	
}

void TiledNode_Irrlicht::OnRegisterSceneNode()
{
	if (IsVisible)
	{		
		SceneManager->registerNodeForRendering(this);
	}

	ISceneNode::OnRegisterSceneNode();
}

TiledNode_Irrlicht::~TiledNode_Irrlicht()
{
	detach();
}
