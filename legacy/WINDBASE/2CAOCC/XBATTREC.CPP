#include "numentry.h"
#include "coordent.h"
#include "dandent.h"
#include "xbattrec.h"

XBattalionRecord::XBattalionRecord():
    PositionRecord(8,"XBattalionRecord",2)
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
  AddEntry(new CoordEntry(2,"¹ÁÎa"));

  AddEntry(new EntryBase(3,"§¡‰¡",30));
  AddEntry(new NumberEntry(4,"¸åÊb",0,100));

  AddEntry(new DandaehoEntry(5,0));
}

XBattalionRecord::~XBattalionRecord()
{}


RecordBase *XBattalionRecord::Duplicate()
{
	return CreateEqualRecord(new XBattalionRecord(),TRUE);
}


WORD XBattalionRecord::GetResID()
{
  NumberEntry *nument=(NumberEntry *)GetEntry(5);
	if(nument) {
		int32 resid;
		if(nument->GetNumber(resid)) return (WORD)resid;
	}
	return -1;
}

