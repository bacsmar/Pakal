#pragma once
#include "cmath"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	namespace math
	{
		inline float angle_between(const tmath::vector2df& fst, const tmath::vector2df& snd)
		{
			float sin = fst.x * snd.y - snd.x * fst.y;
			float cos = fst.x * snd.x + fst.y * snd.y;

			return static_cast<float>(std::atan2(sin, cos)) * float(180 / tmg::pi);
		}

		inline float distance(const tmath::vector2df& fst, const tmath::vector2df& snd)
		{
			return (snd - fst).get_length();
		}

		template<class T>
		inline void swap(T& fst, T& snd)
		{
			T tmp = fst;
			fst = snd;
			snd = tmp;
		}

		inline tmath::vector2df normal(const tmath::vector2df& vector, const tmath::vector2df& origin)
		{
			auto direction = vector - origin;

			tmath::vector2df vec = { -direction.y,direction.x };
			return vec.normalize();
		}

		inline float hermite(float v1, float v2, float v3, float v4, float aPercentage, float aTension, float aBias)
		{
			float mu2 = aPercentage * aPercentage;
			float mu3 = mu2 * aPercentage;
			float m0 = (v2 - v1) * (1 + aBias) * (1 - aTension) / 2;
			m0 += (v3 - v2) * (1 - aBias) * (1 - aTension) / 2;
			float m1 = (v3 - v2) * (1 + aBias) * (1 - aTension) / 2;
			m1 += (v4 - v3) * (1 - aBias) * (1 - aTension) / 2;
			float a0 = 2 * mu3 - 3 * mu2 + 1;
			float a1 = mu3 - 2 * mu2 + aPercentage;
			float a2 = mu3 - mu2;
			float a3 = -2 * mu3 + 3 * mu2;

			return a0 * v2 + a1 * m0 + a2 * m1 + a3 * v3;
		}

		inline tmath::vector2df hermite_lerp(const tmath::vector2df& a, const tmath::vector2df& b, const tmath::vector2df& c, const tmath::vector2df& d, float percentage, float tension = 0, float bias = 0)
		{
			return{
				hermite(a.x, b.x, c.x, d.x, percentage, tension, bias),
				hermite(a.y, b.y, c.y, d.y, percentage, tension, bias)
			};
		}

	}

	namespace utils
	{
		template<class T>
		inline T* at(std::vector<T*>& source, int i, bool looped = false)
		{
			int n = source.size();

			if (i >= n)
				return looped ? source[(i % n + n) % n] : nullptr;

			if (i < 0)
				return looped ? source[(i % n + n) % n] : nullptr;

			return source[i];
		}

		template<class T>
		inline T* first_valid(std::initializer_list<T*> values)
		{
			for(T* value : values)
				if (value) return value;

			return nullptr;
		}

	}
}
