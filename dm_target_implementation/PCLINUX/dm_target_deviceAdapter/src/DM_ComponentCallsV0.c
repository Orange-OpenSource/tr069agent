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
 * File        : DM_ComponentCalls.c
 *
 * Created     : 28/02/2012
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
 * @file DM_ComponentCalls
 *
 * @brief
 *
 * Example of implementation to simulate providing STB data.
 * To use with DM_StatSimulator.c which produce data in share memory 
 *
 **/

#include "DM_ComponentCalls.h"
#include "DM_ENG_Error.h"
#include "CMN_Trace.h"
#include "DM_GlobalDefs.h"
#include "DM_CMN_Thread.h"
#include "DM_ENG_ParameterBackInterface.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _UseSimu_
#include "DM_SharedData.h"
#include <sys/shm.h>
#endif

/* ---------------------------------------- */

static char* _statObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPStats.";
static char* _statEventObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPEventStats.";

#ifdef _UseSimu_

static DM_SharedData* _sharedData = NULL;
static DM_SharedData _copyOfData = { 0, 0, 0, 0, 0 };

static void _readSharedData();

#endif //_UseSimu_


void DM_ComponentCalls_freeSessionData()
{
   // TO DO
}

char* DM_ComponentCalls_getComponentData(const char* name UNUSED, const char* data UNUSED)
{
   char* sVal = NULL;
   return sVal;
}

bool DM_ComponentCalls_getComponentObjectData(const char* objName, OUT DM_ENG_ParameterValueStruct** pParamVal[])
{
   bool res = false;
   if (strcmp(objName, _statObject)==0)
   {
#ifdef _UseSimu_
      if (_sharedData != NULL)
      {
         _readSharedData();
         char* sTimeStamp = DM_ENG_dateTimeToString(_copyOfData.timestamp);
         char* sPacketsReceived = DM_ENG_intToString(_copyOfData.packetsReceived);
         char* sPacketsLost = DM_ENG_intToString(_copyOfData.packetsLost);
         int nbParam = 5;
         *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
         (*pParamVal)[0] = DM_ENG_newParameterValueStruct("LastTimeStamp", DM_ENG_ParameterType_UNDEFINED, sTimeStamp);
         (*pParamVal)[1] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
         (*pParamVal)[2] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
         (*pParamVal)[3] = DM_ENG_newParameterValueStruct("PacketsReceivedBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
         (*pParamVal)[4] = DM_ENG_newParameterValueStruct("PacketsLostBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
         (*pParamVal)[5] = NULL;
         free(sTimeStamp);
         free(sPacketsReceived);
         free(sPacketsLost);
         res = true;
      }
#else
      // résultats bidon
      char* sTimeStamp = "2011-09-27T14:59:53Z";
      char* sPacketsReceived = "5708";
      char* sPacketsLost = "1207";
      int nbParam = 3;
      *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      (*pParamVal)[0] = DM_ENG_newParameterValueStruct("LastTimeStamp", DM_ENG_ParameterType_UNDEFINED, sTimeStamp);
      (*pParamVal)[1] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
      (*pParamVal)[2] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
      (*pParamVal)[3] = NULL;
      res = true;
#endif //_UseSimu_
   }
   return res;
}


bool DM_ComponentCalls_setComponentData(const char* name UNUSED, const char* value UNUSED)
{
   return true;
}

#ifdef _UseSimu_

//
// Collecte de données statistiques écrites par un autre process en mémoire partagée  
//

static int _initSharedData()
{
   int shmid;
   key_t key;

   /*
    * We need to get the segment named
    * "5678", created by the server.
    */
   key = 5678;

   /*
    * Locate the segment.
    */
   if ((shmid = shmget(key, sizeof(DM_SharedData), 0666)) < 0) {
       perror("shmget");
       return 1;
   }

   /*
    * Now we attach the segment to our data space.
    */
   if ((_sharedData = shmat(shmid, NULL, 0)) == (void*)-1) {
       perror("shmat");
       return 1;
   }

   return 0;
}

static int _releaseSharedData()
{
   shmdt(_sharedData);
   _sharedData = NULL;
   return 0;
}

#endif //_UseSimu_

// génère un nombre compris entre 0 et limit-1, selon une distribution particulière
unsigned int _random1(unsigned int limit)
{
   double x = (double)rand() / RAND_MAX;
//   x = 28*log(x/2+1) / (30*x*x*x+1);
   x = x*x*x;
   return (unsigned int)(x * limit);
}

//
// Simulation de notification d'événements (LossEvents)  
//

static DM_CMN_Mutex_t _losseventsMutex = NULL;
static DM_CMN_Cond_t _losseventsCond = NULL;
static bool _losseventsRunning = false;

static void* _losseventsThread(void* data UNUSED)
{
   DM_CMN_Thread_initMutex(&_losseventsMutex);
   DM_CMN_Thread_initCond(&_losseventsCond);

   while (_losseventsRunning)
   {
      DM_CMN_Thread_lockMutex(_losseventsMutex);

      DM_CMN_Timespec_t timeout;
      timeout.sec = time(NULL) + _random1(20);
      timeout.msec = 0;
      DM_CMN_Thread_timedWaitCond(_losseventsCond, _losseventsMutex, &timeout);

      DM_CMN_Thread_unlockMutex(_losseventsMutex);

      DM_ENG_ParameterValueStruct** parameterList = NULL;
      time_t timestamp = time(NULL);
      bool continued = _random1(2);
      bool suspectData = false;
      unsigned int packetsLostBeforeEC = _random1(10)+1;
      unsigned int packetsLost = _random1(packetsLostBeforeEC+1);
      char* sPacketsLostBeforeEC = DM_ENG_intToString(packetsLostBeforeEC);
      char* sPacketsLost = DM_ENG_intToString(packetsLost);
      int nbParam = 4;
      parameterList = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      parameterList[0] = DM_ENG_newParameterValueStruct("LossEventLengthBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLostBeforeEC);
      parameterList[1] = DM_ENG_newParameterValueStruct("LossEventDurationBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLostBeforeEC);
      // On remonte LossEventLength même si == 0, auquel cas ce ne sera pas réellement un LossEvent
      parameterList[2] = DM_ENG_newParameterValueStruct("LossEventLength", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
      parameterList[3] = DM_ENG_newParameterValueStruct("LossEventDuration", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
      parameterList[4] = NULL;
      printf("LossEvent: lostBeforeEC=%s lost=%s continued=%d timestamp=%d\n", sPacketsLostBeforeEC, sPacketsLost, continued, (int)timestamp);
      free(sPacketsLostBeforeEC);
      free(sPacketsLost);

      DM_ENG_SampleDataStruct* sampleData = DM_ENG_newSampleDataStruct(_statEventObject, parameterList, timestamp, continued, suspectData);

      DM_ENG_deleteTabParameterValueStruct(parameterList);

      DM_ENG_ParameterManager_sampleData(sampleData); // N.B. Propriété de sampleData cédée
   }

   DM_CMN_Thread_destroyCond(_losseventsCond);
   DM_CMN_Thread_destroyMutex(_losseventsMutex);

   return 0;
}

/**
 * Prepares the collection of statistics identified by the objName. As appropriate, the data will be collected in polling mode (periodic calls
 * to the DM_ENG_Device_getSampleData() function by the DM Engine) or or by events notified to the DM Engine (callback DM_ENG_ParameterManager_sampleData()). 
 * 
 * @param objName Name of the data model object dedicated to a stats group
 * @param timeRef Time reference used the stats sampling
 * @param sampleInterval Time interval between consecutive sampling
 * @param args Args string that can be used for the specific stats group
 * 
 * @return Error code : 0 if OK, 1 if the component is not ready to start collection, 2 or others for various types of errors
 * 
 * N.B. This function may be called with objName == NULL. In this case, the return value indicates if the device is ready to start statistics.  
 */
int DM_ENG_ComponentCalls_startSampling(const char* objName, time_t timeRef UNUSED, unsigned int sampleInterval UNUSED, const char* args)
{
#ifdef _UseSimu_
   if ((_sharedData == NULL) && (_initSharedData() != 0)) return 2; // échec de l'initialisation de la mémoire partagée
#endif //_UseSimu_

   // Dans cet exemple, activation de la notification d'événements sur objet RTPStats (et non RTPEventStats)
   if ((objName != NULL) && (strcmp(objName, _statObject) == 0) && (_losseventsRunning == false))
   {
      _losseventsRunning = true;
      DM_CMN_ThreadId_t lossevents_Thread_ID = 0;
      DM_CMN_Thread_create(_losseventsThread, NULL, false, &lossevents_Thread_ID);
   }

   printf(">>> STATISTICS ON : %s args=%s\n", objName, args);

   return 0;
}

/**
 * Stop the collection of statistics. 
 * Polling mode : polling function should no longer called.
 * Event mode : no more calls to the callback function.
 */
int DM_ENG_ComponentCalls_stopSampling(const char* objName)
{
#ifdef _UseSimu_
   _releaseSharedData();
#endif //_UseSimu_

   if (strcmp(objName, _statObject) == 0)
   {
      _losseventsRunning = false;
   }

   printf(">>> STATISTICS OFF : %s\n", objName);

   return 0;
}

#ifdef _UseSimu_
static void _readSharedData()
{
   // pose d'un verrou, avec attente si nécessaire
   LIGHT_LOCK_2(_sharedData->flag);

   // accès à la mémoire partagée
   _copyOfData = *_sharedData;
   _sharedData->continuity = 1;

   // suppression du verrou
   LIGHT_UNLOCK_2(_sharedData->flag);
}
#endif //_UseSimu_

static bool _starting = true;

/**
 * 
 * @param timeStampBefore Data for this time stamp are already passed. They should not be provided again.
 * If no new data are available, no new DM_ENG_SampleDataStruct is created. A non zero code (1) is returned.
 */
int DM_ENG_ComponentCalls_getSampleData(const char* objName, time_t timeStampBefore UNUSED, const char* data UNUSED, OUT DM_ENG_SampleDataStruct** pSampleData)
{
   //*pSampleData = DM_ENG_newSampleDataStruct(statObject, parameterList, timestamp, continued, suspectData);
   int res = 1; // objet non pris en charge
   if (strcmp(objName, _statObject) == 0)
   {
#ifdef _UseSimu_
      if (_sharedData == NULL)
      {
         res = 2;
      }
      else
      {
         _readSharedData();
         if (_copyOfData.timestamp <= timeStampBefore) // pas de nouvelles données
         {
            res = 1;
         }
         else
         {
            DM_ENG_ParameterValueStruct** parameterList = NULL;
            bool continued = !_starting && (_copyOfData.continuity == 1);
            _starting = false;
            bool suspectData = false;
            char* sPacketsReceived = DM_ENG_intToString(_copyOfData.packetsReceived);
            char* sPacketsLost = DM_ENG_intToString(_copyOfData.packetsLost);
            int nbParam = 4;
            parameterList = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
            parameterList[0] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
            parameterList[1] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
            parameterList[2] = DM_ENG_newParameterValueStruct("PacketsReceivedBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
            parameterList[3] = DM_ENG_newParameterValueStruct("PacketsLostBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
            parameterList[4] = NULL;
            printf("Polling received=%s lost=%s continued=%d timestamp=%d\n", sPacketsReceived, sPacketsLost, continued, (int)_copyOfData.timestamp);
            free(sPacketsReceived);
            free(sPacketsLost);

            *pSampleData = DM_ENG_newSampleDataStruct(_statObject, parameterList, _copyOfData.timestamp, continued, suspectData);

            DM_ENG_deleteTabParameterValueStruct(parameterList);
            res = 0;
         }
      }
#else
      // résultats bidon
      DM_ENG_ParameterValueStruct** parameterList = NULL;
      bool continued = !_starting;
      _starting = false;
      bool suspectData = false;
      char* sPacketsReceived = "100";
      char* sPacketsLost = "10";
      time_t timestamp = time(NULL);
      int nbParam = 2;
      parameterList = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      parameterList[0] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
      parameterList[1] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
//      parameterList[1] = DM_ENG_newParameterValueStruct("PacketsLostBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
      parameterList[2] = NULL;
      printf("received=%s lost=%s continued=%d timestamp=%d\n", sPacketsReceived, sPacketsLost, continued, (int)timestamp);

      *pSampleData = DM_ENG_newSampleDataStruct(_statObject, parameterList, timestamp, continued, suspectData);

      DM_ENG_deleteTabParameterValueStruct(parameterList);
      res = 0;
#endif //_UseSimu_
   }

   return res;
}
