#pragma once
#include "../Common/APTS_Task_Base.h"
#include "../Common/filesystem/IniFile.h"
#include "../Common/comm/AIO_Acceptor.h"
#include "../Common/comm/IPfilter.h"
#include "../Common/SipAdapter/SIP/include/eXosip2/eXosip.h"
#include "../Common/SipAdapter/SIP/SIP/SIPEvent.h"
#include "../Common/SipAdapter/SIP/VideoList/VideoRequest.h"
#include "SipDefine.h"
#include "SdpParse.h"

class CSipCallInfo
{
public:
	CSipCallInfo(void)
		:tid_(0)
		,cid_(0)
		,did_(0)
		,callstate_(0)
		,srccid_(0)
		,srcdid_(0)
	{

	}
	CSipCallInfo(int tid,int cid,int did,int srcdid,int srccid,const char *srcid,const char *dstid,int callstate)
	{
       this->tid_ = tid;
	   this->cid_ = cid;
	   this->did_ = did;
	   this->srccid_ = srccid;
	   this->srcdid_ = srcdid;
	   this->srcid_ = srcid;
	   this->dstid_ = dstid;
	   this->callstate_ = callstate;
	}
    
	void set_sdp(string sdp){this->sdp_ = sdp;};
	void set_callstate(int state){this->callstate_ = state;};
	void set_ssrc(string ssrc){ssrc_ = ssrc;};
	void set_did(int did){this->did_ = did;};
	void set_cid(int cid){this->cid_ = cid;};
	int  tid_;
	int  cid_;
	int  did_;

	string srcid_;
	int srccid_;
	int srcdid_;

	string dstid_;

	string sdp_;
	string ssrc_;

	int    callstate_;
};

class CSipClientApent
{
public:
	CSipClientApent(void);
	~CSipClientApent(void);
	CSipClientApent(char *ip,long port,char *id,char *name,time_t kt,int agenttype,int registerstate);

	string ip(){return ip_;};
	long port(){return port_;};
	string id(){return id_;};
	string name(){return name_;};
	time_t keeptime(){return keeplive_time_;};
	int    registerstate(){return register_state_;};
	int    agenttype(){return agent_type_;};
	void   keeptime(time_t t){keeplive_time_ = t;};

	void   update_callinfo(int cid,CSipCallInfo *sci);
	void   remove_callinfo(int cid);
	CSipCallInfo * get_callinfo(int cid);
	void  get_callinfo(int srccid,vector<CSipCallInfo*> &vt);
	int   check_callinfo(int cid);

	void  set_current_callinfo(int tid,int did,int cid){this->cid_ = cid;this->tid_ = tid;
	this->did_ = did;};

	//handle call for 
	int    send_invite(eXosip_t *context_eXosip,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid);
	int    send_invite_sdp(eXosip_t *context_eXosip,eXosip_event_t *evt,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid);
	int    send_invite_sdp(eXosip_t *context_eXosip,string sdpcontent,CSipClientApent *sipproxy,CSipCallInfo *srccall,string fromid,string toid);
    int    send_ack(eXosip_t *context_eXosip,string sdpcontent,int did,int ishavesdp=0);
	int    send_200OK(eXosip_t *context_eXosip,int tid,long code,string sdpcontent,CSipCallInfo *srccall);
	int    send_200OK(eXosip_t *context_eXosip,int tid,long code);
	int    send_bye(eXosip_t *context_eXosip);
public:
	int cid_;
	int did_;
	int tid_;

private:
	int  agent_type_;
	int  register_state_;

	string  ip_;
	long  port_;
	string  id_;
	string  name_;

	time_t  keeplive_time_;

	ACE_Recursive_Thread_Mutex    sipcall_lock_;
	map<int,CSipCallInfo *>   sip_call_map_;
};
