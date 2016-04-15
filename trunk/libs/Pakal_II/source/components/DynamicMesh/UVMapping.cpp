#include "UVMapping.h"
#include "persist/Archive.h"

using namespace Pakal;

void UVMapping::Segment::persist(Archive* a)
{
	a->value("z_offset", z_offset);
	a->value("left_cap", left_cap);
	a->value("right_cap", right_cap);
	a->value("bodies", "body", bodies);
}

void UVMapping::persist(Archive* a)
{
	a->value("edge_size_x", edge_size.x);
	a->value("edge_size_y", edge_size.y);
	a->value("fill_size_x", fill_size.x);
	a->value("fill_size_y", fill_size.y);
	a->value("top", top);
	a->value("left", left);
	a->value("right", right);
	a->value("bottom", bottom);
}

tmath::rectf UVMapping::to_uv(tmath::rectf rect)
{
	rect.left_corner.x /= edge_size.x;
	rect.left_corner.y /= edge_size.y;

	rect.size.x /= edge_size.x;
	rect.size.y /= edge_size.y;

	return rect;
}

bool UVMapping::Segment::is_valid()
{
	return !bodies.empty();
}
