#include "eqp24glb.h"
#include <string.h>


char EQPDAEDAEHQ[9];
char EQPDAEDAEA[9];
char EQPDAEDAEB[9];
char EQPDAEDAEC[9];
char EQPTITLENAME[9];
char EQP24NAME[13];
char EQP24KIND[13];
char EQ24SNAME[13];
char EQ24SKIND[13];
char EQ24RNAME[13];
char EQ24RKIND[13];
char EQP24001[13];
char EQP24002[13];
char EQP24003[13];

Equip24Global::Equip24Global()
{
	if(strlen(BudaeName))	InitName();
  else InitDefaultName();
}

Equip24Global::~Equip24Global()
{}

void Equip24Global::InitName()
{
	strcpy(EQPDAEDAEHQ,BudaeName);
	strcat(EQPDAEDAEHQ,"-HQ");
	strcpy(EQPDAEDAEA,BudaeName);
	strcat(EQPDAEDAEA,"-A");
	strcpy(EQPDAEDAEB,BudaeName);
	strcat(EQPDAEDAEB,"-B");
	strcpy(EQPDAEDAEC,BudaeName);
	strcat(EQPDAEDAEC,"-C");

	strcpy(EQPTITLENAME,BudaeName);
	strcat(EQPTITLENAME," ””");

	strcpy(EQP24NAME,BudaeName);
	strcat(EQP24NAME,"name.dat");

	strcpy(EQP24KIND,BudaeName);
	strcat(EQP24KIND,"kind.dat");

	strcpy(EQ24SNAME,BudaeName);
	strcat(EQ24SNAME,"snme.dat");

	strcpy(EQ24SKIND,BudaeName);
	strcat(EQ24SKIND,"sknd.dat");

	strcpy(EQ24RNAME,BudaeName);
	strcat(EQ24RNAME,"rnme.dat");

	strcpy(EQ24RKIND,BudaeName);
	strcat(EQ24RKIND,"rknd.dat");

	strcpy(EQP24001,BudaeName);
	strcat(EQP24001,"001.cfg");
	strcpy(EQP24002,BudaeName);
	strcat(EQP24002,"002.cfg");
	strcpy(EQP24003,BudaeName);
	strcat(EQP24003,"003.cfg");
}

void Equip24Global::InitDefaultName()
{
	strcpy(EQPDAEDAEHQ,"855-HQ");
	strcpy(EQPDAEDAEA,"855-A");
	strcpy(EQPDAEDAEB,"855-B");
	strcpy(EQPDAEDAEC,"855-C");

	strcpy(EQPTITLENAME,"855 ””");

	strcpy(EQP24NAME,"855name.dat");
	strcpy(EQP24KIND,"855kind.dat");

	strcpy(EQ24SNAME,BudaeName);
	strcat(EQ24SNAME,"snme.dat");

	strcpy(EQ24SKIND,"855sknd.dat");

	strcpy(EQ24RNAME,"855rnme.dat");

	strcpy(EQ24RKIND,"855rknd.dat");

	strcpy(EQP24001,"855001.cfg");
	strcpy(EQP24002,"855002.cfg");
	strcpy(EQP24003,"855003.cfg");
}

