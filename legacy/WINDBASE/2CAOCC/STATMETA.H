#ifndef _STATMETA_H
#define _STATMETA_H

#include <dir.h>
#include "metaelem.h"

class StatViewMeta : public DLLFrame<SVMetaElement>
{
public:
	StatViewMeta(char *fname=0);
	virtual ~StatViewMeta();

	BOOL ChangeFileName(char *fname);

	virtual void Draw(JKDC& dc,RealCoordManager& coordman,JKRect32 paintrect=JKRect32(0,0,-1,-1));
	virtual BOOL Draw(JKDC& dc,RealCoordManager& coordman,char *fname,JKRect32 paintrect=JKRect32(0,0,-1,-1));

	BOOL Load();
	BOOL Save();

  char MetaFileName[MAXPATH+1];
};

#endif
