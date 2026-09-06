#include <stdlib.h>
#include <math.h>
#include "numentry.h"
#include "realent.h"
#include "dateent.h"
#include "timeent.h"
#include "howizrec.h"

HowitzerRecord::HowitzerRecord():
	RecordBase(5,"HowitzerRecord")
{
	EntryBase *ent;
	HuboManager *huboman;

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("105M ‰e·¥",0));
		huboman->Add(-1,new HuboBase("155M ŠÑw",1));
		huboman->Add(-1,new HuboBase("155M ˆœ·",1));
		huboman->Add(-1,new HuboBase("155M ¸aº",1));
		huboman->Add(-1,new HuboBase("8 \" ‰e·¥ ",2));
		huboman->Add(-1,new HuboBase("8 \" ¸aº ",2));
		huboman->Add(-1,new HuboBase("”ae¸w   ",3));
		huboman->Add(-1,new HuboBase("H.J"      ,4));
		ent=new EntryBase(0,"Š‰w",9);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}

	AddEntry(new NumberEntry(1,"ÂA”¬aˆáŸ¡-¥¡É·"));
	AddEntry(new NumberEntry(2,"ÂA”¬aˆáŸ¡-RAP"));
  AddEntry(new NumberEntry(3,"¸å·¡Ðe‰-¹Á",0,6400,10,0));
  AddEntry(new NumberEntry(4,"¸å·¡Ðe‰-¶",0,6400,10,0));

	AddEntry(new RealEntry(5,"¤i¬a­¢•¡-ÂA”"));
	AddEntry(new RealEntry(6,"¤i¬a­¢•¡-»¡­¢"));

	AddEntry(new NumberEntry(7,"1¤i Òa‰Á-ì·¡"));
	AddEntry(new NumberEntry(8,"1¤i Òa‰Á-¹·¯±"));
}

HowitzerRecord::~HowitzerRecord()
{}

RecordBase *HowitzerRecord::Duplicate()
{
	return CreateEqualRecord(new HowitzerRecord(),TRUE);
}

//===========================================================

BOOL HowitzerRecord::GetDistance(int32& nomal,int32& rap)
{
	NumberEntry *nument1=(NumberEntry *)GetEntry(1);
	NumberEntry *nument2=(NumberEntry *)GetEntry(2);
	if(nument1&&nument2){
		int32 num1,num2;
		if(nument1->GetNumber(num1)&&nument2->GetNumber(num2)){
			nomal=num1;	rap=num2;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL HowitzerRecord::GetLimit(int32& leftlimit,int32& rightlimit)
{
	NumberEntry *nument1=(NumberEntry *)GetEntry(3);
	NumberEntry *nument2=(NumberEntry *)GetEntry(4);
	if(nument1&&nument2){
		int32 num1,num2;
		if(nument1->GetNumber(num1)&&nument2->GetNumber(num2)){
			leftlimit=num1;	rightlimit=num2;
			return TRUE;
		}
	}
	return FALSE;
}

