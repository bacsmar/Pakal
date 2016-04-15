#include "PakalMath.h"

using namespace Pakal;

void Persist<tmath::vectorn<int, 3>>::persist(Archive* a, tmath::vector3di& vec)
{
	a->value("r", vec.x);
	a->value("g", vec.y);
	a->value("b", vec.z);
}

void Persist<tmath::rect<float>>::persist(Archive* a, tmath::rectf& rect)
{
	a->value("x", rect.left_corner.x);
	a->value("y", rect.left_corner.y);
	a->value("width", rect.size.x);
	a->value("height", rect.size.y);
}
