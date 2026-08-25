#ifndef _EQP24GLB_H
	#define _EQP24GLB_H

extern char EQPDAEDAEHQ[9];
extern char EQPDAEDAEA[9];
extern char EQPDAEDAEB[9];
extern char EQPDAEDAEC[9];
extern char EQPTITLENAME[9];
extern char EQP24NAME[13];
extern char EQP24KIND[13];
extern char EQ24SNAME[13];
extern char EQ24SKIND[13];
extern char EQ24RNAME[13];
extern char EQ24RKIND[13];
extern char EQP24001[13];
extern char EQP24002[13];
extern char EQP24003[13];


#include "jkwindef.h"
#include "joglobal.h"

class Equip24Global
{
public:
	Equip24Global();
	~Equip24Global();
  void InitName();
  void InitDefaultName();
};

#endif