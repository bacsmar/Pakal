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
		tmath::vector2df position;
		VertexDirection direction = VertexDirection::Auto;

		VertexInfo() {};
		VertexInfo(tmath::vector2df pos, VertexDirection dir = VertexDirection::Auto) : position(pos), direction(dir) {}

		void persist(Archive* a);
	};
}