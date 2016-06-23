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
 * This version don't use DM_StatSimulator.c
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
#include <math.h>
#include <sys/time.h> /* gettimeofday */

/* ---------------------------------------- */

static char* _statObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPStats.";
static char* _statInstantObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPInstantStats.";
static char* _statEventObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPEventStats.";

#define _GMIN_DEFAULT 8

static const double _MAX_LOSS_RATE = 4.0 / 15; // = 4 / 3600 : 4 art�facts / heure

static int _conditions = 1;

// S�quencement des �tats
// Alternance entre �pisodes sans perturbation (�tat 0) et �pisodes avec perturbation (�tats 1 ou 2)
// _PROBA[1] et _PROBA[2] sp�cifient le % de chance de passer de l'�tat 0 �, respectivement, soit l'�tat 1 soit l'�tat 2  

// Settings

// dur�e m�diane en ms
static const unsigned int _DURATION[][3]   = {{ 7500, 30, 40 },      { 1500, 1, 2 },       { 1000, 3, 30 },        { 1000, 3, 30 },        { 1500, 12, 35 },      { 1200, 5, 30 },        { 5000, 5000, 30 }};
// proba d'apparition sur 100 => cf. S�quencement des �tats
static const unsigned int _PROBA[][3]      = {{ 0, 50, 50 },         { 0, 90, 10 },        { 0, 60, 40 },          { 0, 60, 40 },          { 0, 90, 10 },         { 0, 10, 90 },          { 0, 99, 1 }};
// taux de r�ception dans le contexte
static const double _RECEIV_RATE[][3]      = {{ 0.9998, 0.05, 0.07 },{ 0.9998, 0.1, 0.1 }, { 0.9998, 0.01, 0.01 }, { 0.9998, 0.01, 0.01 }, { 0.9998, 0.01, 0.1 }, { 0.9998, 0.01, 0.01 }, { 0.9998, 0.9998, 0.01 }};
// fct al�atoire appliqu�e � la dur�e
static const unsigned int _RANDOM_FCT[][3] = {{ 1, 1, 1 },           { 1, 1, 1 },          { 1, 0, 0 },            { 1, 1, 1 },            { 1, 1, 0 },           { 1, 1, 0 },            { 0, 0, 0 }};

static const unsigned int _PERIOD = 2; // delais entre paquets en ms
static const double _CORRECTION_RATE = 0.1;


static DM_CMN_Timespec_t _now = { 0, 0 };
static bool         _continuity = false;
static unsigned int _packetsReceivedBeforeEC = 0;
static unsigned int _packetsLostBeforeEC = 0;
static unsigned int _packetsReceived = 0;
static unsigned int _packetsLost = 0;

void DM_ComponentCalls_freeSessionData()
{
   // TO DO
}

char* DM_ComponentCalls_getComponentData(const char* name, const char* data)
{
   char* sVal = NULL;
   if ( (strcmp(name, "Services.STBService.1.ServiceMonitoring.MainStream.1.Total.RTPStats.QualityScore")==0)
     || (strcmp(name, "Services.STBService.1.ServiceMonitoring.MainStream.1.Total.RTPStats.QualityScoreBeforeEC")==0)
     || (strcmp(name, "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPEventStats.CurrentSample.QualityScore")==0)
     || (strcmp(name, "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPEventStats.CurrentSample.QualityScoreBeforeEC")==0) )
   {
      // data = <PacketsLostHistogram>,<SevereLostEvents>,<Duration>
      // Ex : 29:23:24:25:133:34,98,1476
      // On calcule weight = Somme des valeurs des intervalles, en comptant 1 max pour le 1er et 0 pour le dernier + SevereLostEvents
      // rate = weight / Duration   : nb d'�v�nements par seconde
      // score = ( rate < _MAX_LOSS_RATE ? 20 * (1 - rate / _MAX_LOSS_RATE) : O )
      long weight = 0;
      const char* str = data;
      int state = 0;
      long l = 0;
      while ((str != NULL) && (*str != '\0'))
      {
         char* end = NULL;
         l = 0;
         if ((*str == ':') || (*str == ',')) { str++; }
         l = strtol(str, &end, 10);
         if (state == 0)
         {
            if (l > 0) { weight++; } // 1er intervalle, on compte 1 max
            state = 1;
         }
         else if (state == 1)
         {
            if (*end == ':') { weight += l; } // on ne compte pas le dernier intervalle
            else { state = 2; } 
         }
         else if (state == 2)
         {
            weight += l; // les SevereLossEvents compte double
            state = 3;
         }
         else // state == 3
         {
            break; // derni�re valeur lue = temps de mesure
         }
         str = end;
      }
      if (l > 0)
      {
         double rate = (double)weight / l; // voir cas o� l == 0 (ou inf�rieur � un min)
         int score = 0;
         if (rate < _MAX_LOSS_RATE) { score = 20 * (1 - rate / _MAX_LOSS_RATE); }
         sVal = DM_ENG_intToString(score);
      }
      else
      {
         sVal = strdup(DM_ENG_EMPTY);
      }
   }
   return sVal;
}

bool DM_ComponentCalls_getComponentObjectData(const char* objName, OUT DM_ENG_ParameterValueStruct** pParamVal[])
{
   bool res = false;
   if (strcmp(objName, _statObject)==0)
   {
      char* sTimeStamp = DM_ENG_dateTimeToString(_now.sec);
      char* sPacketsReceivedBeforeEC = DM_ENG_intToString(_packetsReceivedBeforeEC);
      char* sPacketsLostBeforeEC = DM_ENG_intToString(_packetsLostBeforeEC);
      char* sPacketsReceived = DM_ENG_intToString(_packetsReceived);
      char* sPacketsLost = DM_ENG_intToString(_packetsLost);
      int nbParam = 5;
      *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      (*pParamVal)[0] = DM_ENG_newParameterValueStruct("LastTimeStamp", DM_ENG_ParameterType_UNDEFINED, sTimeStamp);
      (*pParamVal)[1] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
      (*pParamVal)[2] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
      (*pParamVal)[3] = DM_ENG_newParameterValueStruct("PacketsReceivedBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsReceivedBeforeEC);
      (*pParamVal)[4] = DM_ENG_newParameterValueStruct("PacketsLostBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLostBeforeEC);
      (*pParamVal)[5] = NULL;
      free(sTimeStamp);
      free(sPacketsReceivedBeforeEC);
      free(sPacketsLostBeforeEC);
      free(sPacketsReceived);
      free(sPacketsLost);
      res = true;
   }
   return res;
}

bool DM_ComponentCalls_setComponentData(const char* name UNUSED, const char* value)
{
   _conditions = value[0] - '0';
   printf("*** Nouvelles conditions de r�ception : %d\n", _conditions+1);
   return true;
}

// g�n�re un nombre compris entre 0 et limit-1, selon une distribution particuli�re
unsigned int _random1(unsigned int limit)
{
   double x = (double)rand() / RAND_MAX;
//   x = 28*log(x/2+1) / (30*x*x*x+1);
   x = x*x*x;
   return (unsigned int)(x * limit);
}

// g�n�re un nombre compris entre 0 et 3*median, en visant median comme m�diane
unsigned int _random2(unsigned int median)
{
   double x = (double)rand() / RAND_MAX;
   x = (12*x*x-16*x+7)*x;
   return (unsigned int)(x * median);
}

static int _gMin = _GMIN_DEFAULT;

static int _currState = 0;          // �tat courant : 0 = hors perturbation, 1 = en perturbation
static DM_CMN_Timespec_t _tChangeState = { 0, 0 }; // t du prochain changement d'�tat
static int _lossEventLengthBeforeEC = 0; // paquets perdus pendant l'�v�nement en cours, avant correction
static int _lossEventLength = 0; // ... apr�s correction
static int _packetsReceivedConsecutively = _GMIN_DEFAULT; // paquets re�us cons�cutivement. A voir !!

static bool _packetReceived()
{
   if ((_now.sec > _tChangeState.sec) || ((_now.sec == _tChangeState.sec)  && (_now.msec >= _tChangeState.msec)))
   {
      unsigned int p = rand() % 100;
      _currState = (_currState != 0 ? 0 : (p < _PROBA[_conditions][1] ? 1 : 2)); // d�termine le changement d'�tat => cf. S�quencement des �tats
      _tChangeState.sec = _now.sec;
      unsigned int d = _DURATION[_conditions][_currState];
      if (_RANDOM_FCT[_conditions][_currState] == 1) { d = _random2(d); }
      _tChangeState.msec = _now.msec + d;
      while (_tChangeState.msec >= 1000)
      {
         _tChangeState.sec++;
         _tChangeState.msec -= 1000;
      }
printf("*** %s perturbation %d ms (%d)\n", (_currState == 0 ? "Sans" : "AVEC"), d, _currState);
   }
   return (rand() < _RECEIV_RATE[_conditions][_currState] * RAND_MAX);
}

static DM_CMN_Mutex_t _receiverMutex = NULL;
static DM_CMN_Cond_t _receiverCond = NULL;
static bool _receiverRunning = false;

static void _notifyEvent()
{
   DM_ENG_ParameterValueStruct** parameterList = NULL;
   time_t timestamp = time(NULL);
   bool continued = 1-_random1(2);
   bool suspectData = false;
   char* sLossEventLengthBeforeEC = DM_ENG_intToString(_lossEventLengthBeforeEC);
   char* sLossEventLength = DM_ENG_intToString(_lossEventLength);
   int nbParam = 4;
   parameterList = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
   parameterList[0] = DM_ENG_newParameterValueStruct("LossEventLengthBeforeEC", DM_ENG_ParameterType_UNDEFINED, sLossEventLengthBeforeEC);
   parameterList[1] = DM_ENG_newParameterValueStruct("LossEventDurationBeforeEC", DM_ENG_ParameterType_UNDEFINED, sLossEventLengthBeforeEC);
   // On remonte LossEventLength m�me si == 0, auquel cas ce ne sera pas r�ellement un LossEvent
   parameterList[2] = DM_ENG_newParameterValueStruct("LossEventLength", DM_ENG_ParameterType_UNDEFINED, sLossEventLength);
   parameterList[3] = DM_ENG_newParameterValueStruct("LossEventDuration", DM_ENG_ParameterType_UNDEFINED, sLossEventLength);
   parameterList[4] = NULL;
   printf("LossEvent: lostBeforeEC=%s lost=%s continued=%d timestamp=%d\n", sLossEventLengthBeforeEC, sLossEventLength, continued, (int)timestamp);
   free(sLossEventLengthBeforeEC);
   free(sLossEventLength);

   DM_ENG_SampleDataStruct* sampleData = DM_ENG_newSampleDataStruct(_statEventObject, parameterList, timestamp, continued, suspectData);

   DM_ENG_deleteTabParameterValueStruct(parameterList);

   DM_ENG_ParameterManager_sampleData(sampleData); // N.B. Propri�t� de sampleData c�d�e
}

static void* _receiverThread(void* data UNUSED)
{
   DM_CMN_Thread_initMutex(&_receiverMutex);
   DM_CMN_Thread_initCond(&_receiverCond);

   DM_CMN_Timespec_t timeout;
   timeout.sec = time(NULL);
   timeout.msec = 0;
   struct timeval now;

   while (_receiverRunning)
   {
      DM_CMN_Thread_lockMutex(_receiverMutex);

      timeout.msec += _PERIOD;
      if (timeout.msec >= 1000)
      {
         timeout.sec++;
         timeout.msec -= 1000;
      }
      DM_CMN_Thread_timedWaitCond(_receiverCond, _receiverMutex, &timeout);

      DM_CMN_Thread_unlockMutex(_receiverMutex);

      gettimeofday(&now, NULL);
      _now.sec = now.tv_sec;
      _now.msec = now.tv_usec/1000;
//printf("_receiverThread %ld\n", _now.sec);

      if (_packetReceived())
      {
         _packetsReceivedBeforeEC++;
         _packetsReceived++;
         _packetsReceivedConsecutively++;
         if (_packetsReceivedConsecutively == _gMin)
         {
            _notifyEvent();
            _lossEventLengthBeforeEC = 0;
            _lossEventLength = 0;
         }
      }
      else
      {
         _packetsLostBeforeEC++; 
         _packetsReceivedConsecutively = 0;
         _lossEventLengthBeforeEC++;
         // correction
         double rate = _CORRECTION_RATE * (1.0+_packetsReceivedConsecutively/2.0); // Formule � voir !!
         if (rate > 1) { rate = 1; }
         if (rand() <= rate * RAND_MAX) // c'est corrig�
         {
            _packetsReceived++;
         }
         else // non corrig�
         {
            _packetsLost++;
            _lossEventLength++;
         }
      }
   }

   DM_CMN_Thread_destroyCond(_receiverCond);
   DM_CMN_Thread_destroyMutex(_receiverMutex);

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
   int gmin = 0;
   if ((args != NULL) && DM_ENG_stringToInt((char*)args, &gmin))
   {
      if (gmin > 0)
      {
         _gMin = gmin;
      }
   }

   // Dans cet exemple, activation de la notification d'�v�nements sur objet RTPStats (et non RTPEventStats)
   if ((objName != NULL) && (strcmp(objName, _statObject) == 0) && (_receiverRunning == false))
   {
      _receiverRunning = true;
      DM_CMN_ThreadId_t receiver_Thread_ID = 0;
      DM_CMN_Thread_create(_receiverThread, NULL, false, &receiver_Thread_ID);
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
   if (strcmp(objName, _statObject) == 0)
   {
      _receiverRunning = false;
   }

   printf(">>> STATISTICS OFF : %s\n", objName);

   return 0;
}

static bool _starting = true;

/**
 * 
 * @param timeStampBefore Data for this time stamp are already passed. They should not be provided again.
 * If no new data are available, no new DM_ENG_SampleDataStruct is created. A non zero code (1) is returned.
 *
 * Two Sampling objects (RTPStats and RTPInstantStats) can be used to perform different aggregations. 
 */
int DM_ENG_ComponentCalls_getSampleData(const char* objName, time_t timeStampBefore UNUSED, const char* data UNUSED, OUT DM_ENG_SampleDataStruct** pSampleData)
{
   //*pSampleData = DM_ENG_newSampleDataStruct(statObject, parameterList, timestamp, continued, suspectData);
   int res = 1; // objet non pris en charge
   if ((strcmp(objName, _statObject) == 0) || (strcmp(objName, _statInstantObject) == 0))
   {
      if (_now.sec <= timeStampBefore) // pas de nouvelles donn�es
      {
         res = 1;
      }
      else
      {
         DM_ENG_ParameterValueStruct** parameterList = NULL;
         _continuity =  1-_random1(2);
         bool continued = !_starting && _continuity;
         _starting = false;
         bool suspectData = false;
         char* sPacketsReceivedBeforeEC = DM_ENG_intToString(_packetsReceivedBeforeEC);
         char* sPacketsLostBeforeEC = DM_ENG_intToString(_packetsLostBeforeEC);
         char* sPacketsReceived = DM_ENG_intToString(_packetsReceived);
         char* sPacketsLost = DM_ENG_intToString(_packetsLost);
         int nbParam = 4;
         parameterList = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
         parameterList[0] = DM_ENG_newParameterValueStruct("PacketsReceived", DM_ENG_ParameterType_UNDEFINED, sPacketsReceived);
         parameterList[1] = DM_ENG_newParameterValueStruct("PacketsLost", DM_ENG_ParameterType_UNDEFINED, sPacketsLost);
         parameterList[2] = DM_ENG_newParameterValueStruct("PacketsReceivedBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsReceivedBeforeEC);
         parameterList[3] = DM_ENG_newParameterValueStruct("PacketsLostBeforeEC", DM_ENG_ParameterType_UNDEFINED, sPacketsLostBeforeEC);
         parameterList[4] = NULL;
         printf("Polling received=%s lost=%s continued=%d timestamp=%d\n", sPacketsReceived, sPacketsLost, continued, (int)_now.sec);
         free(sPacketsReceivedBeforeEC);
         free(sPacketsLostBeforeEC);
         free(sPacketsReceived);
         free(sPacketsLost);

         *pSampleData = DM_ENG_newSampleDataStruct(objName, parameterList, _now.sec, continued, suspectData);

         DM_ENG_deleteTabParameterValueStruct(parameterList);
         res = 0;
      }
   }

   return res;
}
