#include "XmlWriter.h"
#include "Element.h"

using namespace Pakal;

void XmlWriter::write(std::ostream& ostream,const  Element* element)
{
	ASSERT(element != nullptr);
	ostream << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	write_element(ostream, element, 0);
}

void XmlWriter::write_element(std::ostream& stream,const  Element* element, int depth)
{
	ASSERT(element != nullptr);

	write_indent(stream, depth);
	stream << "<" << element->name().c_str();

	std::list<Attribute>::const_iterator attribute = element->attributes().begin();
	while (attribute != element->attributes().end())
	{
		const std::string& value = attribute->string();

		stream << " " << attribute->name() << "=\"";

		std::string::const_iterator character = value.begin();
		while (character != value.end())
		{
			switch (*character)
			{
			case '&':
				stream << "&amp;";
				break;

			case '<':
				stream << "&lt;";
				break;

			case '>':
				stream << "&gt;";
				break;

			case '"':
				stream << "&quot;";
				break;

			default:
				stream << *character;
				break;
			}

			++character;
		}

		stream << "\"";

		++attribute;
	}

	if (element->is_leaf())
	{
		if (element->attributes().empty())
		{
			stream << " />\n";
		}
		else
		{
			stream << "/>\n";
		}
	}
	else
	{
		stream << ">\n";

		std::list<Element>::const_iterator child = element->elements().begin();
		while (child != element->elements().end())
		{
			write_element(stream, &(*child), depth + 1);
			++child;
		}

		write_indent(stream, depth);
		stream << "</" << element->name() << ">\n";
	}
}

void XmlWriter::write_indent(std::ostream& stream, int indent)
{
	for (int i = 0; i < indent; ++i)
	{
		stream << "    ";
	}
}