#include "dataman.h"

class TestRecord :public RecordBase
{
public:
	TestRecord():
		RecordBase(0,"")
	{}
	RecordBase *Duplicate()
	{
		return CreateEqualRecord(new TestRecord(),TRUE);
	}
};

void main()
{
	TestRecord *AddedRecord = new TestRecord();
	if(AddedRecord){
		DataManager dataman;
		if(!dataman.ChangeFileName("testrec",*AddedRecord))
			dataman.CreationError=TRUE;
		delete AddedRecord;
	}
}