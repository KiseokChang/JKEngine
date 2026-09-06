#include "metaelem.h"

SVMetaElement::SVMetaElement(WORD metatype):
	MetaType(metatype),prev(0),next(0)
{}
SVMetaElement::~SVMetaElement()
{}


BOOL SVMetaElement::IsEqual(SVMetaElement *elem,uint16)
{
	if(elem&&MetaType==elem->MetaType) return TRUE;
  return FALSE;
}
