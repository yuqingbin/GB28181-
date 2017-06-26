#pragma once
#include <string>

using namespace std;
class CSdpParse
{
public:
	CSdpParse(void);
	~CSdpParse(void);
	void init(char* sdpbuf);
	int getvalue(string &out,char *key,int offset =0);
private:
	char *  sdpbuf_;
};
