#include "sollucvt.h"

SolLuCvt::SolLuCvt():
	CreationError(FALSE),LeapYear(0)
{
	getdate(&Solar);
	SolToLuna(Solar,Lunar);
}

SolLuCvt::SolLuCvt(date Date,int16 Sig):
	CreationError(FALSE),LeapYear(0)
{
	if(Sig==LUNAR) {
		Lunar.da_year=Date.da_year;
		Lunar.da_mon =Date.da_mon;
		Lunar.da_day =Date.da_day;
		LunaToSol(Lunar,Solar);
	}
	else {
		Solar.da_year=Date.da_year;
		Solar.da_mon =Date.da_mon;
		Solar.da_day =Date.da_day;
		SolToLuna(Solar,Lunar);
	}
}

BOOL SolLuCvt::LunaToSol(date S1,date& S2)
{
	int16 SW,SS;
	uint16 Sy,Sm,Sd,Ly,Lm,Ld;
	int32 T=0,J,K;

	Ly=S1.da_year-1900;
	Lm=S1.da_mon;
	Ld=S1.da_day;

	if(!((Ly>0)&&(Ly<141)&&(Lm>0)&&(Lm<13)&&(Ld>0)&&(Ld<30))) {
		CreationError=TRUE;
		return FALSE;
	}
	T=Ld+49;
	SS=Ly+1900;
	uint16 i;
	for (i=0;i<Ly-1;i++) T=T+Em[i][12];
	for (i=0;i<Lm-1;i++) T=T+Vm[Em[Ly-1][i]];

	J=T/1461;
	T=T-1461*J;
	K=T/365;
	T=T-365*K;
	Sy=(uint16)(4*J+K+1);
	if (((Sy==4*(Sy/4))&&(T>59))) SW=1;
	else SW=0;
	Sm=0;
	while (T>Mm[Sm]+SW) {
		Sd=(int16)T-Mm[Sm]-SW;
		Sm++;
	}
	if ((Sy==4*(Sy/4))&&(T==60)) Sd=29;

	if (Sd==0) {
		Sm--;
		if(Sm<1)	{
      Sm = 12;
		  Sy = Sy - 1;
		}
    Sd = Vs[Sm-1];
    if (Sm==2) {
		  if (((SS%4)==0)&&((SS%100)!=0)&&((SS%400)==0)) Sd=29;
			else Sd=28;
		}
  }

  S2.da_year=Sy+1900;
  S2.da_mon=Sm;
  S2.da_day=Sd;
  return TRUE;
}


BOOL SolLuCvt::SolToLuna(date S1,date& S2)
{
  uint16 Sy,Sm,Sd,Ly,Lm,Ld;
  int32 T;

  Sy=S1.da_year-1900;
	Sm=(uint16)S1.da_mon;
	Sd=(uint16)S1.da_day;

  if(!((Sy>0)&&(Sy<141)&&(Sm>0)&&(Sm<13)&&(Sd>0)&&(Sd<Vs[Sm-1]+1))) {
  	CreationError=TRUE;
    return FALSE;
  }

  T=365*(Sy-1)+Mm[Sm-1]+Sd+(Sy/4)-49;
	if((Sy==4*(Sy/4))&&(30*Sm+Sd<90)) T--;

	Ly=1;
  while (T>Em[Ly-1][12]) {
    T=T-Em[Ly-1][12];
    Ly++;
  }

  Lm=1;
  while (T>Vm[(Em[Ly-1][Lm-1])]) {
    T=T-Vm[(Em[Ly-1][Lm-1])];
    Lm++;
 	}

  Ld=(uint16)T;

  if(((Em[Ly-1][Lm-1]==3)||(Em[Ly-1][Lm-1]==4))&&(T>29)) {
    Ld=(uint16)T-29;
    LeapYear=TRUE;
  }
  if(((Em[Ly-1][Lm-1]==5)||(Em[Ly-1][Lm-1]==6))&&(T>30)) {
    Ld=(uint16)T-30;
    LeapYear=TRUE;
  }
  S2.da_year= Ly+1900;
  S2.da_mon = Lm;
  S2.da_day = Ld;
  return TRUE;
}

