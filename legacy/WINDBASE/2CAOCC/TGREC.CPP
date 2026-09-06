#include "numentry.h"
#include "coordent.h"
#include "dandent.h"
#include "tgrec.h"

TargetRecord::TargetRecord():
	PositionRecord(10,"TargetRecord",1)
{
	EntryBase *ent;
	HuboManager *huboman;

	AddEntry(new EntryBase(0,"Îa¸â¤åÑ¡",5));
	AddEntry(new CoordEntry(1,"¹ÁÎa"));
	huboman=new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("¬a”e»¡Òá­¡          ", 0));
		huboman->Add(-1,new HuboBase("µe”»¡Òá­¡          ", 1));
		huboman->Add(-1,new HuboBase("””»¡Òá­¡          ", 2));
		huboman->Add(-1,new HuboBase("¸åÀa””»³‰i»¡      ", 3));
		huboman->Add(-1,new HuboBase("¸åÀaº—”»³‰i»¡      ", 4));
		huboman->Add(-1,new HuboBase("Í¡¥w””»¥»¡        ", 5));
		huboman->Add(-1,new HuboBase("Í¡¥wº—”»¥»¡        ", 6));
		huboman->Add(-1,new HuboBase("‹¡‰ÑÁ¥¡¥wº—”      ", 7));
		huboman->Add(-1,new HuboBase("¤w¬aÍ¡º—”»¥»¡      ", 8));
		huboman->Add(-1,new HuboBase("14.5M  ‰¡¬aÁ·       ", 9));
		huboman->Add(-1,new HuboBase("37/57M ‰¡¬aÍ¡       ",10));
		huboman->Add(-1,new HuboBase("SA-7 £¡¬a·©         ",11));
		huboman->Add(-1,new HuboBase("·¡•·º—·¥ Í¡¥w””   ",12));
		huboman->Add(-1,new HuboBase("·¡•·º—·¥ ¸åÀa””   ",13));
		huboman->Add(-1,new HuboBase("·¡•·º—·¥ ¸åÀaº—”   ",14));
		huboman->Add(-1,new HuboBase("·¡•·º—·¥ ¸åÀa­¡”   ",15));
		ent=new EntryBase(2,"Îa¸â¬÷»©",20);
		if(ent) ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}
	AddEntry(new NumberEntry(3,"Îa‰¡",-100,10000));
	AddEntry(new EntryBase(4,"§¡‰¡",30));
	AddEntry(new NumberEntry(5,"Ñe¸åÊb",0,100,10,100));
  AddEntry(new DandaehoEntry(6,1));
}

TargetRecord::~TargetRecord()
{}

RecordBase *TargetRecord::Duplicate()
{
	return CreateEqualRecord(new TargetRecord(),TRUE);
}

WORD TargetRecord::GetResID()
{
	NumberEntry *nument=(NumberEntry *)GetEntry(6);
	if(nument) {
		int32 resid;
		if(nument->GetNumber(resid)) return (WORD)resid;
	}
	return -1;
}

