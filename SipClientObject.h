#pragma once
#include "../Common/APTS_Task_Base.h"
#include "../Common/SipAdapter/SIP/include/eXosip2/eXosip.h"
#include "SipDefine.h"
#include <string>
using namespace std;



class CallInviteInfo
{
public:
	CallInviteInfo(void)
		:did(0)
		,tid(0)
		,cid(0)
		,call_status(STATUS_IDLE)
		,smsdid(0)
		,smscid(0)
	{
		sdp = "";
	}
	~CallInviteInfo()
	{

	}
	int did;
	int tid;
	int cid;
	string from;
	string to;
	string dstid;
	string srcid;
	string sdp;
	string ssrc;

	//媒体服务器信息、媒体接收者
	string   recvip;
	string   recvport;
    int      smsdid;
	int      smscid;

    //媒体发送者
	string   sendip;
	string   sendport;
	int      devdid;
	int      devcid;

	int  call_status;
};

class CSipClientObject
{
public:
	CSipClientObject(void)
		:registerstate(SIP_UNREGISTERED)
		,clienttype(0)
		,keepalivetime(0)
	{
	};
	~CSipClientObject(void);

	void insertcall(int cid,CallInviteInfo* cinfo);
    CallInviteInfo* getcallinfobycid(int cid);

public:
	int  clienttype;
	int   registerstate;  //0:未注册  1:已注册
	string   username;    //登录ID
	string   host;        //客户端IP地址
	long     port;
	time_t      keepalivetime;
    
	ACE_Recursive_Thread_Mutex    callinvite_lock_;
	map<int,CallInviteInfo*>  callinvite_map;
};
