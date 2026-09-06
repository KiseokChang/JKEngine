#ifndef _MT_PLLN_H
#define _MT_PLLN_H

#include "pnt32que.h"
#include "metaelem.h"

class MetaPolyLine : public SVMetaElement
{
public:
	MetaPolyLine();
	virtual ~MetaPolyLine();

	void Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect=JKRect32(0,0,-1,-1));
	BOOL GetDrawRealRect(JKRect32& rect);

	uint32 GetFileSize();
	BOOL Get(FILE *fp);
	BOOL Put(FILE *fp);

	BOOL IsEqual(SVMetaElement *,uint16);
	SVMetaElement *Duplicate();

  uint32 MinimizePointCount(int32 delta=3);

	Point32Que PolyPoint;
	BYTE Color;
  int32 Width;
};
#endif
