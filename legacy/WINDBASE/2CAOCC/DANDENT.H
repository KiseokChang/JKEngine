#ifndef _DANDENT_H
#define _DANDENT_H

#include "numentry.h"

class DandaehoEntry : public NumberEntry
{
public:
	DandaehoEntry(WORD entid,WORD redkind=0,char *name="DandaehoEntry");
 ~DandaehoEntry();

	BOOL SetResKind(WORD reskind);
	WORD ResKind;
};

#endif
