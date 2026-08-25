#include "occwin.h"
#include "resman.h"
#include "occapp.h"

OCCApplication::OCCApplication():
	JKApplication()
{
//	BackColor=LtBlue;
}

OCCApplication::~OCCApplication()
{}

void OCCApplication::SetupWindow()
{
	AddControl(new OCCWindow());
}

void OCCApplication::InitResourceMan()
{
	JKApplication::InitResourceMan();
	if(ResMan)	{
		for(uint16 i=0;i<3;i++){
			ResMan->AddResource(new ResourceBase(0x400+i,WORD(RES_IMAGE),(BYTE *)"funcres.res",BYTE(RT_FILE|RT_PACKFILE),i*2));
			ResMan->AddResource(new ResourceBase(0x1000+0x400+i,WORD(RES_IMAGE),(BYTE *)"funcres.res",BYTE(RT_FILE|RT_PACKFILE),i*2+1));
		}
		for(i=0;i<50;i++)
			ResMan->AddResource(new ResourceBase(0x4000+i,WORD(RES_IMAGE),TRUE,"iconres.res",i));
		for(i=0;i<30;i++)
			ResMan->AddResource(new ResourceBase(0x4000+0x100+i,WORD(RES_IMAGE),(BYTE *)"moonres.res",BYTE(RT_FILE|RT_PACKFILE),i));
		for(i=0;i<61;i++)
			ResMan->AddResource(new ResourceBase(0x4000+0x200+i,WORD(RES_IMAGE),TRUE,"dandaeho.res",i));
		for(i=0;i<80;i++)
			ResMan->AddResource(new ResourceBase(0x4000+0x300+i,WORD(RES_IMAGE),TRUE,"edandae.res",i));
	}
}
