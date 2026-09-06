#ifndef _PODAECTL_H
#define _PODAECTL_H

#include "control.h"

class PodaeControl : public JKControl
{
public:
	PodaeControl(int16 index,int32 balsu,int32 interval,JKControl *parent=0);
 ~PodaeControl();

	void InitWindow();
	void EvTimerHappen(DWORD);

	int32 Balsu;
  int32 Interval;

	int16 Index;
};

#endif