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
 * File        : DM_ENG_InformMessageScheduler.c
 *
 * Created     : 22/05/2008
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
 * @file DM_ENG_InformMessageScheduler.c
 *
 * @brief 
 *
 **/
 
#include "DM_ENG_InformMessageScheduler.h"
#include "DM_ENG_EventStruct.h"
#include "DM_ENG_Error.h"
#include "DM_ENG_NotificationInterface.h"
#include "DM_CMN_Thread.h"
#include "CMN_Trace.h"
#include <sys/time.h>

/**
 * DeviceStatus values
 */
typedef enum _DeviceStatus
{
   _UNINITIALIZED = 0, // état sortie usine (et après Factory Reset)
   _DOWN,
   _UP,
   _INITIALIZING,
   _ERROR,
   _DISABLED

} DeviceStatus;

static DeviceStatus lastStatus = _DOWN; // état initial par défaut

static DeviceStatus internStatus(char* status)
{
   return ( (status == NULL)                                       ? _ERROR
          : (strcmp(status, DM_ENG_DEVICE_STATUS_DOWN)==0)         ? _DOWN
          : (strcmp(status, DM_ENG_DEVICE_STATUS_UP)==0)           ? _UP
          : (strcmp(status, DM_ENG_DEVICE_STATUS_INITIALIZING)==0) ? _INITIALIZING
          : (strcmp(status, DM_ENG_DEVICE_STATUS_ERROR)==0)        ? _ERROR
          : (strcmp(status, DM_ENG_DEVICE_STATUS_DISABLED)==0)     ? _DISABLED
          : _DISABLED ); // tous les autres états (veille, redémarrage à chaud, ...) sont considérés "Disabled" 
}

static DM_CMN_ThreadId_t _schedulerId = 0;

static DM_CMN_Mutex_t _schedulerMutex = NULL;
static DM_CMN_Cond_t _schedulerCond = NULL;

static DM_CMN_Mutex_t _statusMutex = NULL;

static DM_ENG_NotificationStatus sendMessageStatus = DM_ENG_COMPLETED;
static unsigned int transferIdSending = 0;
static bool requestDownloadSending = false;

static bool alive = false;
static time_t nextInformTime = 0; // ==0 ssi pas de Inform Time Message planifié

static DM_ENG_EventStruct* pendingEvents = NULL;

static const int MAX_SESSION_TIME = 600; // en secondes (>= 30)
static const DM_CMN_Timespec_t SHORT_TIME = { 0, 200 };

static const long INITIAL_RETRY_DELAY = 5; // seconds
static const long MAX_RETRY_DELAY = 2560; // ou LONG_MAX/2
static bool retryOn = false;
static long retryDelay = 0;

void DM_ENG_InformMessageScheduler_init(DM_ENG_InitLevel level)
{
   DM_CMN_Thread_initMutex(&_schedulerMutex);
   DM_CMN_Thread_initCond(&_schedulerCond);
   DM_CMN_Thread_initMutex(&_statusMutex);
   if (level == DM_ENG_InitLevel_FACTORY_RESET) { lastStatus = _UNINITIALIZED; } // pour envoyer un BOOTSTRAP
   else if (level == DM_ENG_InitLevel_RESUME)   { lastStatus = _DISABLED; } // on considère qu'on était seulement désactivé (en veille...)
}

void DM_ENG_InformMessageScheduler_exit()
{
   DM_CMN_Thread_destroyMutex(_statusMutex);
   DM_CMN_Thread_destroyCond(_schedulerCond);
   DM_CMN_Thread_destroyMutex(_schedulerMutex);
}

static bool containsEvent(DM_ENG_EventStruct* list, DM_ENG_EventStruct* es)
{
   bool res = (es==NULL);
   while (!res && (list != NULL))
   {
      res = (list->eventCode == es->eventCode) // pour les événements vendor specific, comparer les chaines !!
         && (list->commandKey==NULL ? (es->commandKey==NULL)
             : ((es->commandKey!=NULL) && (strcmp(list->commandKey, es->commandKey)==0)));
      list = list->next;
   }
   return res;
}

static void addEvent(const char* event, char* commandKey)
{
   DM_ENG_EventStruct* es = DM_ENG_newEventStruct(event, commandKey);
   if (!containsEvent(pendingEvents, es)) { DM_ENG_addEventStruct(&pendingEvents, es); }
   else { DM_ENG_deleteEventStruct(es); }
   DM_CMN_Thread_signalCond(_schedulerCond);
}

static void checkNextInformTime()
{
   time_t newTime = DM_ENG_ParameterManager_getNextTimeout();
   if ((newTime != 0) && ((nextInformTime == 0) || (newTime <= nextInformTime)))
   {
      DM_CMN_Thread_signalCond(_schedulerCond);
   }
}

static void sendMessage()
{
   sendMessageStatus = DM_ENG_CANCELLED;
   if (!DM_ENG_NotificationInterface_isReady()) return;

   // Construction de la liste de couples (paramètre, valeur) à diffuser
   DM_ENG_ParameterValueStruct* pl = NULL;
   int sizePl = 0;
   bool containsChangedValue = false;
   DM_ENG_Parameter* param;
   for (param = DM_ENG_ParameterManager_getFirstParameter(); param!=NULL; param = DM_ENG_ParameterManager_getNextParameter())
   {
      bool valueChanged = DM_ENG_Parameter_isValueChanged(param) && (param->notification!=DM_ENG_NotificationMode_OFF);
      if ( param->mandatoryNotification || valueChanged )
      {
         char* resumeName = strdup(param->name);
         DM_ENG_ParameterManager_loadLeafParameterValue(param, false);
         DM_ENG_ParameterManager_resumeParameterLoopFrom(resumeName);
         DM_ENG_FREE(resumeName);
         param = DM_ENG_ParameterManager_getCurrentParameter();
         DM_ENG_addParameterValueStruct(&pl, DM_ENG_newParameterValueStruct(param->name, param->type, DM_ENG_Parameter_getFilteredValue(param)));
         sizePl++;
         containsChangedValue |= valueChanged;
      }
   }
   DM_ENG_ParameterManager_updateValueChangeEvent(containsChangedValue);

   if (DM_ENG_ParameterManager_isEventsEmpty()) // on annule la session
   {
      DM_ENG_deleteAllParameterValueStruct(&pl);
      return;
   }

   DM_CMN_Thread_lockMutex(_statusMutex);
   DM_ENG_ParameterManager_addMTransferEvents(); // ajout temporaire des M Download à partir des transferResults
   char* mn = DM_ENG_ParameterManager_getManufacturerName();
   char* oui = DM_ENG_ParameterManager_getManufacturerOUI();
   char* pc = DM_ENG_ParameterManager_getProductClass();
   char* sn = DM_ENG_ParameterManager_getSerialNumber();
   DM_ENG_DeviceIdStruct* id = DM_ENG_newDeviceIdStruct(mn, oui, pc, sn);
   if (mn != NULL) { free(mn); }
   if (oui != NULL) { free(oui); }
   if (pc != NULL) { free(pc); }
   if (sn != NULL) { free(sn); }

   DM_ENG_ParameterValueStruct** parameterList = DM_ENG_newTabParameterValueStruct(sizePl);
   parameterList[0] = pl;
   int i;
   for (i=1; i<sizePl; i++)
   {
      parameterList[i] = parameterList[i-1]->next;
      parameterList[i-1]->next = NULL;
   }

   DM_ENG_EventStruct** evt = DM_ENG_ParameterManager_getEventsTab();
   sendMessageStatus = DM_ENG_NotificationInterface_inform(id, evt, parameterList, time(NULL), DM_ENG_ParameterManager_getRetryCount());
   DM_ENG_deleteDeviceIdStruct(id);
   free(evt);
   DM_ENG_deleteTabParameterValueStruct(parameterList);
   DM_ENG_ParameterManager_deleteMTransferEvents();
   if (sendMessageStatus == DM_ENG_COMPLETED)
   {
      DM_ENG_ParameterManager_removeAllTransferResults(); // TransferComplete non émis dans ce cas
      DM_ENG_ParameterManager_removeAllDownloadRequests(); // DownloadRequest non émis dans ce cas
   }
   DM_CMN_Thread_unlockMutex(_statusMutex);
}

static void *run(void* arg UNUSED)
{
   struct timeval now;
   DM_CMN_Timespec_t timeout;
   bool inSession = false;
   DM_ENG_ParameterManager_lock();
   DBG("Inform message session - Begin");
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   alive = true;
   while (true)
   {
      // on déplace tous les événements en attente vers events
      DM_ENG_ParameterManager_manageEvents(pendingEvents);
      pendingEvents = NULL;

      if (!alive)
      {
         DM_CMN_Thread_unlockMutex(_schedulerMutex);
         DBG("Inform message session - End");
         DM_ENG_ParameterManager_unlock();
         DM_CMN_Thread_lockMutex(_schedulerMutex);
         break;
      }

      bool noEvent = DM_ENG_ParameterManager_isEventsEmpty();
      if (retryOn || noEvent || inSession) // on n'enchaîne pas 2 sessions sans délocker
      {
         inSession = false;
         DM_CMN_Thread_unlockMutex(_schedulerMutex);
         DBG("Inform message session - End");
         DM_ENG_ParameterManager_unlock();
         DM_CMN_Thread_lockMutex(_schedulerMutex);
         if ((noEvent || retryOn) && (pendingEvents == NULL)) // on a pu avoir un addEvent après le unlock()
         {
            time_t currentTime = time(NULL);
            time_t minTimeToWait = 0;
            nextInformTime = DM_ENG_ParameterManager_getNextTimeout();
            if (retryOn)
            {
               minTimeToWait = currentTime + INITIAL_RETRY_DELAY; // INITIAL_RETRY_DELAY utilisé comme délai minimal
               if (nextInformTime < minTimeToWait) { nextInformTime = minTimeToWait; }
               else if (nextInformTime > currentTime + retryDelay)
               {
                  nextInformTime = currentTime + retryDelay;
               }
               retryOn = false;
            }
            if (nextInformTime == 0)
            {
               DM_CMN_Thread_waitCond(_schedulerCond, _schedulerMutex);
            }
            else
            {
               while (nextInformTime > currentTime)
               {
                  timeout.sec = nextInformTime;
                  timeout.msec = 0;
                  DM_CMN_Thread_timedWaitCond(_schedulerCond, _schedulerMutex, &timeout);
                  nextInformTime = 0;
                  if (alive && (minTimeToWait > 0))
                  {
                     currentTime = time(NULL);
                     if (currentTime < minTimeToWait) // attente minimale suite à un échec
                     {
                        nextInformTime = minTimeToWait;
                     }
                  }
               }
            }
            if (!alive) break;
         }
         // petit temps de latence pour permettre le regroupement d'événements quasi-simultanés
         timeout = SHORT_TIME;                   // supprimer ce temps ? !!
         gettimeofday(&now, NULL);
         timeout.sec += now.tv_sec;
         timeout.msec += now.tv_usec/1000;
         if (timeout.msec >= 1000) { timeout.sec += 1; timeout.msec -= 1000; }
         DM_CMN_Thread_timedWaitCond(_schedulerCond, _schedulerMutex, &timeout);
         if (!alive) break;
         DM_CMN_Thread_unlockMutex(_schedulerMutex);
         DM_ENG_ParameterManager_lock();
         DBG("Inform message session - Begin");
         DM_CMN_Thread_lockMutex(_schedulerMutex);
      }
      else
      {
         inSession = true;
         sendMessage();
         while (alive && ((sendMessageStatus == DM_ENG_SESSION_OPENING) || (sendMessageStatus == DM_ENG_SESSION_OPENED)))
         {
            gettimeofday(&now, NULL);
            timeout.sec = now.tv_sec + MAX_SESSION_TIME;
            timeout.msec = now.tv_usec/1000;
            DM_CMN_Thread_timedWaitCond(_schedulerCond, _schedulerMutex, &timeout);
            DM_CMN_Thread_lockMutex(_statusMutex);
            if ((sendMessageStatus == DM_ENG_SESSION_OPENING) || (sendMessageStatus == DM_ENG_SESSION_OPENED))
            {
               gettimeofday(&now, NULL);
               if (now.tv_sec >= timeout.sec)
               {
                  sendMessageStatus = DM_ENG_CANCELLED;
                  transferIdSending = 0;
                  requestDownloadSending = false;
               }
            }
            if ((sendMessageStatus == DM_ENG_SESSION_OPENED) && (transferIdSending == 0) && !requestDownloadSending)
            {
               DM_ENG_TransferRequest* transferResultToSend = DM_ENG_ParameterManager_getTransferResult();
               if (transferResultToSend != NULL)
               {
                  transferIdSending = transferResultToSend->transferId;
                  DM_ENG_TransferCompleteStruct* transferCompleteToSend = DM_ENG_newTransferCompleteStruct(transferResultToSend->isDownload, transferResultToSend->commandKey, transferResultToSend->faultCode, transferResultToSend->startTime, transferResultToSend->completeTime);
                  DM_ENG_NotificationStatus status = DM_ENG_NotificationInterface_transferComplete(transferCompleteToSend);
                  DM_ENG_deleteTransferCompleteStruct(transferCompleteToSend);
                  if (status==0)
                  {
                     DM_ENG_ParameterManager_removeTransferResult(transferIdSending);
                  }
                  if (status!=1) // cas synchrone ou cas d'erreur, résultat non transmis
                  {
                     transferIdSending = 0;
                  }
                  // else : cas asynchrone
               }
               else
               {
                  DM_ENG_DownloadRequest* drToSend = DM_ENG_ParameterManager_getFirstDownloadRequest();
                  if (drToSend != NULL)
                  {
                     requestDownloadSending = true;
                     DM_ENG_NotificationStatus status = DM_ENG_NotificationInterface_requestDownload(drToSend->fileType, drToSend->args);
                     if (status==0)
                     {
                        DM_ENG_ParameterManager_removeFirstDownloadRequest();
                     }
                     if (status!=1) // cas synchrone ou cas d'erreur, résultat non transmis
                     {
                        requestDownloadSending = false;
                     }
                     // else : cas asynchrone
                  }
               }
            }
            DM_CMN_Thread_unlockMutex(_statusMutex);
         }
         if (sendMessageStatus == DM_ENG_COMPLETED)
         {
            retryDelay = 0;
            DM_ENG_ParameterManager_clearEvents();
         }
         else if (alive)
         {
            if (retryDelay == 0)
            {
               retryDelay = INITIAL_RETRY_DELAY;
            }
            else if (retryDelay < MAX_RETRY_DELAY)
            {
               retryDelay *= 2;
            }
            DM_ENG_ParameterManager_incRetryCount();
            retryOn = true;
         }
      }
   }
   alive = false;
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
   return 0;
}

/**
 * Appel provenant du module de comm indiquant que la session est close
 * @param success précise si la connexion avec l'ACS s'est déroulée (ou terminée) correctement
 * sinon la dernière information envoyée est considérée comme non transmise
 */
void DM_ENG_InformMessageScheduler_sessionClosed(bool success)
{
   if (success) { DM_ENG_InformMessageScheduler_isSessionOpened(success); } // pour effacer les informations transmises avec succès
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   DM_ENG_NotificationStatus prev = sendMessageStatus;
   sendMessageStatus = (success ? DM_ENG_COMPLETED : DM_ENG_CANCELLED);
   transferIdSending = 0;
   requestDownloadSending = false;
   if ((prev == DM_ENG_SESSION_OPENING) || (prev == DM_ENG_SESSION_OPENED)) { DM_CMN_Thread_signalCond(_schedulerCond); }
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Indique si une session est en cours
 * @param openingOK, en indiquant que le Inform Message a bien été transmis, permet de libérer
 * les infos du message
 */
bool DM_ENG_InformMessageScheduler_isSessionOpened(bool openingOK)
{
   DM_CMN_Thread_lockMutex(_statusMutex);
   if ((sendMessageStatus == DM_ENG_SESSION_OPENING) && openingOK)
   {
      DM_ENG_ParameterManager_clearEvents();
      DM_ENG_NotificationInterface_clearKnownParameters(DM_ENG_EntityType_ACS);
      sendMessageStatus = DM_ENG_SESSION_OPENED;
   }
   if ((sendMessageStatus == DM_ENG_SESSION_OPENED) && openingOK)
   {
      if (transferIdSending != 0)
      {
         DM_ENG_ParameterManager_removeTransferResult(transferIdSending);
         transferIdSending = 0;
      }
      else if (requestDownloadSending)
      {
         DM_ENG_ParameterManager_removeFirstDownloadRequest();
         requestDownloadSending = false;
      }
   }
   DM_CMN_Thread_unlockMutex(_statusMutex);
   return (sendMessageStatus == DM_ENG_SESSION_OPENED);
}

bool DM_ENG_InformMessageScheduler_isReadyToClose()
{
   DM_CMN_Thread_lockMutex(_statusMutex);
   bool isReady = (transferIdSending == 0) && !requestDownloadSending;
   if (isReady && ((DM_ENG_ParameterManager_getTransferResult() != NULL)
               || (DM_ENG_ParameterManager_getFirstDownloadRequest() != NULL)))
   {
      isReady = false;
      DM_CMN_Thread_signalCond(_schedulerCond);
   }
   DM_CMN_Thread_unlockMutex(_statusMutex);
   return isReady;
}

/**
 *
 * Doit être appelé sous mutex schedulerMutex
 */
static void _start()
{
   if (!alive)
   {
      // initialisations...
      DM_ENG_NotificationInterface_updateAcsParameters();

      retryOn = false;

      DM_CMN_Thread_create(run, NULL, true, &_schedulerId);
   }
}

static void _bootEvent(bool factoryReset)
{
   if (factoryReset)
   {
      addEvent(DM_ENG_EVENT_BOOTSTRAP, NULL);
   }
   addEvent(DM_ENG_EVENT_BOOT, NULL);
   char* commandKey = DM_ENG_ParameterManager_getRebootCommandKey();
   if (commandKey != NULL)
   {
      while (true)
      {
         char* virg = strchr(commandKey, ',');
         if (virg != NULL) { *virg = '\0'; }
         addEvent(DM_ENG_EVENT_M_REBOOT, commandKey);
         if (virg == NULL) break;
         commandKey = ++virg;
      }
      DM_ENG_ParameterManager_setRebootCommandKey(NULL);
   }
}

/**
 *
 * Doit être appelé sous mutex schedulerMutex
 */
static void _stop()
{
   if (alive)
   {
      alive = false;
      DM_CMN_Thread_signalCond(_schedulerCond); // pour arrêter le thread
   }
}

/**
 * Permet la synchronisation sur l'arrêt du thread scheduler
 */
void DM_ENG_InformMessageScheduler_join()
{
   DM_CMN_Thread_join(_schedulerId);
}

/**
 *
 */
void DM_ENG_InformMessageScheduler_bootstrapInform()
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent(DM_ENG_EVENT_BOOTSTRAP, NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 *
 */
void DM_ENG_InformMessageScheduler_changeOnPeriodicInform()
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   checkNextInformTime();
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * @param delay Delay in seconds from the time this method is called to the time the CPE
 * is requested to intiate a one-time Inform method call.
 * @param commandKey The string to return in the CommandKey element of the InformStruct
 * when the CPE calls the Inform method.
 */
int DM_ENG_InformMessageScheduler_scheduleInform(unsigned int delay, char* commandKey)
{
   if (delay <= 0) return DM_ENG_INVALID_ARGUMENTS;
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   
   DM_ENG_ParameterManager_addScheduledInformCommand(time(NULL)+delay, commandKey);
   
   checkNextInformTime();
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
   return 0;
}

/**
 * Entraîne l'envoi d'un CONNECTION REQUEST suite à une demande de l'ACS
 */
void DM_ENG_InformMessageScheduler_requestConnection()
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent(DM_ENG_EVENT_CONNECTION_REQUEST, NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Réception d'une notification de changement de valeur des paramètres à notification active
 * et, dans le cas du paramètre DeviceStatus, permet de gérer les activation/désactivation du device
 */
void DM_ENG_InformMessageScheduler_parameterValueChanged(DM_ENG_Parameter* param)
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   bool eventToAdd = true;
   if (strcmp(param->name, DM_ENG_DEVICE_STATUS_PARAMETER_NAME)==0)
   {
      DeviceStatus status = internStatus(param->value);
      if ((status == _INITIALIZING) || (status == _DOWN)) // 2 seuls états où l'agent est stoppé
      {
         _stop();
         eventToAdd = false;
      }
      else // sinon l'agent doit tourner
      {
         _start();
         if ((lastStatus == _UNINITIALIZED) || (lastStatus == _DOWN) || (lastStatus == _INITIALIZING))
         {
            _bootEvent(lastStatus == _UNINITIALIZED);
            eventToAdd = false;
         }
      }
      if (status == lastStatus)
      {
         eventToAdd = false;
      }
      lastStatus = status;
   }
   if (eventToAdd && (param->notification == DM_ENG_NotificationMode_ACTIVE))
   {
      addEvent(DM_ENG_EVENT_VALUE_CHANGE, NULL);
   }
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Permet d'envoyer un événement DIAGNOSTICS COMPLETE
 */
void DM_ENG_InformMessageScheduler_diagnosticsComplete()
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent(DM_ENG_EVENT_DIAGNOSTICS_COMPLETE, NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Permet d'envoyer un événement TRANSFER COMPLETE
 */
void DM_ENG_InformMessageScheduler_transferComplete(bool autonomous)
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent((autonomous ? DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE : DM_ENG_EVENT_TRANSFER_COMPLETE), NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Permet d'envoyer un événement REQUEST DOWNLOAD
 */
void DM_ENG_InformMessageScheduler_requestDownload()
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent(DM_ENG_EVENT_REQUEST_DOWNLOAD, NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}

/**
 * Permet d'envoyer un événement vendor specific
 */
void DM_ENG_InformMessageScheduler_vendorSpecificEvent(const char* eventCode)
{
   DM_CMN_Thread_lockMutex(_schedulerMutex);
   addEvent(eventCode, NULL);
   DM_CMN_Thread_unlockMutex(_schedulerMutex);
}
