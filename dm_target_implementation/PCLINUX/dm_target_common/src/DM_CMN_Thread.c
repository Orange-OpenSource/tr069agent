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
 * File        : DM_CMN_Thread.c
 *
 * Created     : 07/07/2009
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
 * @file DM_CMN_Thread.c
 *
 * @brief 
 *
 **/
 
#include <pthread.h>
#ifdef WIN32
# include <windows.h>
#endif

#include "DM_CMN_Thread.h"
#include "DM_ENG_Common.h"
#include "CMN_Trace.h"
#include <errno.h> /* for EBUSY */

#define DM_ENG_THREAD_STACK_SIZE (64*1024)
#define DM_HTTP_THREAD_STACK_SIZE (64*1024*2) // Require larger stack size due to ixml recursive call involving stack overflow

///////////////////////
/////// Threads ///////
///////////////////////

int DM_CMN_Thread_create(IN void*(*run)(void*), IN void* data, IN bool joinable, OUT DM_CMN_ThreadId_t* pId)
{
   int res = 1;
#ifndef WIN32
   pthread_t threadId = 0;
#else
   pthread_t threadId = { 0, 0 };
#endif

   pthread_attr_t threadAttr;
   pthread_attr_init(&threadAttr);
   pthread_attr_setdetachstate(&threadAttr, (joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));
   pthread_attr_setstacksize (&threadAttr, DM_ENG_THREAD_STACK_SIZE);

   res = pthread_create(&threadId, &threadAttr, run, data);
   if ( res == 0)
   {
#ifndef WIN32
      *pId = (DM_CMN_ThreadId_t)threadId;
#else
      *pId = (DM_CMN_ThreadId_t)threadId.p;
#endif
      //if (!joinable) { pthread_detach(threadId); }
   }
   else
   {
      *pId = 0;
      EXEC_ERROR( "Creating thread : NOK (ret = %d)", res );
   }

   pthread_attr_destroy(&threadAttr);

   return res;
}

int DM_CMN_Thread_join(IN DM_CMN_ThreadId_t id)
{
   int res =0;
   if (id != 0)
   {
#ifndef WIN32
      res = pthread_join((pthread_t)id, NULL);
#else
      pthread_t threadId = { 0, 0 };
      threadId.p = (void*)id;
      res = pthread_join(threadId, NULL);
#endif
   }
   return res;
}

int DM_CMN_Thread_cancel(IN DM_CMN_ThreadId_t id)
{
   int res =0;
   if (id != 0)
   {
#ifndef WIN32
      res = pthread_cancel((pthread_t)id);
#else
      pthread_t threadId = { 0, 0 };
      threadId.p = (void*)id;
      res = pthread_cancel(threadId);
#endif
   }
   return res;
}

void DM_CMN_Thread_sleep(unsigned int sec)
{
#ifndef WIN32
   sleep(sec);
#else
   Sleep(1000 * sec);
#endif
}

void DM_CMN_Thread_msleep(unsigned int millis)
{
#ifndef WIN32
   usleep(1000 * millis);
#else
   Sleep(millis);
#endif
}

///////////////////////
///////  Mutex  ///////
///////////////////////

int DM_CMN_Thread_initMutex(OUT DM_CMN_Mutex_t* pMutex)
{
   int res = 1;
   pthread_mutex_t* mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
   res = pthread_mutex_init(mutex, NULL);
   if (res == 0)
   {
      *pMutex = (DM_CMN_Mutex_t)mutex;
   }
   return res;
}

int DM_CMN_Thread_lockMutex(IN DM_CMN_Mutex_t mutex)
{
   return (mutex == NULL ? 1 : pthread_mutex_lock((pthread_mutex_t*)mutex));
}

bool DM_CMN_Thread_trylockMutex(IN DM_CMN_Mutex_t mutex)
{
   return (mutex == NULL ? false : (pthread_mutex_trylock((pthread_mutex_t*)mutex) != EBUSY));
}

int DM_CMN_Thread_unlockMutex(IN DM_CMN_Mutex_t mutex)
{
   return (mutex == NULL ? 1 : pthread_mutex_unlock((pthread_mutex_t*)mutex));
}

int DM_CMN_Thread_destroyMutex(IN DM_CMN_Mutex_t mutex)
{
   int res = 1;
   if (mutex != NULL)
   {
      res = pthread_mutex_destroy((pthread_mutex_t*)mutex);
      free(mutex);
   }
   return res;
}

//////////////////////////
/////// Conditions ///////
//////////////////////////

int DM_CMN_Thread_initCond(OUT DM_CMN_Cond_t* pCond)
{
   int res = 1;
   pthread_cond_t* cond = (pthread_cond_t*) malloc(sizeof(pthread_cond_t));
   res = pthread_cond_init(cond, NULL);
   if (res == 0)
   {
      *pCond = (DM_CMN_Cond_t)cond;
   }
   return res;
}

int DM_CMN_Thread_waitCond(IN DM_CMN_Cond_t cond, IN DM_CMN_Mutex_t mutex)
{
   return ((cond == NULL) || (mutex == NULL) ? 1 : pthread_cond_wait((pthread_cond_t*)cond, (pthread_mutex_t*)mutex));
}

int DM_CMN_Thread_timedWaitCond(IN DM_CMN_Cond_t cond, IN DM_CMN_Mutex_t mutex, IN const DM_CMN_Timespec_t* pAbstime)
{
   int res = 1;
   if ((cond != NULL) && (mutex != NULL) && (pAbstime != NULL))
   {
      struct timespec timeout = { 0, 0 };
      timeout.tv_sec = pAbstime->sec;
      timeout.tv_nsec = pAbstime->msec * 1000000;
      res = pthread_cond_timedwait((pthread_cond_t*)cond, (pthread_mutex_t*)mutex, &timeout);
   }
   return res;
}

int DM_CMN_Thread_signalCond(IN DM_CMN_Cond_t cond)
{
   return (cond == NULL ? 1 : pthread_cond_signal((pthread_cond_t*)cond));
}

int DM_CMN_Thread_destroyCond(IN DM_CMN_Cond_t cond)
{
   int res = 1;
   if (cond != NULL)
   {
      res = pthread_cond_destroy((pthread_cond_t*)cond);
      free(cond);
   }
   return res;
}
