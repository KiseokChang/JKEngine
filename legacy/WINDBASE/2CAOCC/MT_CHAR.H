#ifndef _MT_CHAR_H
#define _MT_CHAR_H

#include "pnt32que.h"
#include "metaelem.h"

class MetaTextOut : public SVMetaElement
{
public:
	MetaTextOut(char *text=0);
	virtual ~MetaTextOut();

  BOOL SetText(char *buf);

	void Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect=JKRect32(0,0,-1,-1));
	BOOL GetDrawRealRect(JKRect32& rect);

	uint32 GetFileSize();
	BOOL Get(FILE *fp);
	BOOL Put(FILE *fp);

	BOOL IsEqual(SVMetaElement *,uint16);
	SVMetaElement *Duplicate();

	JKPoint32 Position;
	char   *TextBuffer;
	JKPoint32 FontSize;
	BOOL    TextAlign;
	int32   RotateAngle;
	int32   SlantAngle;
	BYTE Color;
};
#endif
