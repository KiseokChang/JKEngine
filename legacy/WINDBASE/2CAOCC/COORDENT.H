#ifndef _COORDENT_H
#define _COORDENT_H

#include "realpos.h"
#include "entbase.h"

class CoordEntry : public RealPosition, public EntryBase
{
public:
	CoordEntry(WORD entid,int8 *name="CoordEntry",JKPoint32 init=JKPoint32(0,0));
 ~CoordEntry();

	EntryBase *Duplicate();
	int16  Compare(EntryBase *ent,uint16);
	BOOL SetData(BYTE *data);
	BOOL GetData(BYTE *data);
	void Get(FILE *fp);
	void Put(FILE *fp);

	BOOL SetCoord(JKPoint32& coord);
	BOOL GetCoord(JKPoint32& coord);

	JKPoint32 DataCoord;
};
#endif
