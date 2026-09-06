#ifndef _KGRAPHIC_H
#define _KGRAPHIC_H

#include "jkpoint.h"
#include "graph_pm.h"

extern BOOL IsInitGraph;
BOOL InitGraph();
void CloseGraph();

int16 getmaxx();
int16 getmaxy();

BOOL Put256IMG(JKPoint p,BYTE *Image,JKRect rect=JKRect(0,0,-1,-1),BYTE putmode=0);
BOOL Get256IMG(JKPoint p,BYTE *Image,JKRect rect);
BOOL Get256Image(JKRect rect,BYTE *Image);
/////////////////////////////////////////////////////////////////////////////////
void SetAllPalette(BYTE palette[768]);
void GetAllPalette(BYTE palette[768]);
void SetPalette(uint16 color,BYTE r,BYTE g,BYTE b);
void GetPalette(uint16 color,BYTE& r,BYTE& g,BYTE& b);

BYTE IndexFromEachRGB(BYTE rgb);
BYTE FindNearestColor(BYTE r,BYTE g,BYTE b);
#endif
