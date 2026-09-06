#include "numentry.h"
#include "tgrefent.h"
#include "plntgrec.h"

PlanTargetRecord::PlanTargetRecord():
	RecordBase(1,"PlanTargetRecord")
{
	EntryBase *ent;
	HuboManager *huboman;

	AddEntry(new TargetRefEntry(0,""));
	huboman=new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("‰·Ìa¬a          ",0));
    huboman->Add(-1,new HuboBase("Îa¸âŠ…          ",1));
    huboman->Add(-1,new HuboBase("Îa¸â”          ",2));
    huboman->Add(-1,new HuboBase("ÑÁb‰b¡i»¡µb    ",3));
    huboman->Add(-1,new HuboBase("ÑÁbÈáé        ",4));
		huboman->Add(-1,new HuboBase("‰·º…¬a          ",5));
		ent=new EntryBase(1,"‰·‰b¤w¤ó",16);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}

	AddEntry(new EntryBase(2,"¬a‰b¦”¡w00",5));
	AddEntry(new EntryBase(3,"Èe¹·00",10));
	AddEntry(new NumberEntry(4,"¤i®00",0,1000));
	AddEntry(new NumberEntry(5,"¬a‰b¯¡¸b¯¡ˆb00",-200,200,10,0));
	AddEntry(new NumberEntry(6,"¬a‰b¹·ža¯¡ˆb00",-200,200,10,0));

	AddEntry(new EntryBase(7,"¬a‰b¦”¡w01",5));
	AddEntry(new EntryBase(8,"Èe¹·01",10));
	AddEntry(new NumberEntry(9,"¤i®01",0,1000));
	AddEntry(new NumberEntry(10,"¬a‰b¯¡¸b¯¡ˆb01",-200,200,10,0));
	AddEntry(new NumberEntry(11,"¬a‰b¹·ža¯¡ˆb01",-200,200,10,0));

	AddEntry(new EntryBase(12,"¬a‰b¦”¡w02",5));
	AddEntry(new EntryBase(13,"Èe¹·02",10));
	AddEntry(new NumberEntry(14,"¤i®02",0,1000));
	AddEntry(new NumberEntry(15,"¬a‰b¯¡¸b¯¡ˆb02",-200,200,10,0));
	AddEntry(new NumberEntry(16,"¬a‰b¹·ža¯¡ˆb02",-200,200,10,0));

	AddEntry(new EntryBase(17,"¬a‰b¦”¡w03",5));
	AddEntry(new EntryBase(18,"Èe¹·03",10));
	AddEntry(new NumberEntry(19,"¤i®03",0,1000));
	AddEntry(new NumberEntry(20,"¬a‰b¯¡¸b¯¡ˆb03",-200,200,10,0));
	AddEntry(new NumberEntry(21,"¬a‰b¹·ža¯¡ˆb03",-200,200,10,0));

	AddEntry(new EntryBase(22,"¬a‰b¦”¡w04",5));
	AddEntry(new EntryBase(23,"Èe¹·04",10));
	AddEntry(new NumberEntry(24,"¤i®04",0,1000));
	AddEntry(new NumberEntry(25,"¬a‰b¯¡¸b¯¡ˆb04",-200,200,10,0));
	AddEntry(new NumberEntry(26,"¬a‰b¹·ža¯¡ˆb04",-200,200,10,0));
}

PlanTargetRecord::~PlanTargetRecord()
{}

RecordBase *PlanTargetRecord::Duplicate()
{
	return CreateEqualRecord(new PlanTargetRecord(),TRUE);
}

//===================================================

BOOL PlanTargetRecord::GetBalsu(uint16 i,int32& balsu)
{
	if(i<5){
		WORD balsuidarray[5]={4,9,14,19,24};
		NumberEntry *nument=(NumberEntry *)GetEntry(balsuidarray[i]);
		if(nument) return nument->GetNumber(balsu);
	}
	return FALSE;
}

BOOL PlanTargetRecord::GetStartH(uint16 i,int32& htime)
{
	if(i<5){
		WORD startHarray[5]={5,10,15,20,25};
		NumberEntry *nument=(NumberEntry *)GetEntry(startHarray[i]);
		if(nument) return nument->GetNumber(htime);
	}
	return FALSE;
}

BOOL PlanTargetRecord::GetEndH(uint16 i,int32& htime)
{
	if(i<5){
		WORD endHarray[5]={6,11,16,21,26};
		NumberEntry *nument=(NumberEntry *)GetEntry(endHarray[i]);
		if(nument) return nument->GetNumber(htime);
	}
	return FALSE;
}

BOOL PlanTargetRecord::GetIntervalH(uint16 i,uint32& interval)
{
	int32 starth,endh;
	if(GetStartH(i,starth) && GetEndH(i,endh)){
		if(starth<endh){
			interval = endh-starth;
			return TRUE;
		}
	}
	return FALSE;
}

int16 PlanTargetRecord::GetType()
{
	EntryBase *ent = GetEntry(1);
	if(ent){
		int16 index=ent->GetHuboIndex();
		if(index>=0){
			HuboBase *base=ent->HuboMan->GetHuboBase(index);
			if(base) return base->Number;
		}
	}
	return -1;
}

BOOL PlanTargetRecord::GetTargetRecord(TargetRecord& tgrec)
{
	TargetRefEntry& ent = (TargetRefEntry&)*GetEntry((uint16)0);
	return ent.GetTargetRecord(tgrec);
}
