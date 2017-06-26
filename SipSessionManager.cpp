#include "SipSessionManager.h"
CSipSessionManager *CSipSessionManager::s_instance_ = NULL;
CSipSessionManager::CSipSessionManager(void)
{
}

CSipSessionManager::~CSipSessionManager(void)
{
}
CSipSessionManager *CSipSessionManager::getinstance()
{
	if(s_instance_ == NULL)
	{
		s_instance_ = new CSipSessionManager();
	}

	return s_instance_;
}

CSipSession *  CSipSessionManager::get_session(string id,int cid)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(session_lock_);
	for(int i=0;i<v_session_.size();i++)
	{
       if(v_session_[i]->check_session(id,cid) == 1)
	   {
		   return v_session_[i];
	   }
	}

	return NULL;
}

int    CSipSessionManager::insert_session(CSipSession * session)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(session_lock_);
    if(session)
	{
		v_session_.push_back(session);
	}
	return 0;
}

int  CSipSessionManager::handle_event(eXosip_t *context_eXosip,eXosip_event_t *evt)
{
	osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
	osip_uri_t* toURI = osip_from_get_url(evt->request->to);
	CSipClientApent *sagent = CSipClientAgentManager::getinstance()->get_sipagent(fromURI->username);
	if(sagent == NULL)
	{
		APTS_LOGMSG((LM_ERROR,"(%t)%D [GB28181ServerTask] 未注册用户 不响应 from:%s,to:%s,event:%d\n",fromURI->host,toURI->host,evt->type));
		return 0;
	}

	CSipClientApent *sipsmsagent = CSipClientAgentManager::getinstance()->get_sipagent(CSipClientAgentManager::getinstance()->sipsmsid());
	CSipClientApent *sipproxy = CSipClientAgentManager::getinstance()->get_sipagent(CSipClientAgentManager::getinstance()->sipproxyid());
	switch(evt->type)
	{
	case  EXOSIP_CALL_CLOSED:
		{
			CSipSession *session = get_session(toURI->username,evt->cid);
			if(session)
			{
				session->handle_bye(context_eXosip,evt);
			}
		}
		break;
	case  EXOSIP_CALL_ACK:
		{
			CSipSession *session = get_session(toURI->username,evt->cid);
			if(session)
			{
				session->handle_ack(context_eXosip,evt);
			}
		}
		break;
	case EXOSIP_CALL_ANSWERED:
		{
			CSipSession *session = get_session(toURI->username,evt->cid);
			if(session)
			{
				session->handle_answered(context_eXosip,evt);
			}
		}
		break;
	case EXOSIP_CALL_INVITE:
		{
		   CSipSession *session = get_session(fromURI->username,evt->cid);
           if(session == NULL)
		   {
			   sdp_message_t *remotesdp= NULL;
			   remotesdp = eXosip_get_remote_sdp(context_eXosip,evt->did);
			   if(remotesdp)
			   {
				   if(strcmp(remotesdp->s_name,"Play") == 0|| strcmp(remotesdp->s_name,"pjmedia") == 0 )
				   {
					   CSipClientApent *sipagent_send = CSipClientAgentManager::getinstance()->get_sipagent(toURI->username); 
					   //创建会话
					   CSipSession *newsession = new CSipSession(sipproxy,sipsmsagent,sipagent_send,sagent,SIPSESSION_TYPE::SESSION_REALTIME_VIDEO);
					   insert_session(newsession);

					   newsession->handle_invite(context_eXosip,evt);
				   }
				   else if(strcmp(remotesdp->s_name,"RecordInfo") == 0)
				   {

				   }
			   }
		   }
		}
		break;
	}

	return 0;
}