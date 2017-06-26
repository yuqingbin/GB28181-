/********************************************************************
	@file    	GB28181ServerTask.cpp
    created:	2017-5-15   18:01
	file base:	GB28181ServerTask
	file ext:	cpp
	@author:	yuqingbin
	
	@purpose:	
*********************************************************************/
#ifndef APTS_SVC_BUILD_DLL
#define APTS_SVC_BUILD_DLL
#endif
#include "../Common/message/Message_Block_Manager.h"
#include "../Common/comm/Client_Manager.h"
#include "GB28181ServerTask.h"
#include "ace/Reactor.h"
#include "ace/Proactor.h"
#include "ace/Win32_Proactor.h"
#include "../Common/Message/Event.h"
#include "../Common/SipAdapter/SIP/XML/Parase/XMLParse.h"
#include "../Common/SipAdapter/SIP/XML/builder/XMLBuilder.h"
#include "../Common/APTS_Options.h"

#include <winsock2.h>
#include <windows.h>
#include <sstream>

#pragma comment(lib, "osip2.lib")  
#pragma comment(lib, "osipparser2.lib")  
#pragma comment(lib, "eXosip.lib")  
#pragma comment(lib, "Iphlpapi.lib")  
#pragma comment(lib, "Dnsapi.lib")  
#pragma comment(lib, "ws2_32.lib")

//子线程
void handle_sip_event_thread(void* arg);
void handle_catalog_inquire(void *arg);

ACE_FACTORY_DEFINE(APTS_SVC,GB28181ServerTask)
GB28181ServerTask::GB28181ServerTask()
:listen_port_(0)
,context_eXosip(NULL)
,serverenable_(0)
,serverport_(0)
,registerserver_timerid_(0)
,rid_(-1)
,is_auther_(0)
,is_registered_(0)
{
	//子线程明细
	handle_sipevent_thread_id_ = 1;
}
GB28181ServerTask::~GB28181ServerTask()
{
    if(is_registered_ == 1)
	{
		registertoserver(0);
	}
}
int GB28181ServerTask::init(int argc, ACE_TCHAR *argv[])
{
	this->msg_queue ()->high_water_mark (32*1024*1024);
	this->msg_queue ()->low_water_mark (16*1024*1024);
	if ( PARENT::init(argc,argv) == -1)
		return -1;
	return 0;
}

int GB28181ServerTask::fini()
{
	if (fini_)
	{
		return 0;
	}
	fini_ = true;
	//释放内存
	ACE_Reactor::instance()->cancel_timer(this);
	if ( attach_task_ )
	{
		attach_task_->fini();
	}
	// 关闭与任务相关的消息队列，然后删除队列中的所有消息，并发信号给池中的线程
	this->flush();
	// 等待线程池中的线程退出
	this->wait();
	// 关闭
	this->close();
    CLIENT_MANAGER::close();
	if (context_eXosip)
	{
		eXosip_quit(context_eXosip);
		context_eXosip = NULL;
	}
	ACE_DEBUG((LM_INFO,"(%t)%D 关闭GB28181 协议服务器端通信任务...\n"));
	return 0;
}

int GB28181ServerTask::load_ini()
{
	OPTIONS::instance(this->get_option());
	CLIENT_INFO_MANAGER::instance(this->client_info_manager());
	ini_file_ = OPTIONS::instance()->app_path_ + "\\" + ini_file_;

	IniFile inifile(this->ini_file_.c_str());
	this->listen_port_ = inifile.ReadInt("GB28181","LocalPort",5080);
	this->listen_ip_ = inifile.ReadString("GB28181","LocalIP","");
    this->mediaserver_username = inifile.ReadString("GB28181","MediaServerUserName","sms1234567");
    
	this->local_id_ = inifile.ReadString("GB28181","LocalID","123456788");
	this->serverip_ = inifile.ReadString("GB28181","ServerIP","127.0.0.1");
	this->serverport_ = inifile.ReadInt("GB28181","ServerPort",5000);
	this->serverid_ =inifile.ReadString("GB28181","ServerID","123456789001");
    this->serverenable_ = inifile.ReadInt("GB28181","ServerEnable",0);
	this->userpasswnd = inifile.ReadString("GB28181","UserPasswd","12345678");

	CSipClientApent *sca = new CSipClientApent((char*)listen_ip_.c_str(),listen_port_,(char*)local_id_.c_str(),(char*)local_id_.c_str(),0,AGENT_GS,SIP_REGISTERED);
	CSipClientAgentManager::getinstance()->bind_sipagent(local_id_,sca);
	CSipClientAgentManager::getinstance()->set_sipproxy(local_id_);  //本级域

	return 0;
}

int GB28181ServerTask::start()
{
	PARENT::start();
	/// 启动处理线程
	this->open();
	//启动定时器
#if _DEBUG
	ACE_Time_Value timeout(600);
#else
	ACE_Time_Value timeout(600);
#endif
	ACE_Reactor::instance()->schedule_timer(this,0,ACE_Time_Value::zero,timeout);

	//sip代理服务器
	if (NULL == context_eXosip)
	{		
		context_eXosip = eXosip_malloc();
		int ret = eXosip_init(context_eXosip);
		if (ret != 0)
		{
			APTS_LOGMSG((LM_ERROR,"(%t)%D [%s] can't init exosip...\n",get_task_name()));
			eXosip_quit(context_eXosip);
			context_eXosip = NULL;
			return -1;
		}
		else
		{
          if (eXosip_listen_addr(context_eXosip, IPPROTO_UDP, listen_ip_.c_str(), listen_port_, AF_INET, 0))
		  {
            APTS_LOGMSG((LM_ERROR,"(%t)%D[%s] exosip listen (%s:%d) failed \n",get_task_name(),listen_ip_.c_str(),listen_port_));
		    //eXosip_quit(context_eXosip);
			return 0;
		  }
		  eXosip_masquerade_contact(context_eXosip, listen_ip_.c_str(), listen_port_);

		  if(!ACE_Thread_Manager::instance()->thread_within(handle_sipevent_thread_id_))
		  {
			  ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(handle_sip_event_thread), this, 
				  THR_NEW_LWP | THR_DETACHED | THR_INHERIT_SCHED, &handle_sipevent_thread_id_);
		  }

		  registerserver_timerid_ = ACE_Proactor::instance()->schedule_timer( *this,(const void*)&registerserver_timerid_, ACE_Time_Value(5), ACE_Time_Value(3600));
		}
	}
	return 0;
}

int GB28181ServerTask::open(void * /* = 0 */)
{
	int result = PARENT::open();
	return result;
}
int GB28181ServerTask::svc()
{
	APTS_LOGMSG((LM_INFO,"(%t)%D [ACE_Proactor]GB28181 Server通信任务正在运行(ThreadCount %d)...\n",thr_num_));
	ACE_Proactor::run_event_loop();
	return 0;
}
int GB28181ServerTask::message_svc()
{
	APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask]服务器端消息处理任务正在运行\n"));

	for (ACE_Message_Block *blk; getq (blk) != -1; ) 
	{
		//Event_Key *event_key = (Event_Key *) blk->rd_ptr ();
		MESSAGE_BLOCK_MANAGER::instance()->release_message(blk);
		/*Event_Key *event_key = (Event_Key *) blk->rd_ptr ();
		Asynch_Comm_Handler_Base* handler = 0;
		ACE_Message_Block* mb = blk->cont();
		if (CLIENT_MANAGER::instance()->find(event_key->dest_id_, handler) == 0)
		{
			if (handler)
			{
				handler->initiate_write(*mb, mb->length());
				MESSAGE_BLOCK_MANAGER::instance()->release_event_block(blk);
				APTS_LOGMSG((LM_DEBUG, "[GB28181ServerTask] post BA:%x, MSG:%x to CLIENT:%d\n", 
					event_key->business_area_, event_key->msgid_, event_key->dest_id_));
			}
			else
			{
				APTS_LOGMSG((LM_DEBUG,"[GB28181ServerTask]Handler = %x\n", handler));
				MESSAGE_BLOCK_MANAGER::instance()->release_message(blk);
			}
		}
		else
		{
			MESSAGE_BLOCK_MANAGER::instance()->release_message(blk);
		}*/

	}
	return 0;
}

int GB28181ServerTask::handle_timeout(const ACE_Time_Value &time, const void *)
{
	
	return 0;
}
void GB28181ServerTask::handle_time_out(const ACE_Time_Value &tv, const void *act /* = 0 */)
{
	long timerid = *((int*)act);
	if( registerserver_timerid_ == timerid )
	{
		registertoserver(3600);
	}
}
void handle_sip_event_thread(void* arg)
{
	GB28181ServerTask* task = (GB28181ServerTask*)arg;
	if (task)
	{
		task->svc_exsip_event();
	}
}

void handle_catalog_inquire(void* arg)
{
	GB28181ServerTask* task = (GB28181ServerTask*)arg;
	if (task)
	{
      task->send_catalog();
	}
}
void GB28181ServerTask::send_catalog()
{
	CatalogRequest data;
	if (!catalog_inquirelst_.Get_Rm_CataReq(data))
	{
		return;
	}

	TreeNodeList sendList;
	TreeNode node; 
	map<string,CSipClientApent*>   sipagentmaptemp;
	CSipClientAgentManager::getinstance()->get_sipagent(sipagentmaptemp);
	for (map<string,CSipClientApent*>::iterator iter=sipagentmaptemp.begin();iter!=sipagentmaptemp.end();++iter)
	{
		if(iter->second&&iter->second->agenttype() == AGENT_DVR)
		{
			node.id = iter->second->id();
			node.name = iter->second->name();
			node.parentID = local_id_;
			node.isParental = true;
			node.state = true;
			sendList.push_back(node);
		}
	}
	vector<string> msgs;
	XMLBuilder::GetXMLData(sendList, data, msgs);
	vector<string>::iterator it = msgs.begin();
	while (it!=msgs.end())
	{
		if( response_message((*it).c_str(),data.from.c_str(),data.to.c_str(),"MESSAGE") )
			APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] send_catalog  from:%s,to:%s,catalog:%s\n",data.from.c_str(),data.to.c_str(),(*it).c_str()));
		Sleep(100);
		it++;
	}
}
int GB28181ServerTask::response_message(const char* content, const char* from, const char* to, const char* evtName)
{
	osip_message_t *msg = NULL; 
	int rid = eXosip_message_build_request(context_eXosip, &msg,
		evtName, to, from, NULL);
	if(rid < 0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] eXosip_message_build_request error from:%s,to:%s",from,to));
		return 0;
	}
	osip_message_set_body (msg, content, ::strlen(content));
	osip_message_set_content_type (msg, "application/MANSCDP+xml");
	int result = eXosip_message_send_request(context_eXosip, msg);
	if (result < 0)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] response_message error from:%s,to:%s",from,to));
		return 0;
	}
    
	return 1;
}

void GB28181ServerTask::registertoserver(int expires)
{
	eXosip_event_t *je  = NULL;
	osip_message_t *reg = NULL;
	char from[100];/*sip:主叫用户名@被叫IP地址*/
	char to[100];/*sip:被叫IP地址:被叫IP端口*/

	memset(from, 0, 100);
	memset(to, 0, 100);
	sprintf(from, "sip:%s@%s", local_id_.c_str(), listen_ip_.c_str());
	sprintf(to, "sip:%s:%d", serverip_.c_str(), serverport_);
	
	if(is_auther_ == 0 && rid_ < 0)
	{
		rid_ = eXosip_register_build_initial_register(context_eXosip,from, to, NULL, expires, &reg);
		int ret = eXosip_register_send_register(context_eXosip, rid_, reg);
	}
	else
	{
		eXosip_add_authentication_info(context_eXosip, local_id_.c_str(), 
			local_id_.c_str(), userpasswnd.c_str(), "md5", NULL); 

		int ret = eXosip_register_build_register(context_eXosip, rid_, expires, &reg);

		ret = eXosip_register_send_register(context_eXosip, rid_, reg);
	}

}

void GB28181ServerTask::printf_osip_message(osip_message_t *message)
{
	char *buf = NULL;
	if(message)
	{ 
		osip_message_to_str(message,&buf,&(message->message_length));
		APTS_LOGMSG((LM_DEBUG,"(%t)%D [GB28181ServerTask]printf_osip_message \n%s\n", buf));
		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask]printf_osip_message \n%s\n", buf));
		osip_free(buf);
		buf=NULL;
	}

}

int GB28181ServerTask::svc_exsip_event()
{
    eXosip_event_t *evt = NULL;
	for(;;)  
	{  
		//侦听是否有消息到来  
		if (NULL == context_eXosip)
		{
			continue;;
		}
		evt = eXosip_event_wait (context_eXosip,0,50);   
		eXosip_lock (context_eXosip); 
		eXosip_default_action (context_eXosip,evt);
		eXosip_unlock (context_eXosip);  

		//处理事件
        process_event(evt);
		//free
		if (evt)
		{
			eXosip_lock (context_eXosip);  
			eXosip_event_free(evt);
			eXosip_unlock(context_eXosip);
			evt = NULL;
		}
	}
	return 0;
}

int GB28181ServerTask::getviainfo(osip_message_t* request, string& ip, long& port)
{
	osip_via_t* el = NULL;
	int index = 0;
	while (osip_message_get_via(request, index, &el) != OSIP_UNDEFINED_ERROR)
	{
		index++;
		if (!el)
		{
			continue;
		}

		char* host = via_get_host(el);
		char* pPort = via_get_port(el);

		if (host && pPort)
		{
			ip = host;
			port = atol(pPort);
			return 1;
		}
	}

	return 0;
}
void GB28181ServerTask::handle_register(eXosip_event_t *evt)
{
	osip_authorization_t *dest = NULL;
	int ret = osip_message_get_authorization(evt->request,0,&dest);
	if(dest != NULL && ret == 0)
	{
		osip_message_t *answer= NULL;
		osip_contact_t *contact= NULL;
		char *buf;
		eXosip_message_build_answer(context_eXosip,evt->tid,200,&answer);
		CString ts = CTime::GetCurrentTime().Format("%Y%m%dT%H:%M:%S");
		osip_message_set_date(answer,ts.GetBuffer());
		osip_message_get_contact(evt->request,0,&contact);
		osip_contact_to_str(contact,&buf);
		osip_message_set_contact(answer,buf);
		osip_free(buf);
		if(eXosip_message_send_answer(context_eXosip,evt->tid, 200,answer)==0)//成功注册返回
		{
			osip_header_t * h;
			int  expires=0;
			if(osip_message_header_get_byname(evt->request,"expires",0,&h) != -1)
			{
				expires = atoi(h->hvalue);
				if(expires >0)
				{
					osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
					string ip;
					long port =0;
					getviainfo(evt->request,ip,port);
					if(strcmp(mediaserver_username.c_str(),fromURI->username) == 0 )
					{
						CSipClientApent *sca = new CSipClientApent((char*)ip.c_str(),port,fromURI->username,fromURI->username,CTime::GetCurrentTime().GetTime(),AGENT_SMS,SIP_REGISTERED);
						CSipClientAgentManager::getinstance()->bind_sipagent(fromURI->username,sca,1);
						CSipClientAgentManager::getinstance()->set_sipsms(mediaserver_username);
					}
					else
					{
						CSipClientApent *sca = new CSipClientApent((char*)ip.c_str(),port,fromURI->username,fromURI->username,CTime::GetCurrentTime().GetTime(),AGENT_DVR,SIP_REGISTERED);
						CSipClientAgentManager::getinstance()->bind_sipagent(fromURI->username,sca);	
					}
				}
				else if(expires == 0)
				{
					osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
					CSipClientAgentManager::getinstance()->unbind_sipagnet(fromURI->username);
					APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] 注销from:%s@%s:%d,%s\n",fromURI->username,fromURI->host,fromURI->port,fromURI->username));

				}
				else
				{
					printf("error\n");
				}
			}
		}
		return;
	}
	else
	{
		osip_message_t * answer;
		int ret = eXosip_message_build_answer(context_eXosip,evt->tid, 401,&answer);

		ret = osip_message_set_www_authenticate(answer,"Digest realm=\"3402000000\",nonce=\"5396ea7d22a90f95\"");
		ret = eXosip_message_send_answer(context_eXosip,evt->tid, 401,answer);//401返回
		return;
	}
	
}

void GB28181ServerTask::handle_callproceeding(eXosip_event_t *evt)
{
	osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
	osip_uri_t* toURI = osip_from_get_url(evt->request->to);
	APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] call proceeding callid:%d,fromuser:%s\n",evt->cid,fromURI->username));
}

void GB28181ServerTask::handle_message(eXosip_event_t *evt)
{
   osip_body_t* body = NULL;
   osip_content_type_t * osiptype = NULL;
   osiptype = osip_message_get_content_type(evt->request);
   osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
   osip_uri_t* toURI = osip_from_get_url(evt->request->to);
   if(osiptype != NULL)
   {
	   std::string type = osiptype->type;
	   std::string subtype = osiptype->subtype;
	   APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] Message Type:%s,SubType:%s\n",type.c_str(),subtype.c_str()));
	   
	   if(subtype.find("xml") != std::string::npos)
	   {
		   //消息体是xml格式,
		   char* message = NULL;
		   osip_message_get_body(evt->request, 0, &body);
		   size_t size = 0;
		   if (osip_body_to_str(body, &message, &size) != OSIP_SUCCESS)
		   {
			   return;
		   }

		   if(message&&strlen(message)>0)
		   {
				//先回应200OK
			   osip_message_t *answer = NULL; 
			   int i = eXosip_message_build_answer (context_eXosip, evt->tid, 200, &answer);
			   osip_message_set_content_type (answer, "application/MANSCDP+xml");
			   eXosip_message_send_answer(context_eXosip,evt->tid, 200, answer);

			   std::string cmdtype = XMLParse::GetCMDType(message);

			   //1.心跳信息
               if(cmdtype.compare("Keepalive") == 0)
			   {
				   //心跳数据
				   keepalive_sipclient_map(fromURI->username);
			   }
			   //2.设备目录查询
			   else if(cmdtype.compare("Catalog") == 0 )
			   {
                   CatalogRequest data;
				   char from[100];/*sip:主叫用户名@被叫IP地址*/
				   char to[100];/*sip:被叫IP地址:被叫IP端口*/

				   memset(from, 0, 100);
				   memset(to, 0, 100);
				   sprintf(from, "sip:%s@%s", local_id_.c_str(), listen_ip_.c_str());
				   sprintf(to, "sip:%s:%s", fromURI->host, fromURI->port);
				   data.from = from;
				   data.to = to;
				   std::string  sn = XMLParse::GetSN(message);
				   std::string  deviceid = XMLParse::GetDeviceID(message);
				   data.sn = atoi(sn.c_str());
				   data.deviceId = deviceid;

				   if (catalog_inquirelst_.Add(data))
				   {
					   ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(handle_catalog_inquire), this, 
						   THR_NEW_LWP | THR_DETACHED | THR_INHERIT_SCHED, NULL);
				   }
			   }
			   else if(cmdtype.compare("RecordInfo") == 0)
			   {
				   //历史视频文件检索
                   
			   }

			   osip_free(message);
		   }
	   }
	   
   }
}

void GB28181ServerTask::handle_registerresponse(eXosip_event_t *evt,int success)
{
	printf_osip_message(evt->request);
	if(success == 1)
	{
		osip_header_t * h;
		int  expires=0;
		if(osip_message_header_get_byname(evt->request,"expires",0,&h) != -1)
		{
			expires = atoi(h->hvalue);
			if(expires >0)
			{
				//添加到客户端管理
				osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
				osip_uri_t* toURI = osip_from_get_url(evt->request->to);
				CSipClientApent *sca = new CSipClientApent((char*)serverip_.c_str(),serverport_,(char*)serverid_.c_str(),(char*)serverid_.c_str(),0,AGENT_GA,SIP_REGISTERED);
				CSipClientAgentManager::getinstance()->bind_sipagent(serverid_,sca);
				is_registered_ = 1;
			}
			else
			{
				is_registered_ = 0;
				CSipClientAgentManager::getinstance()->unbind_sipagnet(serverid_);
			}
		}
	  
	}
	else
	{
      registertoserver(3600);
	}
}

void GB28181ServerTask::keepalive_sipclient_map(string usrname)
{
	CSipClientApent *sa = CSipClientAgentManager::getinstance()->get_sipagent(usrname);
	if(sa)
		sa->keeptime(CTime::GetCurrentTime().GetTime());
}

void GB28181ServerTask::process_event( eXosip_event_t *evt )
{
	if(evt)
	{
		APTS_LOGMSG((LM_INFO,"(%t)%D [GB28181ServerTask] eXosip_event_type:%d,did:%d,cid:%d\n",evt->type,evt->did,evt->cid));

		switch(evt->type)
		{
		case EXOSIP_CALL_TIMEOUT:
		case EXOSIP_CALL_CANCELLED:
		case EXOSIP_CALL_CLOSED:
		case EXOSIP_CALL_INVITE:
		case EXOSIP_CALL_ANSWERED:
		case EXOSIP_CALL_ACK:
			{
				if(!evt->request)
					break;
				printf_osip_message(evt->request);
				//CSipClientAgentManager::getinstance()->handle_event(context_eXosip,evt);
				CSipSessionManager::getinstance()->handle_event(context_eXosip,evt);
			}
			break;
		case EXOSIP_CALL_PROCEEDING:
			{
				if(!evt->request)
					break;
				/*printf_osip_message(evt->request);
				handle_callproceeding(evt);*/
			}
			break;
		case EXOSIP_REGISTRATION_SUCCESS:	// 注册成功
			{
				handle_registerresponse(evt,1);
			}
			break;
		case EXOSIP_REGISTRATION_FAILURE:	// 注册失败
			{
				handle_registerresponse(evt,0);
			}
			break;
		case EXOSIP_MESSAGE_NEW:
			{
				if(!evt->request)
					break;
				printf_osip_message(evt->request);
				std::string method = osip_message_get_method(evt->request);
				if(method.compare("REGISTER") == 0)
				{
					handle_register(evt);
				}
				else if(method.compare("MESSAGE") == 0)
				{
					handle_message(evt);
				}
			}
			break;
		default:
			break;
		}
	}
}
