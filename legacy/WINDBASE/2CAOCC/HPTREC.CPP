#include "boolent.h"
#include "numentry.h"
#include "timeent.h"
#include "tgrefent.h"
#include "hptrec.h"

HPTRecord::HPTRecord():
	RecordBase(7,"HPTRecord")
{
	AddEntry(new TargetRefEntry(0,""));
	AddEntry(new TimeEntry(1,"ÂA‹eÎa¸âˆ—¯¥¯¡ˆe"));
	AddEntry(new BooleanEntry(2,"‰¡ˆaÃ¡ µa¦",FALSE));
}

HPTRecord::~HPTRecord()
{}

RecordBase *HPTRecord::Duplicate()
{
	return CreateEqualRecord(new HPTRecord(),TRUE);
}

BOOL HPTRecord::GetTargetRecord(TargetRecord& tgrec)
{
	TargetRefEntry& ent = (TargetRefEntry&)*GetEntry((uint16)0);
	return ent.GetTargetRecord(tgrec);
}
