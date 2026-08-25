#include "eqpgloba.h"
#include "joglobal.h"
#include <string.h>


char DAEDAEHQ[9];
char DAEDAEA[9];
char DAEDAEB[9];
char DAEDAEC[9];
char TITLENAME[9];
char EQUIPNAME[9];
char SOMONAME[9];
char PLACENAME[13];
char BL_NAME[13];
char CSR_NAME[13];
char EDU_NAME[13];

EquipGlobal::EquipGlobal()
{
	if(strlen(BudaeName))	InitName();
  else InitDefaultName();
}

EquipGlobal::~EquipGlobal()
{}

void EquipGlobal::InitName()
{
	strcpy(DAEDAEHQ,BudaeName);
  strcat(DAEDAEHQ,"-HQ");
  strcpy(DAEDAEA,BudaeName);
  strcat(DAEDAEA,"-A");
  strcpy(DAEDAEB,BudaeName);
  strcat(DAEDAEB,"-B");
  strcpy(DAEDAEC,BudaeName);
  strcat(DAEDAEC,"-C");

  strcpy(TITLENAME,BudaeName);
  strcat(TITLENAME," ””");


  strcpy(EQUIPNAME,BudaeName);
  strcat(EQUIPNAME,"equip");
  strcpy(SOMONAME,BudaeName);
  strcat(SOMONAME,"somo");

  strcpy(PLACENAME,BudaeName);
  strcat(PLACENAME,"place.dat");

  strcpy(BL_NAME,BudaeName);
  strcat(BL_NAME,"_bl.dat");
  strcpy(CSR_NAME,BudaeName);
  strcat(CSR_NAME,"_csr.dat");
	strcpy(EDU_NAME,BudaeName);      
  strcat(EDU_NAME,"_edu.dat");
}

void EquipGlobal::InitDefaultName()
{
	strcpy(DAEDAEHQ,"855-HQ");
  strcpy(DAEDAEA,"855-A");
  strcpy(DAEDAEB,"855-B");
  strcpy(DAEDAEC,"855-C");

  strcpy(TITLENAME,"855 ””");

  strcpy(EQUIPNAME,"855equip");
  strcpy(SOMONAME,"855somo");

  strcpy(PLACENAME,"855place.dat");

  strcpy(BL_NAME,"855_bl.dat");
  strcpy(CSR_NAME,"855_CSR.dat");
	strcpy(EDU_NAME,"855_EDU.dat");
}

