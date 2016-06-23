/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'. 
 *
 *---------------------------------------------------------------------------
 * File        : CMN_Trace.h
 *
 * Created     : 01/07/2009
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file CMN_Trace.h
 *
 * @brief Trace system 
 *
 **/


#ifndef _CMN_TRACE_H_
#define _CMN_TRACE_H_

#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include <unistd.h> // usleep

#define INFOSTR  "INFO - "
#define DBGSTR   "DBG - "
#define WARNSTR  "WARN - "
#define ERRORSTR "ERROR - "

#define MAX_TRACE_SIZE (4096)

// -------------------------------------------------------------
// Macro Definition to Map trace with trace
// -------------------------------------------------------------
/* Include trace header */
#ifndef WIN32
#include "traces.h"
#endif

#if FTX_TRACE_LEVEL > 0

#ifdef WIN32
#ifndef MAX_TRACE_SIZE
#define MAX_TRACE_SIZE 4096
#endif

char  tmpTraceStr[MAX_TRACE_SIZE];
static FILE * pFile = NULL;

static void openTrace() {
  pFile = fopen ("./CWMP.log","w");
}

static void closeTrace(){
  if(NULL != pFile) fclose (pFile);
}

static void addTrace(void * str)
{
  char tmpStr[MAX_TRACE_SIZE + 10];

  if(pFile == NULL) {
    pFile = fopen ("./CWMP.log","a+");
  }
  
  if (pFile != NULL) {
      sprintf(tmpStr, "%s %s", str, tmpTraceStr);
      fputs (tmpStr ,pFile);
      fflush(pFile);
  }
}

#define WIN_TRACE_INIT do{openTrace();}while(0)
#define WIN_TRACE_CLOSE do{closeTrace();}while(0)

#endif // WIN32
#else
#ifdef WIN32
#define WIN_TRACE_INIT do{}while(0)
#define WIN_TRACE_CLOSE do{}while(0)

#endif // WIN32
#endif // FTX_TRACE_LEVEL > 0



#ifndef DBG
  #if FTX_TRACE_LEVEL >= 7
#ifndef WIN32
    #define DBG( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
                            FTX_VERBOSE(__FTX_TRACES_FILTER_TR069_SHIFT, tmpTraceStr );\
                                     }    while(0)
#else
    #define DBG( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
			    addTrace("DBG - "); \
                                     }    while(0)
#endif			   
  #else
    #define DBG( ... )
  #endif
#endif

#ifndef INFO
  #if FTX_TRACE_LEVEL >= 6
#ifndef WIN32
    #define INFO( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
                            FTX_INFO(__FTX_TRACES_FILTER_TR069_SHIFT, tmpTraceStr );\
                                     }    while(0)
#else
    #define INFO( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
			    addTrace("INFO - "); \
                                     }    while(0)
#endif

  #else
    #define INFO( ... )
  #endif
#endif

#ifndef WARN
  #if FTX_TRACE_LEVEL >= 5
#ifndef WIN32
    #define WARN( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
                            FTX_WARNING(__FTX_TRACES_FILTER_TR069_SHIFT, tmpTraceStr );\
                            } while(0)
#else
    #define WARN( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
			    addTrace("WARN - "); \
                                     }    while(0)
#endif	

  #else
    #define WARN( ... )
  #endif
#endif

#ifndef EXEC_ERROR
  #if FTX_TRACE_LEVEL >= 4
#ifndef WIN32  
    #define EXEC_ERROR( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 1, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
                            FTX_ERROR(__FTX_TRACES_FILTER_TR069_SHIFT, tmpTraceStr );\
                            } while(0)
#else
    #define EXEC_ERROR( ... ) do{ snprintf(tmpTraceStr, MAX_TRACE_SIZE - 10, __VA_ARGS__);\
                            strcat(tmpTraceStr, "\n"); \
			    addTrace("ERROR - "); \
                                     }    while(0)
#endif				    
			    
  #else
    #define EXEC_ERROR( ... ) 
  #endif
#endif

#ifndef ASSERT
 #if FTX_TRACE_LEVEL >= 1
#ifndef WIN32
#define ASSERT( ... ) do{ FTX_ASSERT( __VA_ARGS__ );} while(0)
#else
  #define ASSERT( ... ) 
#endif

 #else
  #define ASSERT( ... ) 
 #endif
#endif

#endif /* _CMN_TRACE_H_ */
