#pragma once
#include "../Common/APTS_Task_Base.h"
#include "SipClientApent.h"

class CSipClientAgentManager
{
public:
	CSipClientAgentManager(void);
	~CSipClientAgentManager(void);
    static CSipClientAgentManager *getinstance();
	void  set_sipproxy(string &id);
	string sipproxyid();
	string sipsmsid();
	void  set_sipsms(string &id);

	int   bind_sipagent(string id,CSipClientApent *agent,int issms = 0);
	int   unbind_sipagnet(string id);
	CSipClientApent * get_sipagent(string id,int issms=0);

	void  get_sipagent(map<string,CSipClientApent*> &sipagentmap);

private:
	static CSipClientAgentManager *sipagent_manager_instance_;
	ACE_Recursive_Thread_Mutex    sipagent_lock_;
	map<string,CSipClientApent*>  sipagent_map_;
	map<string,CSipClientApent*>  sipagent_sms_map_;

	string    sipproxyid_; //本级域编码
	string    sipsmsid_; //本级域编码
};
