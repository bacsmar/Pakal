
#pragma once
//#ifndef __PAKAL_MATH_H_INCLUDED__
//#define __PAKAL_MATH_H_INCLUDED__

#include "Config.h"
#include "math/tm.h"
//#include <math.h>
//#include <float.h>
//#include <stdlib.h> // for abs() etc.
//#include <limits.h> // For INT_MAX / UINT_MAX


namespace Pakal
{
	namespace tmath
	{
		using vector2df = vectorn<float, 2>;
		using vector2di = vectorn<int, 2>;
		using vector2du = vectorn<unsigned, 2>;
		using vector3df = vectorn<float, 3>;

		struct recti
		{
			recti() {}
			recti(int x, int y, int width, int height) : left_corner(x, y), size(width, height) {}
			vector2di left_corner;
			vector2di size;
		};
	}
}