#include "MaterialManager.h"
#include "MaterialCallbacks.h"

#include <irrlicht/source/Irrlicht/OpenGLNoFixed/COGLFixedPipelineRenderer.h>

using namespace irr;
using namespace io;
using namespace gui;

bool  Pakal::MaterialManager::create_materials()
{
	LOG_INFO("[MaterialManager]: starting...");
	auto driver = m_system->get_driver();
	auto hasPixelShaderSupport = driver->queryFeature(video::EVDF_PIXEL_SHADER_2_0) && driver->queryFeature(video::EVDF_ARB_FRAGMENT_PROGRAM_1);
	if (false == hasPixelShaderSupport)
	{
		LOG_ERROR("WARNING: Pixel shaders disabled because of missing driver/hardware support.");
	}
	auto hasVertexShaderSupport = driver->queryFeature(video::EVDF_VERTEX_SHADER_2_0) && driver->queryFeature(video::EVDF_ARB_VERTEX_PROGRAM_1);
	if (false == hasVertexShaderSupport)
	{
		LOG_ERROR("WARNING: Vertex shaders disabled because of missing driver/hardware support.");
	}

	auto device = m_system->get_device();
	TransparentSpriteShaderCallBack* transparentSpriteCB = new TransparentSpriteShaderCallBack(device);
	video::COGLES2MaterialSolidCB* TransparentAlphaChannelCB = new video::COGLES2MaterialSolidCB(); 

	create_material_renderer(MaterialType::EMT_TRANSPARENT_REF, "SimpleVertex.vs", "SimpleFragment.fs", transparentSpriteCB, video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
	create_material_renderer(MaterialType::EMT_TRANSPARENT_ALPHA_CHANNEL, "COGLES2Solid.vsh", "COGLES2TransparentAlphaChannel.fsh", TransparentAlphaChannelCB, video::EMT_TRANSPARENT_ALPHA_CHANNEL);

	transparentSpriteCB->drop();	
	TransparentAlphaChannelCB->drop();
	
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
		LOG_INFO("[MaterialManager]: material created: %s %s", vsFileName.c_str(), psFileName.c_str());		
		m_shaders[type] = materialType;
		return;
	}
	LOG_WARNING("[MaterialManager]: Material error. %s %s", vsFileName.c_str(), psFileName.c_str());
}

unsigned Pakal::MaterialManager::internal_get_material(MaterialType type)
{
	auto material = m_shaders.find(type);
	ASSERT(material != m_shaders.end());
	return material->second;
}
