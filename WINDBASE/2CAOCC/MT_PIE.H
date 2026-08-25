#ifndef _MT_PIE_H
#define _MT_PIE_H

#include "pnt32que.h"
#include "metaelem.h"

class MetaPieslice : public SVMetaElement
{
public:
	MetaPieslice();
	virtual ~MetaPieslice();

//	void SVLine(JKDC& dc,RealCoordManager& coordman,JKPoint32 p1,JKPoint32 p2);
	void Draw(JKDC& dc,RealCoordManager& coordman,JKPoint32 org=JKPoint32());
	BOOL GetDrawRealRect(JKRect32& rect);

	uint32 GetFileSize();
	BOOL Get(FILE *fp);
	BOOL Put(FILE *fp);

	BOOL IsEqual(SVMetaElement *,uint16);
	SVMetaElement *Duplicate();

	Point32Que PolyPoint;
	BYTE Color;
	int32 Width;
};
#endif
