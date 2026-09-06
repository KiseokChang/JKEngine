#include "numentry.h"
#include "coordent.h"
#include "dandent.h"
#include "battrec.h"

BattalionRecord::BattalionRecord():
	PositionRecord(3,"BattalionRecord",3)
{
	EntryBase *ent;
	HuboManager *huboman;

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("2Í¡µa”e"));
		huboman->Add(-1,new HuboBase("27 ¬a”e"));
		huboman->Add(-1,new HuboBase("76 ¬a”e"));
		huboman->Add(-1,new HuboBase(" 7 ¬a”e"));
		huboman->Add(-1,new HuboBase("15 ¬a”e"));
		ent=new EntryBase(0,"¬w‹s¦”¡w",7);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}
	AddEntry(new EntryBase(1,"¦”¡wÃ·",5));
	AddEntry(new NumberEntry(2,"¤wµi¤w¶áˆb",0,6400));
	AddEntry(new CoordEntry(3,"¹ÁÎa"));

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
		ent=new EntryBase(4,"Š‰w",9);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}
	AddEntry(new NumberEntry(5,"Í¡¢…®",0,300));

  AddEntry(new NumberEntry(6,"Ñe¸¥wb",0,1500));
  AddEntry(new NumberEntry(7,"·¥ˆa¥wb",0,1500));
  AddEntry(new NumberEntry(8,"Ñe¸Èe´¢",0,50000));
  AddEntry(new NumberEntry(9,"·¥ˆaÈe´¢",0,50000));

	huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("¬a‰b”‹¡",1));
		huboman->Add(-1,new HuboBase("¬a‰bº—  ",2));
		huboman->Add(-1,new HuboBase("·¡•·º—  ",3));
		ent=new EntryBase(10,"¦”¬wÑ×",8);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}

	AddEntry(new DandaehoEntry(11,FALSE));
}

BattalionRecord::~BattalionRecord()
{}


RecordBase *BattalionRecord::Duplicate()
{
	return CreateEqualRecord(new BattalionRecord(),TRUE);
}


//========================================================

BOOL BattalionRecord::GetAZ(int32& az)
{
	NumberEntry *nument = (NumberEntry *)GetEntry(2);
	if(nument) return nument->GetNumber(az);
	return FALSE;
}

BOOL BattalionRecord::SetAZ(int32 az)
{
	NumberEntry *nument = (NumberEntry *)GetEntry(2);
	if(nument) return nument->SetNumber(az);
	return FALSE;
}

uint16 BattalionRecord::GetStatus()
{
	EntryBase *ent = GetEntry(10);
	if(ent&&ent->HuboMan){
		int16 index=ent->GetHuboIndex();
		if(index>=0){
			HuboBase *base=ent->HuboMan->GetHuboBase(index);
			if(base) return (uint16)base->Number;
		}
	}
	return 0;
}

void BattalionRecord::SetStatus(uint16 status)
{
	EntryBase *ent = GetEntry(10);
	if(ent&&ent->HuboMan){
		int16 index=ent->HuboMan->GetIndexByNumber(status);
		if(index>=0){
			HuboBase *base=ent->HuboMan->GetHuboBase(index);
			if(base) ent->SetData(base->TextBuff);
		}
	}
}

BOOL BattalionRecord::GetBalsu(int32& balsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(8);
	if(nument) return nument->GetNumber(balsu);
	return FALSE;
}

BOOL BattalionRecord::SetBalsu(int32 balsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(8);
	if(nument) return nument->SetNumber(balsu);
	return FALSE;
}

WORD BattalionRecord::GetResID()
{
	NumberEntry *nument=(NumberEntry *)GetEntry(11);
	if(nument) {
		int32 resid;
		if(nument->GetNumber(resid)) return (WORD)resid;
	}
	return -1;
}

BOOL BattalionRecord::GetMunsu(int32& munsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(5);
	if(nument) return nument->GetNumber(munsu);
	return FALSE;
}

BOOL BattalionRecord::SetMunsu(int32 munsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(5);
	if(nument) return nument->SetNumber(munsu);
	return FALSE;
}


