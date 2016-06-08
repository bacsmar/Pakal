
#pragma once
//#ifndef __PAKAL_MATH_H_INCLUDED__
//#define __PAKAL_MATH_H_INCLUDED__

#include "Config.h"
#include "math/tm.h"
#include "math/tmg.h"
#include "persist/PersistUtils.h"

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
		using vector3di = vectorn<int, 3>;
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

			T left()
			{
				return left_corner.x;
			}
			T top()
			{
				return left_corner.y;
			}
			T right()
			{
				return left_corner.x + size.x;
			}
			T bottom()
			{
				return left_corner.y + size.y;
			}

			vectorn<T,2> top_left()
			{
				return{ left(),top() };
			}
			vectorn<T, 2> top_right()
			{
				return{ right(),top() };
			}
			vectorn<T, 2> bottom_left()
			{
				return{ left(),bottom() };
			}
			vectorn<T, 2> bottom_right()
			{
				return{ right(),bottom() };
			}

		};
		using recti = rect<int>;
		using rectu = rect<unsigned>;
		using rectf = rect<float>;
	}

	template<>
	struct Persist<tmath::vector3di>
	{
		static void persist(Archive* a, tmath::vector3di& vec);
	};

	template<>
	struct Persist<tmath::vector3df>
	{
		static void persist(Archive* a, tmath::vector3di& vec);
	};

	template<>
	struct Persist<tmath::rectf>
	{
		static void persist(Archive* a, tmath::rectf& rect);
	};

	template<>
	struct Persist<tmath::vector2di>
	{
		static void persist(Archive* a, tmath::vector2di& rect);
	};

	tmath::vector3df get_intersection_on_plane_and_line(
		const tmath::vector3df& lineVOrigin,
		const tmath::vector3df&  lineVDirection,
		const tmath::vector3df& planeNormal, 
		double planeDValue);


}
