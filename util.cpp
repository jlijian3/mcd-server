
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <iostream>

#include "util.h"

using namespace std;

void vperror(const char *fmt, ...) {
  int old_errno = errno;
  char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  if (vsnprintf(buf, sizeof(buf), fmt, ap) == -1) {
    buf[sizeof(buf) - 1] = '\0';
  }
  va_end(ap);

  errno = old_errno;

  perror(buf);
}

void PrintUsage(const char *srv_name, const char* version)
{
	cout << srv_name << " version: " << version;
  cout << "  date: "<< __DATE__ << " " << __TIME__ << "\n";
	cout << "Usage: " << srv_name;
  cout << " [-s | --setup filename] [-p | --print] [-d | --debug level] [-h | --help] [-f | --flush]\n";
	cout << " -h --help             # Display this usage information.\n";
	cout << " -s --setup filename   # setup file path.\n";
	cout << " -p --print            # Display log & debug info to screen.\n";
	cout << " -d --debug level      # output debug level info, level={0, 1, 2, ...}, 0=not debug info, default.\n";
	cout << " -f --flush            # flush log realtime to output." << endl;
  cout << " -i --signal           # send signal to process, -i [stop | reload].\n"; 
	exit(-1);
}

void GetOpt(int argc, char** argv,
            bool& isPrint,
            bool& isFlush,
            int& debugLevel,
            std::string& setupFilePath,
            std::string& signame,
            const char *srv_name,
            const char* version)
{
	const char * const short_options = "hpfd:s:i:";
	const struct option long_options[] = {
		{ "help", 0, NULL, 'h' },
		{ "print", 0, NULL, 'p' },
		{ "flush", 0, NULL, 'f' },
    { "debug", 1, NULL, 'd' },
		{ "setup", 1, NULL, 's' },
    { "signal", 1, NULL, 'i' },
    { NULL, 0, NULL, 0 } /* required at end of array. */
	};
	int next_option;

	do {
		next_option = getopt_long(argc, argv, short_options, long_options, NULL);
	    switch( next_option )
		{
		case 'h': /* -h or --help */
			PrintUsage(srv_name, version);
			break;

		case 'p': /* -p or --print */
			isPrint = true;
			break;

		case 'f': /* -f or --flush */
			isFlush = true;
			break;

		case 'd': /* -d or --debug */
			debugLevel = atoi(optarg);
			if( debugLevel < 0 || debugLevel > 5)
				PrintUsage(srv_name, version);
			break;

		case 's': /* -s or --setup */
			setupFilePath = optarg;
			break;

    case 'i': /* -i or --signal */
      signame = optarg;
      break;

		case '?': /* the user specified an invalid option. */
			PrintUsage(srv_name, version);
			break;

		case -1: /* done with options. */
			break;

		default: /* something else: unexpected. */
			break;
		}
	} while( next_option != -1 );

}


bool PIDSaveToFile(const char* pidFilePath)
{
	FILE *f = fopen(pidFilePath, "w");
	if( !f )	return false;
	fprintf(f, "%d\n", getpid());
	fclose(f);
	return true;
}

char *urlEncode(const char * asSrcUrl, int aiLen, int * aiNewLen)
{
  char			lcChar;
  const char		*lpSrcPtr=asSrcUrl;
  char			*lpNewUrl=NULL;
  char			*lpNewPtr=NULL;
  unsigned char	lsHexChars[] = "0123456789ABCDEF";

  lpNewPtr = lpNewUrl = (char *)malloc(aiLen * 3 + 1);

  while (lpSrcPtr-asSrcUrl < aiLen)
  {
    lcChar = *lpSrcPtr++;

    if (lcChar == ' ')
    {
      *lpNewPtr++ = '+';
    }
    else if ((lcChar < '0' && lcChar != '-' && lcChar != '.') ||
      (lcChar < 'A' && lcChar > '9') ||
      (lcChar > 'Z' && lcChar < 'a' && lcChar != '_') ||
      (lcChar > 'z')) 
    {
      lpNewPtr[0] = '%';
      lpNewPtr[1] = lsHexChars[(lcChar >> 4) & 0x0F];
      lpNewPtr[2] = lsHexChars[lcChar & 0x0F];

      lpNewPtr += 3;
    }
    else
    {
      *lpNewPtr++ = lcChar;
    }
  }

  *lpNewPtr = 0;
  if (aiNewLen)
  {
    *aiNewLen = lpNewPtr - lpNewUrl;
  }

  return lpNewUrl;
}
