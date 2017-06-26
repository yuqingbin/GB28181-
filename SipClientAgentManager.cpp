#include "SipClientAgentManager.h"

CSipClientAgentManager *CSipClientAgentManager::sipagent_manager_instance_ = NULL;
CSipClientAgentManager::CSipClientAgentManager(void)
{
}

CSipClientAgentManager::~CSipClientAgentManager(void)
{
}

CSipClientAgentManager* CSipClientAgentManager::getinstance()
{
   if(sipagent_manager_instance_ == NULL)
   {
	   sipagent_manager_instance_ = new CSipClientAgentManager();
   }

   return sipagent_manager_instance_;
}

void  CSipClientAgentManager::set_sipproxy(string &id)
{
	sipproxyid_ = id;
}

void  CSipClientAgentManager::set_sipsms(string &id)
{
	sipsmsid_ = id;
}

string CSipClientAgentManager::sipproxyid()
{
  return sipproxyid_;
}
string CSipClientAgentManager::sipsmsid()
{
  return sipsmsid_;
}

int   CSipClientAgentManager::bind_sipagent(string id,CSipClientApent *agent,int issms)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipagent_lock_);

	if(issms)
	{
		map<string,CSipClientApent*>::iterator iter = sipagent_sms_map_.find(id);
		if( iter != sipagent_sms_map_.end() )
		{
			APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] Á÷Ã½Ìå×¢²áusrname³åÍ»:%s\n",id.c_str()));
			delete agent;
			agent = NULL;
			return -1;
		}
		sipagent_sms_map_.insert(pair<string,CSipClientApent*>(id,agent));

		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] Á÷Ã½Ìå×¢²áfrom:%s@%s:%d,%s\n",agent->id().c_str(),agent->ip().c_str(),agent->port(),agent->name().c_str()));

	}
	else
	{
		map<string,CSipClientApent*>::iterator iter = sipagent_map_.find(id);
		if( iter != sipagent_map_.end() )
		{
			APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] ×¢²áusrname³åÍ»:%s\n",id.c_str()));
			delete agent;
			agent = NULL;
			return -1;
		}
		sipagent_map_.insert(pair<string,CSipClientApent*>(id,agent));
		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] ×¢²áfrom:%s@%s:%d,%s\n",agent->id().c_str(),agent->ip().c_str(),agent->port(),agent->name().c_str()));

	}
	
   return 0;
}

int  CSipClientAgentManager::unbind_sipagnet(string id)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipagent_lock_);
	map<string,CSipClientApent*>::iterator iter = sipagent_map_.find(id);
	if( iter != sipagent_map_.end() )
	{
		delete iter->second;
		iter->second = NULL;
		sipagent_map_.erase(iter);
	}
	else
	{
		//return -1;
		map<string,CSipClientApent*>::iterator itersms = sipagent_sms_map_.find(id);
		if(itersms!=sipagent_sms_map_.end())
		{
			delete itersms->second;
			itersms->second = NULL;
			sipagent_sms_map_.erase(itersms);
		}
	}

	return 0;
}
CSipClientApent * CSipClientAgentManager::get_sipagent(string id,int issms)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipagent_lock_);
	map<string,CSipClientApent*>::iterator iter = sipagent_sms_map_.find(id);
	if( iter != sipagent_sms_map_.end() )
	{
		return iter->second;
	}
	else
	{
		map<string,CSipClientApent*>::iterator iter2 = sipagent_map_.find(id);
		if( iter2 != sipagent_map_.end() )
		{
			return iter2->second;
		}
	}
	
	return NULL;
}

void  CSipClientAgentManager::get_sipagent(map<string,CSipClientApent*> &sipagentmap)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipagent_lock_);
   sipagentmap.insert(sipagent_map_.begin(),sipagent_map_.end());
}