#include "SdpParse.h"

CSdpParse::CSdpParse(void)
:sdpbuf_(NULL)
{
}

CSdpParse::~CSdpParse(void)
{
}

void CSdpParse::init(char* sdpbuf)
{
	sdpbuf_ = sdpbuf;
}

int CSdpParse::getvalue(string &out,char *key,int offset)
{
   if(sdpbuf_ == NULL)
	   return -1;

   std::string sdp(sdpbuf_);
   int endpos = 0;
   int pos = sdp.find(key);
   if(pos != -1)
   {
	   endpos = sdp.find("\r\n");
	   out = sdp.substr(pos+1,endpos-pos-2);
   }
   return endpos;
}