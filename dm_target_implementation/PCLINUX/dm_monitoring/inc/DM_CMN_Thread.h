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
 * File        : DM_CMN_Thread.h
 *
 * Created     : 06/07/2009
 * Author      : Gilbert LANDAIS
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file DM_CMN_Thread.h
 *
 * @brief definition of thread and mutex generic function
 *
 **/

#ifndef _DM_CMN_THREAD_H_
#define _DM_CMN_THREAD_H_

#include "CMN_Type_Def.h"

typedef unsigned long int DM_CMN_ThreadId_t;
typedef void* DM_CMN_Mutex_t;
typedef void* DM_CMN_Cond_t;

typedef struct _DM_CMN_Timespec_t
{
   time_t sec;    // seconds
   unsigned int msec; // milliseconds

} __attribute((packed)) DM_CMN_Timespec_t;

/////////////////////////
///////  Threads  ///////
/////////////////////////

/**
 * Crée un nouveau thread
 * 
 * @param run Routine à exécuter
 * @param data Données fournies en argument de la routine
 * @param joinable Etat Joinable (joinable==true) : possibilité de se synchroniser sur la fin de ce thread
 *   ou Etat Detached (joinable==false) : libération des ressources dès la fin d'exécution.
 *   N.B. Ce booléen permet de garantir la bonne terminaison des threads. Dans un environnement comme Mosaic il n'est peut-être pas utile.
 * @param pId Pointeur sur un id de thread positionné par cette fonction si le thread a pu être créé
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_create(IN void*(*run)(void*), IN void* data, IN bool joinable, OUT DM_CMN_ThreadId_t* pId);

/**
 * Attend la fin d'exécution du thread identifié
 * 
 * @param id Identifiant de thread
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_join(IN DM_CMN_ThreadId_t id);

/**
 * "Tente" d'interrompre l'exécution d'un thread
 * Utilisation à préciser...
 * 
 * @param id Identifiant de thread
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_cancel(IN DM_CMN_ThreadId_t id);

/**
 * Causes the currently executing thread to sleep for the specified number of milliseconds.
 * 
 * @param millis - the length of time to sleep in milliseconds.
 */
void DM_CMN_Thread_msleep(unsigned int millis);

/**
 * Causes the currently executing thread to sleep for the specified number of seconds.
 * 
 * @param sec - the length of time to sleep in seconds.
 */
void DM_CMN_Thread_sleep(unsigned int sec);

///////////////////////
///////  Mutex  ///////
///////////////////////

/**
 * Creates and initializes a mutex.
 * 
 * @param pMutex Pointer on a mutex object created by this function
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_initMutex(OUT DM_CMN_Mutex_t* pMutex);

/**
 * Acquires the lock on the given mutex.
 * 
 * @param mutex A mutex object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_lockMutex(IN DM_CMN_Mutex_t mutex);

/**
 * Acquires the lock only if it is free at the time of invocation
 * 
 * @param mutex A mutex object
 * 
 * @return true si le mutex a pu être verrouillé, false sinon
 */
bool DM_CMN_Thread_trylockMutex(IN DM_CMN_Mutex_t mutex);

/**
 * Releases the lock.
 * 
 * @param mutex A mutex object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_unlockMutex(IN DM_CMN_Mutex_t mutex);

/**
 * Destroys the mutex.
 * 
 * @param mutex A mutex object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_destroyMutex(IN DM_CMN_Mutex_t mutex);


//////////////////////////
/////// Conditions ///////
//////////////////////////

/**
 * Creates and initializes a condition.
 * 
 * @param pCond Pointer on a condition object created by this function
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_initCond(OUT DM_CMN_Cond_t* pCond);

/**
 * Unlocks the mutex and wait for the condition.
 * After the condition is signaled, the mutex is locked again (as soon as possible) before resuming execution. 
 * 
 * @param cond A condition object
 * @param mutex A mutex object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_waitCond(IN DM_CMN_Cond_t cond, IN DM_CMN_Mutex_t mutex);

/**
 * As for DM_CMN_Thread_waitCond, but the waiting time is limited to millis milliseconds
 * 
 * @param cond A condition object
 * @param mutex A mutex object
 * @param pAbstime Deadline causing the end of the waiting.
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_timedWaitCond(IN DM_CMN_Cond_t cond, IN DM_CMN_Mutex_t mutex, IN const DM_CMN_Timespec_t* pAbstime);

/**
 * Wakes up the thread that is currently waiting on the condition.
 * It can resume the execution as soon as the associated mutex is free. 
 * 
 * @param cond A condition object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_signalCond(IN DM_CMN_Cond_t cond);

/**
 * Destroys the condition.
 * 
 * @param cond A cond object
 * 
 * @return 0 if OK, otherwise an error code
 */
int DM_CMN_Thread_destroyCond(IN DM_CMN_Cond_t cond);

#endif /*_DM_CMN_THREAD_H_*/
