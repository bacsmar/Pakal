#pragma once

#include "MeshComponent.h"

namespace Pakal
{
	class _PAKALExport GridComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(GridComponent, Component);
	public:
		virtual ~GridComponent(){}

		struct Settings
		{
			bool visible = false;
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual void show() = 0;
		virtual void hide() = 0;
	};
}