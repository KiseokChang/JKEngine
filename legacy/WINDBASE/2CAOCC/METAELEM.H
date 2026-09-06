#ifndef _METAELEM_H
#define _METAELEM_H

#include <stdio.h>
#include "metabank.h"

class SVMetaElement
{
public:
	SVMetaElement(WORD metatype=SVMETA_BASE);
	virtual ~SVMetaElement();

	virtual void Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect=JKRect32(0,0,-1,-1))=0;
	virtual BOOL GetDrawRealRect(JKRect32& rect)=0;

  virtual uint32 GetFileSize()=0;
	virtual BOOL Get(FILE *fp)=0;
	virtual BOOL Put(FILE *fp)=0;

	virtual BOOL IsEqual(SVMetaElement *,uint16);
	virtual SVMetaElement *Duplicate()=0;


  WORD MetaType;
	SVMetaElement *prev,*next;
};

#endif
