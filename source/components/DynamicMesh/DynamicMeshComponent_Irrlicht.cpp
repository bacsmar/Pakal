#include "DynamicMeshComponent_Irrlicht.h"
#include "Utils.h"

#include "irrlicht/IrrGraphicsSystem.h"
#include "irrlicht.h"
#include "irrlicht/MaterialManager.h"
#include "dimension2d.h"

using namespace Pakal;
using namespace irr::scene;
using namespace irr::video;

DynamicMeshComponent_Irrlicht::DynamicMeshComponent_Irrlicht(IrrGraphicsSystem* graphicsSystem) : m_system(graphicsSystem) , m_mesh(new irr::scene::SMesh()), m_node(nullptr) , m_fillTexture(nullptr), m_edgeTexture(nullptr), m_mapping(nullptr)
{}

DynamicMeshComponent_Irrlicht::~DynamicMeshComponent_Irrlicht()
{
	m_mesh->drop();
}

std::vector<VertexInfo>& DynamicMeshComponent_Irrlicht::get_vertices()
{
	return m_vertices;
}

BasicTaskPtr DynamicMeshComponent_Irrlicht::intialize(const Settings& settings)
{
	m_vertices = settings.vertices;
	m_mapping = settings.mapping;

	m_meshGenerator.set_ambient_color(settings.ambient_color);
	m_meshGenerator.set_fill_mode(settings.fill_mode);
	m_meshGenerator.set_is_closed(settings.is_closed);
	m_meshGenerator.set_pixels_per_unit(settings.pixels_per_unit);
	m_meshGenerator.set_smooth_factor(settings.smooth_factor);
	m_meshGenerator.set_split_corners_threshold(settings.split_corners_threshold);
	m_meshGenerator.set_split_when_different(settings.split_when_different);
	m_meshGenerator.set_strech_threshold(settings.strech_threshold);
	m_meshGenerator.set_uv_mapping(settings.mapping);

	auto fillT = settings.fill_texture;
	auto edgeT = settings.edges_texture;

	return m_system->execute_block([this,fillT,edgeT]()
	{
		m_fillTexture = m_system->get_driver()->getTexture(fillT.c_str());
		m_edgeTexture = m_system->get_driver()->getTexture(edgeT.c_str());

		if (m_edgeTexture)
		{
			auto edgeBuffer = new SMeshBuffer();
			
			m_mesh->addMeshBuffer(edgeBuffer);
			edgeBuffer->Material.setTexture(0, m_edgeTexture);
			edgeBuffer->drop();

			auto size = m_edgeTexture->getSize();
			m_mapping->edge_size = { int(size.Width), int(size.Height) };
		}

		if (m_fillTexture)
		{
			auto fillBuffer = new SMeshBuffer();

			m_mesh->addMeshBuffer(fillBuffer);
			fillBuffer->Material.setTexture(0, m_fillTexture);
			fillBuffer->drop();

			auto size = m_fillTexture->getSize();
			m_mapping->fill_size = { int(size.Width), int(size.Height) };
		}

	
		m_node = m_system->get_smgr()->addMeshSceneNode(this->m_mesh);
		m_node->setMaterialFlag(irr::video::EMF_LIGHTING, false);
		m_node->setMaterialFlag(EMF_NORMALIZE_NORMALS, true);
		m_node->setMaterialFlag(EMF_BACK_FACE_CULLING, false);
		m_node->setMaterialFlag(EMF_FRONT_FACE_CULLING, false);
		m_node->setMaterialType(m_system->get_material_manager()->get_material(MaterialManager::MaterialType::EMT_TRANSPARENT_ALPHA_CHANNEL));
		m_node->setVisible(true);
	});
}

BasicTaskPtr DynamicMeshComponent_Irrlicht::terminate()
{
	m_vertices.clear();
	m_mapping = nullptr;

	return m_system->execute_block([this]()
	{
		m_node->getParent()->removeChild(m_node);
		m_node = nullptr;
		m_fillTexture = m_edgeTexture = nullptr;
	});
}

BasicTaskPtr DynamicMeshComponent_Irrlicht::tesellate()
{
	std::vector<VertexInfo*> pointers(m_vertices.size());
	std::transform(m_vertices.begin(), m_vertices.end(), pointers.begin(), std_utils::address<VertexInfo>());


	m_meshGenerator.set_vertices(pointers);
	m_meshGenerator.tesellate();

	return m_system->execute_block([this]()
	{
		auto ambientColor = 
			SColor(255, 
			m_meshGenerator.get_ambient_color().x, 
			m_meshGenerator.get_ambient_color().y, 
			m_meshGenerator.get_ambient_color().z);


		if (m_edgeTexture)
		{
			auto edgeBuffer = static_cast<SMeshBuffer*>(m_mesh->getMeshBuffer(0));
			auto& edgeVertices = m_meshGenerator.get_edge_mesh().get_positions();
			auto& edgeUVs = m_meshGenerator.get_edge_mesh().get_uvs();
			auto& edgeIndices = m_meshGenerator.get_edge_mesh().get_indices();


			int vertexCount = edgeVertices.size();
			edgeBuffer->Vertices.clear();
			edgeBuffer->Vertices.reallocate(vertexCount);

			for (int i = 0; i < vertexCount; i++)
			{
				edgeBuffer->Vertices.push_back(S3DVertex(
					edgeVertices[i].x,
					edgeVertices[i].y,
					edgeVertices[i].z,
					1, 1, 1,
					ambientColor,
					edgeUVs[i].x,
					edgeUVs[i].y));
			}


			int indexCount = edgeIndices.size();
			edgeBuffer->Indices.clear();
			edgeBuffer->Indices.reallocate(indexCount);
			for (int i = 0; i < indexCount; i++)
			{
				edgeBuffer->Indices.push_back(edgeIndices[i]);
			}

			edgeBuffer->recalculateBoundingBox();
		}

		if (m_fillTexture)
		{
			auto fillBuffer = static_cast<SMeshBuffer*>(m_mesh->getMeshBuffer(1));
			auto& fillVertices = m_meshGenerator.get_fill_mesh().get_positions();
			auto& fillUVs = m_meshGenerator.get_fill_mesh().get_uvs();
			auto& fillIndices = m_meshGenerator.get_fill_mesh().get_indices();


			int vertexCount = fillVertices.size();
			fillBuffer->Vertices.clear();
			fillBuffer->Vertices.reallocate(vertexCount);

			for (int i = 0; i < vertexCount; i++)
			{
				fillBuffer->Vertices.push_back(S3DVertex(
					fillVertices[i].x,
					fillVertices[i].y,
					fillVertices[i].z,
					1, 1, 1,
					ambientColor,
					fillUVs[i].x,
					fillUVs[i].y));
			}


			int indexCount = fillIndices.size();
			fillBuffer->Indices.clear();
			fillBuffer->Indices.reallocate(indexCount);
			for (int i = 0; i < indexCount; i++)
			{
				fillBuffer->Indices.push_back(fillIndices[i]);
			}

			fillBuffer->recalculateBoundingBox();
		}

		m_mesh->setDirty();
	});	
}
