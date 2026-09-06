#ifndef _ENTCTRL_H
#define _ENTCTRL_H
#include "entbase.h"
#include "control.h"

#define ENTRYCONTROLMESSAGE				(USERMESSAGE)
#define ENTRYCTRL_QUERY	 					(ENTRYCONTROLMESSAGE+1)
#define ENTRYCTRL_RELOAD 					(ENTRYCONTROLMESSAGE+2)
#define ENTRYCTRL_BEGINMODIFY 		(ENTRYCONTROLMESSAGE+3)
#define ENTRYCTRL_ENDMODIFY 			(ENTRYCONTROLMESSAGE+4)
#define ENTRYCTRL_CANCELMODIFY 		(ENTRYCONTROLMESSAGE+5)
#define ENTRYCTRL_INVALIDMODIFY 	(ENTRYCONTROLMESSAGE+6)

class EntryControl:public JKControl
{
public:
	EntryControl(JKRect& rect,WORD ctlid,EntryBase& ent);
	virtual ~EntryControl(){}

	ProcessResult RespondMessage(JKMSG& msg);

	virtual void ModifyEntry()=0;
	virtual void ReLoad()=0;

	EntryBase& RefEntry;
};
#endif