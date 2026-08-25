#ifndef _DTMANSET_H
#define _DTMANSET_H

#include "plntgman.h"
#include "tgman.h"
#include "battman.h"
#include "xbattman.h"
#include "fireman.h"
#include "howizman.h"
#include "hptman.h"
#include "airman.h"

class OCCDataManSet
{
public:
	OCCDataManSet();
	virtual ~OCCDataManSet();

	BOOL CreationError;
};
#endif
