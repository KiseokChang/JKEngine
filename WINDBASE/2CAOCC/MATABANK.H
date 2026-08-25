#ifndef _METABANK_H
#define _METABANK_H

#include "real2pix.h"
#include "metadef.h"

void SVLine(JKDC& dc,RealCoordManager& coordman,JKPoint32 p1,JKPoint32 p2,BYTE color,int32 width);
void SVArc(JKDC& dc,RealCoordManager& coordman,JKPoint32 p,int32 startmil,int32 endmil,int32 radius,BYTE color,int32 width);
#endif
