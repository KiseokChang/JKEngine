#ifndef _AirREC_H
#define _AirREC_H

#include <dos.h>
#include "point32.h"
#include "recbase.h"

class AirRecord : public RecordBase
{
public:
	AirRecord();
 ~AirRecord();

	RecordBase *Duplicate();

	BOOL SetCoord(uint16 i,JKPoint32 p);
	BOOL GetCoord(uint16 i,JKPoint32& p);
	BOOL SetCoords(JKPoint32 *p);
	BOOL GetCoords(JKPoint32 *p);

	BOOL IsFirable(JKPoint32 p1,JKPoint32 p2,struct time& firetime);
};
#endif