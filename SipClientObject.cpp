#include "SipClientObject.h"
CSipClientObject::~CSipClientObject(void)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(callinvite_lock_);
	map<int,CallInviteInfo*>::iterator iter = callinvite_map.begin();
	for (;iter!=callinvite_map.end();++iter)
	{
		delete iter->second;
        iter->second = NULL;
		callinvite_map.erase(iter);
	}
}
void CSipClientObject::insertcall(int cid,CallInviteInfo* cinfo)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(callinvite_lock_);
	map<int,CallInviteInfo*>::iterator iter = callinvite_map.find(cid);
	if(iter!=callinvite_map.end())
	{
	  APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] insertcall error cid:%d,from:%s,to:%s\n",cid,cinfo->from.c_str(),cinfo->to.c_str()));
      delete cinfo;
	  cinfo = NULL;
	  return;
	}
    callinvite_map.insert(pair<int,CallInviteInfo*>(cid,cinfo));
    APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] insertcall cid:%d,from:%s,to:%s\n",cid,cinfo->from.c_str(),cinfo->to.c_str()));
}

CallInviteInfo* CSipClientObject::getcallinfobycid(int cid)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(callinvite_lock_);
	map<int,CallInviteInfo*>::iterator iter = callinvite_map.find(cid);
	if(iter!=callinvite_map.end())
	{
		return iter->second;
	}
	return NULL;
}