#include "MaterialManager.h"
#include "MaterialCallbacks.h"
#include <irrlicht/source/Irrlicht/COpenGLSLMaterialRenderer.h>
#include "IVertexDescriptor.h"

using namespace irr;
using namespace io;
using namespace gui;

bool  Pakal::MaterialManager::create_materials()
{
	auto driver = m_system->get_driver();
	auto hasPixelShaderSupport = driver->queryFeature(video::EVDF_PIXEL_SHADER_2_0) && driver->queryFeature(video::EVDF_ARB_FRAGMENT_PROGRAM_1);
	if (false == hasPixelShaderSupport)
	{
		LOG_WARNING("WARNING: Pixel shaders disabled because of missing driver/hardware support.");
	}
	auto hasVertexShaderSupport = driver->queryFeature(video::EVDF_VERTEX_SHADER_2_0) && driver->queryFeature(video::EVDF_ARB_VERTEX_PROGRAM_1);
	if (false == hasVertexShaderSupport)
	{
		LOG_WARNING("WARNING: Vertex shaders disabled because of missing driver/hardware support.");
	}

	auto device = m_system->get_device();
	MyShaderCallBack* transparentSpriteCB = new MyShaderCallBack(device);

	create_material_renderer(MaterialType::EMT_TRANSPARENT_SPRITE, "SimpleVertex.vs", "SimpleFragment.fs", transparentSpriteCB);
	transparentSpriteCB->drop();
	
	return m_shaders.size() > 0;
}

void Pakal::MaterialManager::create_material_renderer(MaterialType type, const std::string vsFileName, const std::string& psFileName,
	irr::video::IShaderConstantSetCallBack* cb, irr::video::E_MATERIAL_TYPE irrType)
{	
	std::string fragmentShader;
	std::string vertexShader;
	SharedPtr<std::istream> resource;	

	resource = ResourceMgr.open_read_resource(vsFileName);
	ASSERT(resource->good());
	Pakal::file_utils::read_to_string(*resource, vertexShader);

	resource = ResourceMgr.open_read_resource(psFileName);
	ASSERT(resource->good());
	Pakal::file_utils::read_to_string(*resource, fragmentShader);

	
	// create materials
	auto driver = m_system->get_driver();
	irr::video::IGPUProgrammingServices* gpu = driver->getGPUProgrammingServices();

	irr::s32 materialType = gpu->addHighLevelShaderMaterial(
		vertexShader.c_str(), "main", video::EVST_VS_2_0,
		fragmentShader.c_str(), "main", video::EPST_PS_2_0,
		cb,
		irrType, 0, video::EGSL_DEFAULT);

	if (materialType != -1)
	{
		LOG_INFO("[MaterialManager]: shader parsed. %s %s", vsFileName.c_str(), psFileName.c_str());
		auto renderer = static_cast<video::COpenGLSLMaterialRenderer*>(driver->getMaterialRenderer(materialType));		

		auto descriptor = driver->addVertexDescriptor((video::E_MATERIAL_TYPE)materialType, video::EVT_STANDARD);

		auto id = renderer->getVertexShaderAttribID("inVertexPosition");
		descriptor->addAttribute("inVertexPosition", 3, video::EVAS_POSITION, video::EVAT_FLOAT, id);

		id = renderer->getVertexShaderAttribID("inVertexNormal");
		descriptor->addAttribute("inVertexNormal", 3, video::EVAS_NORMAL, video::EVAT_FLOAT, id);

		id = renderer->getVertexShaderAttribID("inVertexColor");
		descriptor->addAttribute("inVertexColor", 4, video::EVAS_COLOR, video::EVAT_UBYTE, id);

		id = renderer->getVertexShaderAttribID("inTexCoord0");
		descriptor->addAttribute("inTexCoord0", 2, video::EVAS_TEXCOORD0, video::EVAT_FLOAT, id);
		
		m_shaders[type] = materialType;
		return;
	}
	LOG_WARNING("[MaterialManager]: shader error. %s %s", vsFileName.c_str(), psFileName.c_str());
}

unsigned Pakal::MaterialManager::internal_get_material(MaterialType type)
{
	auto material = m_shaders.find(type);
	ASSERT(material != m_shaders.end());
	return material->second;
}
