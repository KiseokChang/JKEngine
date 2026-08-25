#include "resman.h"
#include "jkapp.h"


WORD InternPaletteID[InternPaletteCount]={
	0x0001
};

WORD InternImageID[InternImageCount]={
	0x0001,0x1001,0x2001,
	0x0002,0x1002,0x2002,
	0x0003,0x1003,0x2003,
	0x0004,0x1004,0x2004,
	0x0005,0x1005,
	0x0006,0x0007,0x0008,0x0009,
	0x000a,0x100a,
	0x000b,0x100b,
	0x000c,0x100c,
	0x000d,0x100d
};


JKApplication::JKApplication():
	JKControl(JKRect(0,0,getmaxx(),getmaxy()),0),EventHandler(this),
	IsInitApp(FALSE)
{
	BackColor=Black;
}

JKApplication::~JKApplication()
{}

void JKApplication::InitApplication()
{
	if(!IsInitApp){
		InitResourceMan();

		ResourceBase *res;
		BYTE *data;
		res=ResMan->GetResource(InternImageID[JKPalette],RES_PALETTE);
		if(res){
			data = res->AllocRealData();
			if(data) {
				::GetAllPalette(OldPalette);
				::SetAllPalette(data);
				res->FreeRealData(data);
			}
		}
		IsInitApp=TRUE;
	}
}

void JKApplication::CloseApplication()
{
	if(IsInitApp){
		::SetAllPalette(OldPalette);
		IsInitApp=FALSE;
	}
}


void JKApplication::InitResourceMan()
{
	if(ResMan)	{
		WORD i;
		for(i=0;i<InternPaletteCount;i++)
			ResMan->AddResource(new ResourceBase(InternPaletteID[i],WORD(RES_PALETTE),(BYTE *)"jkpalett.dat",BYTE(RT_FILE|RT_PACKFILE),i));
		for(i=0;i<InternImageCount;i++)
			ResMan->AddResource(new ResourceBase(InternImageID[i],WORD(RES_IMAGE),(BYTE *)"jkimage.dat",BYTE(RT_FILE|RT_PACKFILE),i));
	}
}



WORD JKApplication::Run()
{
	InitApplication();

	if(EventHandler::CreationError||JKControl::CreationError) return 0;

	MsgFilter->InitFilter();
	MsgQue->ClearNode();
	if(!IsInit) InitWindow();
	EventHandler::Run();
	CloseApplication();
	return 1;
}

void JKApplication::DestroySubWindow(DWORD winid)
{
	JKControl::DestroySubWindow(winid);
	if(!NodeCount) CloseWindow();
}