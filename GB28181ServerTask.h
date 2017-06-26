/********************************************************************
	@file    	GB28181ServerTask.h
    created:	2017-5-15   12:52
	file base:	GB28181ServerTask
	file ext:	h
	@author:	yuqingbin
	
	@purpose:	
*********************************************************************/
#ifndef __GB_28181_SERVER_TASK_H__
#define __GB_28181_SERVER_TASK_H__

#include "../Common/APTS_Task_Base.h"
#include "../Common/filesystem/IniFile.h"
#include "../Common/comm/AIO_Acceptor.h"
#include "../Common/comm/IPfilter.h"
#include "../Common/SipAdapter/SIP/include/eXosip2/eXosip.h"
#include "../Common/SipAdapter/SIP/SIP/SIPEvent.h"
#include "../Common/SipAdapter/SIP/VideoList/VideoRequest.h"
#include "SdpParse.h"
#include "SipClientAgentManager.h"
#include "SipSessionManager.h"

ACE_FACTORY_DECLARE(APTS_SVC,GB28181ServerTask)
class APTS_SVC_Export GB28181ServerTask:public  APTS_Task_Base, public ACE_Handler
{
	typedef APTS_Task_Base PARENT;
public:
	GB28181ServerTask();
	~GB28181ServerTask();
	virtual int start();
	virtual int handle_timeout (const ACE_Time_Value &time,const void *);
	virtual void handle_time_out(const ACE_Time_Value &tv, const void *act /* = 0 */);

	//SIP代理客户端
	void registertoserver(int expires=3600);
	void send_catalog();

	//SIP服务器
	int svc_exsip_event();
	void process_event( eXosip_event_t *evt );
	void handle_register(eXosip_event_t *evt);
	void handle_invite(eXosip_event_t *evt);
	void handle_message(eXosip_event_t *evt);
	void handle_callanswered(eXosip_event_t *evt);
	void handle_callproceeding(eXosip_event_t *evt);
	void handle_registerresponse(eXosip_event_t *evt,int success);

    //int send_invite(char *fromip,char *fromid,long fromport,char *toip,char* toid,long toport,CallInviteInfo* incallinfo,CSipClientObject *sco,int is_have_sdp=0);
    int send_ack();
	int response_message(const char* content, const char* from, const char* to, const char* evtName);

	void keepalive_sipclient_map(string usrname);

	void printf_osip_message(osip_message_t *message);
	int  getviainfo(osip_message_t* request, string& ip, long& port);
protected:
	virtual int init (int argc, ACE_TCHAR *argv[]);
	virtual int fini (void);
	virtual int svc(void);
	virtual int message_svc(void);
	virtual int open (void * = 0);
	virtual int load_ini();
private:
	eXosip_t *context_eXosip;
	int      listen_port_;
	std::string    listen_ip_;
	std::string    local_id_;
	std::string       userpasswnd;
	//流媒体服务器区域编码
	string       mediaserver_username;
	ACE_thread_t    handle_sipevent_thread_id_; 
    std::string    serverip_;
	long           serverport_;
	std::string    serverid_;
	int            serverenable_;
	long           registerserver_timerid_;
	int            is_registered_;
	int            rid_;
	int            is_auther_;

    CataReqList    catalog_inquirelst_;
public:
};

#endif /* __GB_28181_SERVER_TASK_H__ */