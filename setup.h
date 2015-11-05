/*
	k/v???ò????????? by ?????? 2004.8.24
	
	update by ?????? 07.4.12
  update by lijian2 2011.08.20
*/
#ifndef _CWQ_SETUP_H
#define _CWQ_SETUP_H

#include <string>
#include <map>
#include <vector>

using namespace std;

class Setup
{
protected:
	string	_empty_string;
  string  _filename;

	bool LoadFromFile(const char *filePath, map<string, string>& keys, vector<string>& urls);
	virtual void GetAll(map<string, string>& keys);
	string& GetVal(map<string, string>& keys, const char *name);
	int GetInt(map<string, string>& keys, const char *name, int defaultVal);
	void GetString(map<string, string>& keys, const char *name, string& dst, const char*& dstPtr);

public:
	Setup() {};
	virtual ~Setup() {};

	bool Load(const string setupPath);
  void GetFileName(string &filename) const; 

	string	S_PID_FILE_PATH;
	string	S_LOG_FILE_PREFIX;
	string	S_DEBUG_FILE_PREFIX;
	
	const char* PID_FILE_PATH;
	const char*	LOG_FILE_PREFIX;
	const char*	DEBUG_FILE_PREFIX;

	int		LISTEN_PORT;
	int		LISTEN_QUE_SIZE;
	int		MAX_EPOLL_SIZE;
	int		MAX_CMD_THREAD_NUM;

	int		CLIENT_RECV_TIMEOUT;
	int		CLIENT_SEND_TIMEOUT;
  
	int		MAX_LOG_FILE_SIZE;
	int		MAX_DEBUG_FILE_SIZE;
  int   DEBUG_LEVEL;

  int   REQS_PER_EVENT;

  int   SUPPORT_IPV6;
  int   MAX_CONNS;
};


#endif


