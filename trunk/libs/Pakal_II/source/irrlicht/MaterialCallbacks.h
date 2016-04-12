#pragma once
#include "IrrGraphicsSystem.h"

namespace Pakal
{
	//////////////////////////////////////////////////////////////////////////
	class TransparentSpriteShaderCallBack : public irr::video::IShaderConstantSetCallBack
	{
		irr::IrrlichtDevice* m_device = nullptr;

	protected:
		bool FirstUpdate = true;

		irr::s32 WVPMatrixID = -1;
		irr::s32 WVMatrixID = -1;
		irr::s32 NMatrixID = -1;

		irr::s32 TMatrix0ID = -1;
		irr::s32 AlphaRefID = -1;
		irr::s32 TextureUsage0ID = -1;
		irr::s32 TextureUnit0ID = -1;

		irr::f32 AlphaRef = 0.5f;
		irr::s32 TextureUsage0 = 0;
		irr::s32 TextureUnit0 = 0;
		irr::core::matrix4 textureMatrix;
	public:
		explicit TransparentSpriteShaderCallBack(irr::IrrlichtDevice* device) : m_device(device)
		{}

		void OnSetMaterial(const irr::video::SMaterial& material) override
		{
			AlphaRef = material.MaterialTypeParam;
			textureMatrix = material.getTextureMatrix(0);
		};

		void OnSetConstants(irr::video::IMaterialRendererServices* services, irr::s32 userData) override
		{
			if (FirstUpdate)
			{
				WVPMatrixID = services->getVertexShaderConstantID("uWVPMatrix");
				WVMatrixID = services->getVertexShaderConstantID("uWVMatrix");
				NMatrixID = services->getVertexShaderConstantID("uNMatrix");
				TMatrix0ID = services->getVertexShaderConstantID("uTMatrix0");

				AlphaRefID = services->getPixelShaderConstantID("uAlphaRef");
				TextureUnit0ID = services->getPixelShaderConstantID("uTextureUnit0");

				FirstUpdate = false;
			}
			irr::video::IVideoDriver* driver = services->getVideoDriver();

			irr::core::matrix4 textureMatrix0 = driver->getTransform(irr::video::ETS_TEXTURE_0);
			irr::core::matrix4 textureMatrix1 = driver->getTransform(irr::video::ETS_TEXTURE_1);

			const irr::core::matrix4 W = driver->getTransform(irr::video::ETS_WORLD);
			const irr::core::matrix4 V = driver->getTransform(irr::video::ETS_VIEW);
			const irr::core::matrix4 P = driver->getTransform(irr::video::ETS_PROJECTION);

			// vertex shader parameters
			irr::core::matrix4 Matrix = P * V * W;
			services->setVertexShaderConstant(WVPMatrixID, Matrix.pointer(), 16);

			Matrix = V * W;
			services->setVertexShaderConstant(WVMatrixID, Matrix.pointer(), 16);

			Matrix.makeInverse();
			services->setVertexShaderConstant(NMatrixID, Matrix.getTransposed().pointer(), 16);

			services->setVertexShaderConstant(TMatrix0ID, textureMatrix.pointer(), 1);

			// fragment shader parameters

			services->setPixelShaderConstant(TextureUnit0ID, &TextureUnit0, 1);
			services->setPixelShaderConstant(AlphaRefID, &AlphaRef, 1);

		}
	};


	//////////////////////////////////////////////////////////////////////////
	class NoTextureShaderCallBack : public irr::video::IShaderConstantSetCallBack
	{
		irr::IrrlichtDevice* m_device = nullptr;

	protected:
		bool FirstUpdate = true;

		irr::s32 WVPMatrixID = -1;		
	public:
		explicit NoTextureShaderCallBack(irr::IrrlichtDevice* device) : m_device(device)
		{}

		void OnSetMaterial(const irr::video::SMaterial& material) override
		{			
		};

		void OnSetConstants(irr::video::IMaterialRendererServices* services, irr::s32 userData) override
		{
			if (FirstUpdate)
			{
				WVPMatrixID = services->getVertexShaderConstantID("uWVPMatrix");
				FirstUpdate = false;
			}
			irr::video::IVideoDriver* driver = services->getVideoDriver();

			irr::core::matrix4 textureMatrix0 = driver->getTransform(irr::video::ETS_TEXTURE_0);
			irr::core::matrix4 textureMatrix1 = driver->getTransform(irr::video::ETS_TEXTURE_1);

			const irr::core::matrix4 W = driver->getTransform(irr::video::ETS_WORLD);
			const irr::core::matrix4 V = driver->getTransform(irr::video::ETS_VIEW);
			const irr::core::matrix4 P = driver->getTransform(irr::video::ETS_PROJECTION);

			// vertex shader parameters
			irr::core::matrix4 Matrix = P * V * W;
			services->setVertexShaderConstant(WVPMatrixID, Matrix.pointer(), 16);			

		}
	};
}