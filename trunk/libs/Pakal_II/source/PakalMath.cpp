#include "PakalMath.h"
#include "persist/Archive.h"

using namespace Pakal;

void Persist<tmath::vectorn<int, 3>>::persist(Archive* a, tmath::vector3di& vec)
{
	a->value("x", vec.x);
	a->value("y", vec.y);
	a->value("z", vec.z);
}

void Persist<tmath::vectorn<float, 3>>::persist(Archive* a, tmath::vector3df& vec)
{
	a->value("x", vec.x);
	a->value("y", vec.y);
	a->value("z", vec.z);
}

void Persist<tmath::rect<float>>::persist(Archive* a, tmath::rectf& rect)
{
	a->value("x", rect.left_corner.x);
	a->value("y", rect.left_corner.y);
	a->value("width", rect.size.x);
	a->value("height", rect.size.y);
}

void Persist<tmath::vectorn<int, 2>>::persist(Archive* a, tmath::vector2di& rect)
{
	a->value("x", rect.x);
	a->value("y", rect.y);
}

Pakal::tmath::vector3df Pakal::get_intersection_on_plane_and_line(
	const Pakal::tmath::vector3df& lineVOrigin, 
	const Pakal::tmath::vector3df& lineVDirection, 
	const Pakal::tmath::vector3df& plane_normal, double plane_D_value)
{
	//TODO check for the plane origin to rest that vector on the point and the vector;
	auto vecPV = lineVDirection - lineVOrigin;
	//-----calculate the parametic ecuation using the point and the vector
	//--calculate the value T using the parametric ecuation
	auto escalars_results = (lineVOrigin.x*plane_normal.x) + (lineVOrigin.y*plane_normal.y) + (lineVOrigin.z*plane_normal.z) + plane_D_value;
	auto t_results = (vecPV.x*plane_normal.x) + (vecPV.y*plane_normal.y) + (vecPV.z*plane_normal.z);
	if (t_results == 0) return nullptr;//--if the T values is 0 means that the line doesn't intersect the plane
	auto t_value = -escalars_results / t_results;
	//---calculate the point
	Pakal::tmath::vector3df intersection_point;
	intersection_point.x = lineVOrigin.x + float(vecPV.x*t_value);
	intersection_point.y = lineVOrigin.y + float(vecPV.y*t_value);
	intersection_point.z = lineVOrigin.z + float(vecPV.z*t_value);
	return intersection_point;
}
