#include "SipSession.h"

CSipSession::CSipSession(void)
:session_type_(0)
,sipagent_proxy_(NULL)
,sipagent_receiver_(NULL)
,sipagent_sender_(NULL)
,sipagent_sms_(NULL)
{
}

CSipSession::~CSipSession(void)
{
}

CSipSession::CSipSession(CSipClientApent *proxy,CSipClientApent *sms,CSipClientApent *sender,CSipClientApent *receiver,int seesiontype)
{
	this->sipagent_proxy_ = proxy;
	this->sipagent_receiver_ = receiver;
	this->sipagent_sender_ = sender;
	this->sipagent_sms_ = sms;
	this->session_type_ = seesiontype;
}

void CSipSession::insert_call(int cid)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
  if( cid >= 0 )
   v_callid_.push_back(cid);
}

void CSipSession::remove_call(int cid)
{
  ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
  vector<int>::iterator iter = v_callid_.begin();
  for (;iter!=v_callid_.end();)
  {
	  if(*iter == cid)
		  iter = v_callid_.erase(iter);
	  else
		  ++iter;
  }
}

int CSipSession::check_session(string id,int cid)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> locker(sipcall_lock_);
	vector<int>::iterator iter = v_callid_.begin();
	for (;iter!=v_callid_.end();++iter)
	{
		if(*iter == cid)
			return 1;
	}

	//if(sipagent_sms_ && 
	//	/*id.compare(sipagent_sms_->id().c_str()) == 0 && */
	//	sipagent_sms_->check_callinfo(cid) == 1 )
	//{
	//	return 1;
	//}
	//else if(sipagent_receiver_ && 
	//	/*id.compare(sipagent_receiver_->id().c_str()) == 0 && */
	//	sipagent_receiver_->check_callinfo(cid) == 1 )
	//{
	//	return 1;
	//}
	//else if(sipagent_sender_ && 
	//	/*id.compare(sipagent_sender_->id().c_str()) == 0 &&*/ 
	//	sipagent_sender_->check_callinfo(cid) == 1 )
	//{
	//	return 1;
	//}
	//else if(sipagent_proxy_ && 
	//	/*id.compare(sipagent_proxy_->id().c_str()) == 0 && */
	//	sipagent_proxy_->check_callinfo(cid) == 1 )
	//{
	//	return 1;
	//}
	
	return 0;
}

int CSipSession::handle_invite(eXosip_t *context_eXosip,eXosip_event_t *evt)
{
	osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
	osip_uri_t* toURI = osip_from_get_url(evt->request->to);
	if(session_type_ == SIPSESSION_TYPE::SESSION_REALTIME_VIDEO)
	{
		CSipCallInfo *callinfo = new CSipCallInfo(evt->tid,evt->cid,evt->did,0,0,fromURI->username,toURI->username,SIPREQUST_STATUS::STATUS_RECE_INVITE_SDP);

		sdp_message_t *remotesdp= NULL;
		remotesdp = eXosip_get_remote_sdp(context_eXosip,evt->did);
		if(remotesdp)
		{
			string ssrc;
			CSdpParse sdpp;

			sdpp.init(evt->request->message);
			sdpp.getvalue(ssrc,"y=");
			callinfo->set_ssrc(ssrc);

			char* csdp = NULL;
			sdp_message_to_str(remotesdp, &csdp);
			if(csdp)
			{
				callinfo->set_sdp(csdp);
				osip_free(csdp);
			}
		}
		sipagent_receiver_->update_callinfo(evt->cid,callinfo);

		sipagent_receiver_->set_current_callinfo(evt->tid,evt->did,evt->cid);
		recevier_cid_ = evt->cid;
		recevier_tid_ = evt->tid;
		recevier_did_ = evt->did;

        insert_call(evt->cid);
		int ret = sipagent_sms_->send_invite(context_eXosip,sipagent_proxy_,callinfo,sipagent_receiver_->id(),sipagent_sender_->id());
	    insert_call(ret);
	}
  return 0;
}

int CSipSession::handle_answered(eXosip_t *context_eXosip,eXosip_event_t *evt)
{
  osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
  osip_uri_t* toURI = osip_from_get_url(evt->request->to);

  printf("handle_answered evt->cid:%d,from:%s,to:%s\n",evt->cid,fromURI->username,toURI->username);
  int ret = 0;
  if(session_type_ == SIPSESSION_TYPE::SESSION_REALTIME_VIDEO)
  {
	  CSipCallInfo *callinfo = sipagent_sms_->get_callinfo(evt->cid);
	  if( sipagent_sms_->id().compare(toURI->username) == 0 )
	  {
		 //printf("evt->cid:%d,callstate:%d\n",evt->cid,callinfo->callstate_);
         sipagent_sms_->set_current_callinfo(evt->tid,evt->did,evt->cid);
		 sms_cid_ = evt->cid;
		 sms_tid_ = evt->tid;
		 sms_did_ = evt->did;
		  if(callinfo && callinfo->callstate_ == SIPREQUST_STATUS::STATUS_SEND_INVITE_NOSDP)
		  {
			  callinfo->set_callstate(SIPREQUST_STATUS::STATUS_RECE_200OK);
			  callinfo->set_did(evt->did);
			  callinfo->set_cid(evt->cid);
			  if(sipagent_sender_)
			  {
				  int ret = sipagent_sender_->send_invite_sdp(context_eXosip,evt,sipagent_proxy_,callinfo,sipagent_proxy_->id(),sipagent_sender_->id());
			      insert_call(ret);
			  }
		  }
		  else if(callinfo && callinfo->callstate_ == SIPREQUST_STATUS::STATUS_SEND_INVITE_SDP)
		  {
			  callinfo->set_callstate(SIPREQUST_STATUS::STATUS_RECE_200OK);
			  callinfo->set_did(evt->did);
			  callinfo->set_cid(evt->cid);

			  sdp_message_t *remotesdp= NULL;
			  remotesdp = eXosip_get_remote_sdp(context_eXosip,evt->did);
			  string sdpcontent;
			  if(remotesdp)
			  {
				  char* csdp = NULL;
				  sdp_message_to_str(remotesdp, &csdp);
				  if(csdp)
				  {
					  sdpcontent = csdp;
					  osip_free(csdp);
				  }
			  }

			  sipagent_receiver_->send_200OK(context_eXosip,recevier_tid_,200,sdpcontent,callinfo);
		  }
	  }
	  else if( sipagent_sender_->id().compare(toURI->username) == 0 )
	  {
		  CSipCallInfo *callinfo = sipagent_sender_->get_callinfo(evt->cid);
		  if(callinfo)
		  {
			  sipagent_sender_->set_current_callinfo(evt->tid,evt->did,evt->cid);
			  sender_cid_ = evt->cid;
			  sender_tid_ = evt->tid;
			  sender_did_ = evt->did;

			  if(callinfo && callinfo->callstate_ == SIPREQUST_STATUS::STATUS_SEND_INVITE_SDP)
			  {
				  callinfo->set_callstate(SIPREQUST_STATUS::STATUS_RECE_200OK);
				  callinfo->set_cid(evt->cid);
				  callinfo->set_did(evt->did);

				  ret = sipagent_sender_->send_ack(context_eXosip,"",evt->did);
				  if(ret < 0)
				  {
					  return -1;
				  }

				  callinfo->set_callstate(SIPREQUST_STATUS::STATUS_SEND_ACK);

				  sdp_message_t *remotesdp= NULL;
				  remotesdp = eXosip_get_remote_sdp(context_eXosip,evt->did);
				  string sdpcontent;
				  if(remotesdp)
				  {
					  char* csdp = NULL;
					  sdp_message_to_str(remotesdp, &csdp);
					  if(csdp)
					  {
						  sdpcontent = csdp;
						  osip_free(csdp);
					  }
				  }
				  ret = sipagent_sms_->send_ack(context_eXosip,sdpcontent,sms_did_,1);
				  if(ret<0)
				  {
					  return -1;
				  }

				  CSipCallInfo *recvcallinfo = sipagent_receiver_->get_callinfo(sipagent_receiver_->cid_);
				  if(recvcallinfo && recvcallinfo->callstate_ == SIPREQUST_STATUS::STATUS_RECE_INVITE_SDP)
				  {
					  //发送给流媒体 invite sdp
					  sipagent_sms_->send_invite_sdp(context_eXosip,recvcallinfo->sdp_,sipagent_proxy_,recvcallinfo,sipagent_receiver_->id(),sipagent_sender_->id());
				  }
			  }
		  }
	  }
  }
  return 0;
}

int CSipSession::handle_ack(eXosip_t *context_eXosip,eXosip_event_t *evt)
{
	osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
	osip_uri_t* toURI = osip_from_get_url(evt->request->to);
	int ret = 0;
	if(session_type_ == SIPSESSION_TYPE::SESSION_REALTIME_VIDEO)
	{
       if( sipagent_receiver_->id().compare(fromURI->username) == 0 )
	   {
           sipagent_sms_->send_ack(context_eXosip,"",sipagent_sms_->did_);
	   }
	}
	return 0;
}

int CSipSession::handle_bye(eXosip_t *context_eXosip,eXosip_event_t *evt)
{
	osip_uri_t* fromURI = osip_from_get_url(evt->request->from);
	osip_uri_t* toURI = osip_from_get_url(evt->request->to);
	int ret = 0;
	if(session_type_ == SIPSESSION_TYPE::SESSION_REALTIME_VIDEO)
	{
		remove_call(evt->cid);
		if( sipagent_receiver_->id().compare(fromURI->username) == 0 )
		{
			sipagent_receiver_->send_200OK(context_eXosip,evt->tid,200);

			vector<CSipCallInfo *> vcallinfo;
			sipagent_sms_->get_callinfo(evt->cid,vcallinfo);
			for (int i=0;i<vcallinfo.size();i++)
			{
				if(vcallinfo[i])
					eXosip_call_terminate(context_eXosip,vcallinfo[i]->cid_,vcallinfo[i]->did_);

				vector<CSipCallInfo *> vDVRcallinfo;
				sipagent_sender_->get_callinfo(vcallinfo[i]->cid_,vDVRcallinfo);
				for (int i=0;i<vDVRcallinfo.size();i++)
				{
					if(vDVRcallinfo[i])
						eXosip_call_terminate(context_eXosip,vDVRcallinfo[i]->cid_,vDVRcallinfo[i]->did_);
				}
			}
		}
	}
	return 0;
}