#ifndef _RECVIEW_H
#define _RECVIEW_H

#include "recbase.h"
#include "control.h"

class RecordViewBase
{
public:
	RecordViewBase(JKControl& refctrl,RecordBase& recbase);
  virtual ~RecordViewBase();
	void EvCharDown(uint16 key,WORD );
	void RV_RespondMessage(JKMSG& msg);
	virtual void EvEntryCtrlQuery(WORD ctrlid);
	virtual void EvEntryCtrlEndModify(WORD ctrlid);
	virtual void EvEntryCtrlCancelModify(WORD ctrlid);
	virtual void EvEntryCtrlInvalidModify(WORD ctrlid);

	BOOL ModifyEntry(WORD ctrlid);
	void ReLoad();

	RecordBase& RefRecord;
	JKControl& RefControl;

	BOOL IsChange;
};

#endif