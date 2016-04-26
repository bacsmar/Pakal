#include "PakalMath.h"
#include "persist/Archive.h"

using namespace Pakal;

void Persist<tmath::vectorn<int, 3>>::persist(Archive* a, tmath::vector3di& vec)
{
	a->value("x", vec.x);
	a->value("y", vec.y);
	a->value("z", vec.z);
}

void Persist<tmath::vectorn<float, 3>>::persist(Archive* a, tmath::vector3di& vec)
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
