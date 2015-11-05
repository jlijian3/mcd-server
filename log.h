#ifndef _CWQ_LOG_H
#define _CWQ_LOG_H

#ifdef MCD_DEBUG
#define dlog1(args...)		fprintf(stderr, args)
#else
#define dlog1(args...)
#endif

#define dlog2		          dlog1
#define dlog3             dlog1		
#define dlog4             dlog1	

#endif

