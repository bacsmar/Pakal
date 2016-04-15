#include "VertexInfo.h"
#include "persist/Archive.h"

using namespace Pakal;

void VertexInfo::persist(Archive* a)
{
	a->value("x", position.x);
	a->value("y", position.y);
	a->value("dir", direction);
}
