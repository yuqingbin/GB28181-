#include "SipClientApent.h"

CSipClientApent::CSipClientApent(void)
:agent_type_(0)
,register_state_(REGISTER_STATUE::SIP_REGISTERED)
,keeplive_time_(0)
,cid_(0)
,did_(0)
,tid_(0)
{
}

CSipClientApent::~CSipClientApent(void)
{
}

CSipClientApent::CSipClientApent(char *ip,long port,char *id,char *name,time_t kt,int agenttype,int registerstate)
{
	if(ip)
		this->ip_ = ip;
	if(id)
		this->id_ = id;
	if(name)
		this->name_ = name;
	this->port_ = port;
	this->keeplive_time_ = kt;
	this->agent_type_ = agenttype;
	this->register_state_ = registerstate;
}

void   CSipClientApent::update_callinfo(int cid,CSipCallInfo *sci)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
  map<int,CSipCallInfo*>::iterator iter = sip_call_map_.find(cid);
  if(iter!=sip_call_map_.end())
  {
	  if(iter->second)
	  {
		  delete iter->second;
		  iter->second = NULL;
		  sip_call_map_.erase(iter);
	  }
  }
  
  sip_call_map_.insert(pair<int,CSipCallInfo*>(cid,sci));
}

void   CSipClientApent::remove_callinfo(int cid)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
  map<int,CSipCallInfo*>::iterator iter = sip_call_map_.find(cid);
  if(iter!=sip_call_map_.end())
  {
	  if(iter->second)
	  {
		  delete iter->second;
		  iter->second = NULL;
		  sip_call_map_.erase(iter);
	  }
  }
}

CSipCallInfo * CSipClientApent::get_callinfo(int cid)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
  map<int,CSipCallInfo*>::iterator iter = sip_call_map_.find(cid);
  if(iter!=sip_call_map_.end())
  {
	 return iter->second;
  }

  return NULL;
}

void  CSipClientApent::get_callinfo(int srccid,vector<CSipCallInfo*> &vt)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
     map<int,CSipCallInfo*>::iterator iter = sip_call_map_.begin();
	 for (;iter!=sip_call_map_.end();++iter)
	 {
        if(iter->second&&iter->second->srccid_ == srccid)
		{
			vt.push_back(iter->second);
		}
	 }
}

int   CSipClientApent::check_callinfo(int cid)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
	map<int,CSipCallInfo*>::iterator iter = sip_call_map_.find(cid);
	if(iter!=sip_call_map_.end())
	{
		return 1;
	}

	return 0;
}

int    CSipClientApent::send_invite(eXosip_t *context_eXosip,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid)
{
    if(sipproxy == NULL || srccall == NULL)
		return -1;

	int ret = 0;
	osip_message_t *invitesip = NULL;
	char from[100];/*sip:主叫用户名@被叫IP地址*/
	char to[100];/*sip:被叫IP地址:被叫IP端口*/

	memset(from, 0, 100);
	memset(to, 0, 100);
	sprintf(from, "sip:%s@%s:%d", sipproxy->id().c_str()/*fromid.c_str()*/, sipproxy->ip().c_str(),sipproxy->port());
	sprintf(to, "sip:%s@%s:%d",this->id().c_str()/*toid.c_str()*/,this->ip().c_str(), this->port());

	ret = eXosip_call_build_initial_invite(context_eXosip,&invitesip,to,from,NULL,"");
	did_ = ret;
	if(ret < 0 )
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite eXosip_call_build_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}

	ret = eXosip_call_send_initial_invite(context_eXosip,invitesip);
	if(ret < 0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite eXosip_call_send_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}

	cid_ = ret;
	CSipCallInfo *callinfo = new CSipCallInfo(ret,cid_,did_,srccall->did_,srccall->cid_,srccall->srcid_.c_str(),srccall->dstid_.c_str(),SIPREQUST_STATUS::STATUS_SEND_INVITE_NOSDP);
	callinfo->set_ssrc(srccall->ssrc_);
	update_callinfo(ret,callinfo);
	

	APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] send_invite from:%s,to:%s\n",from,to));
	return ret;
}

int    CSipClientApent::send_invite_sdp(eXosip_t *context_eXosip,eXosip_event_t *evt,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid)
{
	if(sipproxy == NULL || srccall == NULL)
		return -1;

	int ret = 0;
	osip_message_t *invitesip = NULL;
	char from[100];/*sip:主叫用户名@被叫IP地址*/
	char to[100];/*sip:被叫IP地址:被叫IP端口*/

	memset(from, 0, 100);
	memset(to, 0, 100);
	sprintf(from, "sip:%s@%s:%d", sipproxy->id().c_str()/*fromid.c_str()*/, sipproxy->ip().c_str(),sipproxy->port());
	sprintf(to, "sip:%s@%s:%d",this->id().c_str()/*toid.c_str()*/,this->ip().c_str(), this->port());


	ret = eXosip_call_build_initial_invite(context_eXosip,&invitesip,to,from,NULL,"");
	if(ret < 0 )
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite_sdp eXosip_call_build_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}
	did_ = ret;
    
	sdp_message_t *remotesdp= NULL;
	remotesdp = eXosip_get_remote_sdp(context_eXosip,evt->did);
	string  sdpcontent;
	if(remotesdp)
	{
		char* csdp = NULL;
		sdp_message_to_str(remotesdp, &csdp);
		if(csdp)
		{
			sdpcontent = csdp;
			osip_free(csdp);
		}

		if(sdpcontent.find("y=") == std::string::npos)
		{
			sdpcontent.append("y=");
			sdpcontent.append(srccall->ssrc_.c_str());
			sdpcontent.append("\r\n");
		}
	}

	osip_message_set_body(invitesip,sdpcontent.c_str(),sdpcontent.length());

	osip_message_set_content_type(invitesip,"application/sdp");

	ret = eXosip_call_send_initial_invite(context_eXosip,invitesip);
	if(ret < 0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite_sdp eXosip_call_send_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}

	cid_ = ret;
	CSipCallInfo *callinfo = new CSipCallInfo(ret,cid_,did_,srccall->did_,srccall->cid_,srccall->srcid_.c_str(),srccall->dstid_.c_str(),SIPREQUST_STATUS::STATUS_SEND_INVITE_SDP);

	callinfo->set_ssrc(srccall->ssrc_);
    callinfo->set_sdp(sdpcontent);
	update_callinfo(ret,callinfo);

	APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] send_invite_sdp from:%s,to:%s\n",from,to));
	return ret;
}

int    CSipClientApent::send_invite_sdp(eXosip_t *context_eXosip,string sdpcontent,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid)
{
	if(sipproxy == NULL || srccall == NULL)
		return -1;

	int ret = 0;
	osip_message_t *invitesip = NULL;
	char from[100];/*sip:主叫用户名@被叫IP地址*/
	char to[100];/*sip:被叫IP地址:被叫IP端口*/

	memset(from, 0, 100);
	memset(to, 0, 100);
	sprintf(from, "sip:%s@%s:%d", sipproxy->id().c_str()/*fromid.c_str()*/, sipproxy->ip().c_str(),sipproxy->port());
	sprintf(to, "sip:%s@%s:%d",this->id().c_str()/*toid.c_str()*/,this->ip().c_str(), this->port());


	ret = eXosip_call_build_initial_invite(context_eXosip,&invitesip,to,from,NULL,"");
	if(ret < 0 )
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite_sdp 2 eXosip_call_build_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}

	osip_message_set_body(invitesip,sdpcontent.c_str(),sdpcontent.length());

	osip_message_set_content_type(invitesip,"application/sdp");

	ret = eXosip_call_send_initial_invite(context_eXosip,invitesip);
	if(ret < 0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] send_invite_sdp 2 eXosip_call_send_initial_invite fail,from:%s,to:%s\n",from,to));
		return -1;
	}

	CSipCallInfo *callinfo = new CSipCallInfo(ret,ret,ret,srccall->did_,srccall->cid_,srccall->srcid_.c_str(),srccall->dstid_.c_str(),SIPREQUST_STATUS::STATUS_SEND_INVITE_SDP);

	callinfo->set_ssrc(srccall->ssrc_);
	callinfo->set_sdp(sdpcontent);
	update_callinfo(ret,callinfo);

	APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] send_invite_sdp 2 from:%s,to:%s\n",from,to));
	return ret;
}

int    CSipClientApent::send_ack(eXosip_t *context_eXosip,string sdpcontent,int did,int ishavesdp)
{
	osip_message_t *ack = NULL;
	int ret = eXosip_call_build_ack(context_eXosip,did,&ack);
    if(ishavesdp)
	{
		osip_message_set_body(ack,sdpcontent.c_str(),sdpcontent.length());
		osip_message_set_content_type(ack,"application/sdp");
	}
	ret = eXosip_call_send_ack(context_eXosip,did,ack);
	if(ret <0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] 1 eXosip_call_send_ack fail did:%d\n",did));
		return -1;
	}
	return 0;
}

int CSipClientApent::send_200OK(eXosip_t *context_eXosip,int tid,long code,string sdpcontent,CSipCallInfo *srccall)
{
	osip_message_t *answer = NULL; 
	int ret = eXosip_call_build_answer (context_eXosip, tid, code, &answer);
    if(ret < 0)
		return -1;

	osip_message_set_body (answer, sdpcontent.c_str(), sdpcontent.length());
	osip_message_set_content_type (answer, "application/sdp");

	ret = eXosip_call_send_answer(context_eXosip, tid, code, answer);
	if(ret <0)
	{
		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] eXosip_call_send_answer error"));
		return -1;
	}

	CSipCallInfo *callinfo = new CSipCallInfo(ret,ret,ret,srccall->did_,srccall->cid_,srccall->srcid_.c_str(),srccall->dstid_.c_str(),SIPREQUST_STATUS::STATUS_SEND_200OK);

	callinfo->set_ssrc(srccall->ssrc_);
	callinfo->set_sdp(sdpcontent);
	update_callinfo(ret,callinfo);

	return 0;
}
int    CSipClientApent::send_200OK(eXosip_t *context_eXosip,int tid,long code)
{
	osip_message_t *answer = NULL; 
	int ret = eXosip_call_build_answer (context_eXosip, tid, code, &answer);
	if(ret < 0)
		return -1;

	ret = eXosip_call_send_answer(context_eXosip, tid, code, answer);
	if(ret <0)
	{
		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] eXosip_call_send_answer error"));
		return -1;
	}

	return 0;
}