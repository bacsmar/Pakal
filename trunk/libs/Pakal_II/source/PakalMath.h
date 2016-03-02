
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

		template<typename T>
		struct rect
		{
			rect() {}
			rect(T x, T y, T width, T height) : left_corner(x, y), size(width, height) {}
			vectorn<T,2> left_corner;
			vectorn<T,2> size;
			inline vectorn<T,2> get_length() const
			{
				return{ abs(size.x) + abs(left_corner.x), abs(size.y) + abs(left_corner.y) };
			}
		};
		using recti = rect<int>;
		using rectu = rect<unsigned>;
		using rectf = rect<float>;
	}
}