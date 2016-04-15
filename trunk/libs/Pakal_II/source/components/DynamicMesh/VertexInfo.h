#pragma once
#include "Config.h"
#include "PakalMath.h"

namespace Pakal
{
	class Archive;

	enum class VertexDirection
	{
		None = 0,
		Auto,
		Top,
		Right,
		Down,
		Left
	};


	struct _PAKALExport VertexInfo
	{
		VertexDirection direction;
		tmath::vector2df position;

		void persist(Archive* a);
	};
}