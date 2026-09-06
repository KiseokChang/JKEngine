#include <stdlib.h>
#include "numentry.h"
#include "coordent.h"
#include "timeent.h"
#include "dateent.h"
#include "firerec.h"

FireRecord::FireRecord():
	PositionRecord(3,"FireRecord",2)
{
	EntryBase *ent;
	HuboManager *huboman;
	AddEntry(new EntryBase(0,"¦”¡wÃ·",5));
	AddEntry(new EntryBase(1,"Îa¸â¤åÑ¡",5));
  AddEntry(new CoordEntry(2,"¹ÁÎa"));

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
		ent=new EntryBase(3,"Îa¸â¬÷»©",20);
		ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
  }
	huboman=new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("ÌaŠA  "));
		huboman->Add(-1,new HuboBase("¢bÑÁ"));
		huboman->Add(-1,new HuboBase("¹A´s  "));
		ent=new EntryBase(4,"¶a wÒa‰Á",6);
		ent->SetHuboManager(huboman,TRUE);
		AddEntry(ent);
	}
	AddEntry(new EntryBase(5,"Èe¹·",10));
	AddEntry(new NumberEntry(6,"¤i®",1,1000));
	AddEntry(new EntryBase(7,"¯¥‰Å",10));
	AddEntry(new DateEntry(8,"¬a‰b¡ww·©¸a"));
	AddEntry(new TimeEntry(9,"¬a‰b¡ww¯¡ˆe"));
}

FireRecord::~FireRecord()
{}

RecordBase *FireRecord::Duplicate()
{
	return CreateEqualRecord(new FireRecord(),TRUE);
}

BOOL FireRecord::GetBalsu(int32& balsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(6);
	if(nument) return nument->GetNumber(balsu);
  return FALSE;
}

BOOL FireRecord::SetBalsu(int32 balsu)
{
	NumberEntry *nument=(NumberEntry *)GetEntry(6);
	if(nument) return nument->SetNumber(balsu);
	return FALSE;
}
