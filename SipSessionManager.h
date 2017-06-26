#pragma once
#include "../Common/APTS_Task_Base.h"
#include "SipSession.h"
#include "SipClientAgentManager.h"
#include "SipDefine.h"

class CSipSessionManager
{
public:
	CSipSessionManager(void);
	~CSipSessionManager(void);

	static CSipSessionManager *getinstance();

	CSipSession *  get_session(string id,int cid);
	int            insert_session(CSipSession * session);
		
	int  handle_event(eXosip_t *context_eXosip,eXosip_event_t *evt);    
private:
	static CSipSessionManager* s_instance_;

	ACE_Recursive_Thread_Mutex    session_lock_;
	vector<CSipSession *>      v_session_;
};
