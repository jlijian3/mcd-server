

/*
	?߽??? by ?????? 2007
  modify by lijian2 2011-09-01
*/


#ifndef __PS_UTIL_INCLUDE__
#define __PS_UTIL_INCLUDE__

#include "type.h"

class Util
{
public:

	// (?̰߳?ȫ)ȡ??????
	static string Serr(int errnum)
	{
		char _err[256] = { 0 };
		char* pe = strerror_r(errnum, _err, sizeof(_err));
		return *_err ? _err : pe;
	}

	// ??beg-end??ʱ?????????Ϊ??λ?ĸ?????
	static float ExecTime(const struct timeval& beg, const struct timeval& end)
	{
		return ((float)(end.tv_sec - 1 - beg.tv_sec)) + ((float)(end.tv_usec + 1000000.0 - beg.tv_usec))/1000000.0;
	}

	// ??beg-end??ʱ?????????Ϊ??λ?ĸ????ִ?
	static string ExecTimeStr(const struct timeval& beg, const struct timeval& end)
	{
		return ToString(ExecTime(beg, end));
	}

	// ȡ???ڴ?
	static string GetDate()
	{
		time_t		nowtime;
		struct tm	res;
		char st[20];

		time(&nowtime);
		localtime_r(&nowtime, &res);
		sprintf(st, "%04d-%02d-%02d %02d:%02d:%02d",
			res.tm_year+1900,
			res.tm_mon+1,
			res.tm_mday,
			res.tm_hour,
			res.tm_min,
			res.tm_sec);
		return st;
	}

	static int GetFileSize(const string& filePath)
	{
		return GetFileSize(filePath.c_str());
	}

	//?ļ???С??ԭ??stat????-1??ʾ???���?????ô˺????????????жϷ????Ƿ?-1???ʳ??���?ļ?Ϊ0
	static int GetFileSize(const char* filePath)
	{
		struct stat buf;
		return stat(filePath, &buf) ? 0 : buf.st_size;
	}
	
	//?ļ??Ƿ?????
	static bool FileExisted(const string& filePath)
	{
		return FileExisted(filePath.c_str());
	}

	static bool FileExisted(const char* filePath)
	{
		return (access(filePath, F_OK) == 0);
	}

	// ???ļ?·????ȡ?ļ???????????'/'?ַ??滻??0???????ļ?ָ??
	static char* GetFileName(const char *filePath)
	{
		char *p;
		char *p1 = (char *)filePath;

		while((p=strchr(p1, '/')) != NULL)	p1 = ++p;
		return p1;
	}
	
	// ???ļ?·?????????ļ??�����?Ƶ?fileName?У??????ض?
	static char* GetFileName(const char *filePath, char *fileName, int fileNameSize)
	{
		const char *p;
		const char *p1 = filePath;
	
		while((p=strchr(p1, '/')) != NULL)	p1 = ++p;
		strncpy(fileName, p1, fileNameSize-1);
		*(fileName + fileNameSize - 1) = 0;
		return fileName;
	}
	
	// ???ļ?·????ȡ·????????'/'????һ?ַ??滻??0??????·??ָ??
	static char* GetPath(char *filePath)
	{
		char *p;
		char *p1 = (char *)filePath;

		while((p=strchr(p1, '/')) != NULL)	p1 = ++p;
		*p1 = 0;
		return filePath;
	}

	// ?????ļ?·??????·??????????һ??·????'/'
	static string GetPath(const string& filePathName)
	{
		return filePathName.substr(0, filePathName.find_last_of("/") + 1);
	}

	// ?????ļ?·???????ļ???
	static string GetFileName(const string& filePathName)
	{
		return filePathName.substr(filePathName.find_last_of("/") + 1);
	}
	
	// ?????ļ????ĺ?׺
	static string GetFileNameSuffix(const string& filePathName)
	{
		return filePathName.substr(filePathName.find_last_of(".") + 1);
	}

	// ?????ִ?ͷβ?Ŀո?
	static string& StringTrim(string& s)
	{
		if( s.empty() )	return s;
		s.erase(0, s.find_first_not_of(" \t\r\n"));
		s.erase(s.find_last_not_of(" \t\r\n") + 1);
		return s;
	}
	
	//	?????ִ?ͷβ?ķ??ַ�����?????ִ?ָ??
	static char* StrTrim(char *str)
	{
		char *p, *pt;
	
		p = str;
		while(*p && isspace(*p))	p++;
		pt = p;
	
		p += strlen(pt) - 1;
		while(isspace(*p))	p--;
		*(++p) = '\0';
	
		return pt;
	}

	//	?????ִ?β?ķ??ַ�����?????ִ?ָ??
	static char* StrTrimRight(char *str)
	{
		size_t len = strlen(str);
		char *ps = str + len - 1;
		while( len-- && (*ps == '\r' || *ps == '\n' || *ps == '\t' || *ps == ' ') )
			ps--;

		*(++ps) = '\0';
		return str;
	}

	//	??ӡ?Կո??ֿ????ִ?
	static void ShowBin(const char *buf, int len)
	{
		while( len-- ) cout<<*buf++<<" ";
		cout<<endl;
	}

	static long long toInt64(const string& s)
	{
#ifdef _WIN32
		return _atoi64(s.c_str());
#else
		return atoll(s.c_str());
#endif
	}

	static int toInt(const string& s)
	{
		return atoi(s.c_str());
	}

	static int toInt32(const string& s)
	{
		return toInt32(s.c_str());
	}

	static int toInt32(const char* c)
	{
		return atoi(c);
	}

	static string ToString(short val)
	{
		char buf[8];
		sprintf(buf, "%d", (int)val);
		return buf;
	}

	static string ToString(unsigned short val)
	{
		char buf[8];
		sprintf(buf, "%u", (unsigned int)val);
		return buf;
	}

	static string ToString(int val)
	{
		char buf[16];
		sprintf(buf, "%d", val);
		return buf;
	}

	static string ToString(unsigned int val)
	{
		char buf[16];
		sprintf(buf, "%u", val);
		return buf;
	}

	static string ToString(long val)
	{
		char buf[32];
		sprintf(buf, "%ld", val);
		return buf;
	}

	static string ToString(unsigned long val)
	{
		char buf[32];
		sprintf(buf, "%lu", val);
		return buf;
	}

	static string ToString(long long val)
	{
		char buf[32];
#ifdef _MSC_VER
		sprintf(buf, "%I64d", val);
#else
		sprintf(buf, "%lld", val);
#endif
		return buf;
	}

	static string ToString(unsigned long long val)
	{
		char buf[32];
#ifdef _MSC_VER
		sprintf(buf, "%I64u", val);
#else
		sprintf(buf, "%llu", val);
#endif
		return buf;
	}

	static string ToString(float val)
	{
		char buf[16];
		sprintf(buf, "%f", val);
		return buf;
	}

  static string ToString(double val)
	{
		char buf[32];
		sprintf(buf, "%f", val);
		return buf;
	}

  static char* H2A(const unsigned char* src, size_t srcLen, char* dst)
  {
    char *pd = dst;
    while( srcLen-- )
    {
       sprintf(pd, "%02x", *src++);
       pd += 2;
    }
    *pd = 0;
    return dst;
  }


};


char *urlEncode(const char * asSrcUrl, int aiLen, int * aiNewLen);
void vperror(const char *fmt, ...);
void GetOpt(int argc, char** argv,
            bool& isPrint,
            bool& isFlush,
            int& debugLevel,
            std::string& setupFilePath,
            std::string& signame,
            const char *srv_name,
            const char* version);
bool PIDSaveToFile(const char* pidFilePath);

#endif /*  __PS_UTIL_INCLUDE__ */
