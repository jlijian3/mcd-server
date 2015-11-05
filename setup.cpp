/*
	k/v???ò????????? by ?????? 2004.8.24
	
	update by ?????? 07.4.12
*/
#include "setup.h"
#include "util.h"

bool Setup::Load(const string setupPath)
{
	map<string, string>	keys;
	vector<string>	urls;
  
  _filename = setupPath;
	if( !LoadFromFile(setupPath.c_str(), keys, urls) )
		return false;

	GetAll(keys);
	return true;
}

void Setup::GetFileName(string &filename) const {
  filename = _filename;
}

bool Setup::LoadFromFile(const char *filePath, map<string, string>& keys, vector<string>& urls)
{
	FILE *f = fopen(filePath, "r");
	if( !f )
		return false;


	char line[512], *pkey, *pval;
	memset(line, 0, sizeof(line));

	while( fgets(line, sizeof(line)-1, f) )
	{
		pkey = Util::StrTrimRight(line);
    
		if( *pkey
			&& *pkey != '#'		//'#' is remark
			&& (pval = strchr(line, '=')) )
		{
			*pval++ = '\0';
			keys[pkey] = pval;
		}

		memset(line, 0, sizeof(line));
	}
	fclose(f);

	return true;
}

string& Setup::GetVal(map<string, string>& keys, const char *name)
{
	map<string, string>::iterator it = keys.find(name);
	return it == keys.end() ? _empty_string : it->second;
}

void Setup::GetString(map<string, string>& keys, const char *name,
					  string& dst, const char*& dstPtr)
{
	map<string, string>::iterator it = keys.find(name);
	if( it != keys.end() ) {
		dst = it->second;
		dstPtr = dst.c_str();
	}
	else
		dstPtr = dst.c_str();
}

int Setup::GetInt(map<string, string>& keys, const char *name, int defaultVal)
{
	map<string, string>::iterator it = keys.find(name);
	if( it == keys.end() )
		return defaultVal;
	else
		return atoi((it->second).c_str());
}

void Setup::GetAll(map<string, string>& keys)
{
	GetString(keys, "PidFile", S_PID_FILE_PATH, PID_FILE_PATH);
	GetString(keys, "LogFilePrefix", S_LOG_FILE_PREFIX, LOG_FILE_PREFIX);
	GetString(keys, "DebugFilePrefix", S_DEBUG_FILE_PREFIX, DEBUG_FILE_PREFIX);
	

	LISTEN_PORT = GetInt(keys, "ListenPort", 9901);
	LISTEN_QUE_SIZE = GetInt(keys, "ListenQueSize", 1024);
	MAX_EPOLL_SIZE = GetInt(keys, "MaxEpollSize", 100);
	MAX_CMD_THREAD_NUM = GetInt(keys, "MaxCmdThreadNum", 1);

	CLIENT_RECV_TIMEOUT = GetInt(keys, "ClientRecvTimeout", 15);
	CLIENT_SEND_TIMEOUT = GetInt(keys, "ClientSendTimeout", 15);

  REQS_PER_EVENT = GetInt(keys, "ReqsPerEvent", 50);

	MAX_LOG_FILE_SIZE = GetInt(keys, "MaxLogFileSize", 1024); //??λM??Ĭ??1G
	MAX_DEBUG_FILE_SIZE = GetInt(keys, "MaxDebugFileSize", 1024); //??λM??Ĭ??1G
  DEBUG_LEVEL = GetInt(keys, "DebugLevel", 0);

  SUPPORT_IPV6 = GetInt(keys, "SupportIPV6", 0);
  MAX_CONNS = GetInt(keys, "MaxConns", 1024);
}

