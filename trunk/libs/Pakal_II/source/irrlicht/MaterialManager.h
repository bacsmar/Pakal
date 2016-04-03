#pragma once
#include "IrrGraphicsSystem.h"

#include <irrlicht.h>
#include <string>
#include "ResourceManager.h"
#include "OSManager.h"

namespace Pakal
{
	class BasicTask;
	class Component;
	class IDebugDrawerClient;
	struct RendererInfo;
	class OSManager;

	class _PAKALExport MaterialManager
	{
	public:

		enum class MaterialType
		{
			EMT_TRANSPARENT_SPRITE
		};

		virtual ~MaterialManager()
		{
		}

		explicit MaterialManager(IrrGraphicsSystem* irrlichtGraphicsSystem) : m_system(irrlichtGraphicsSystem) {}

		inline unsigned get_material(MaterialType type) const
		{
			return m_call_delegate(type);
		}

	protected:
		virtual unsigned internal_get_material(MaterialType type);

	private:
		IrrGraphicsSystem* m_system = nullptr;
		std::unordered_map<MaterialType,int> m_shaders;	// key = our type, value = irrlicht type

		virtual bool create_materials();
		void create_material_renderer(MaterialType type, const std::string vsFileName, const std::string& psFileName, 
										irr::video::IShaderConstantSetCallBack* cb, irr::video::E_MATERIAL_TYPE irrType = irr::video::EMT_SOLID);

		std::function<unsigned(MaterialType type)> m_call_delegate = [=](MaterialType type)
		{
			// first time usage.. load shaders
			this->create_materials();
			// then, set the this function as bypass
			this->m_call_delegate = [=](MaterialType _type) { return this->internal_get_material(_type); };
			// then call it
			return this->m_call_delegate(type);
		};			
	};	
}