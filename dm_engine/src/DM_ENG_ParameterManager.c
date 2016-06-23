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
 * File        : DM_ENG_ParameterManager.c
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
 * @file DM_ENG_ParameterManager.c
 *
 * @brief
 *
 **/


#include "DM_ENG_ParameterManager.h"
#include "DM_ENG_ParameterData.h"
#include "DM_ENG_ParameterBackInterface.h"
#include "DM_ENG_Device.h"
#include "DM_ENG_Error.h"
#include "DM_ENG_ParameterStatus.h"
#include "DM_ENG_SystemNotificationStruct.h"
#include "DM_ENG_DiagnosticsLauncher.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_InformMessageScheduler.h"
#include "DM_ENG_NotificationInterface.h"
#include "DM_ENG_StatisticsModule.h"
#include "DM_ENG_ComputedExp.h"
#include "DM_CMN_Thread.h"
#include "CMN_Trace.h"
#include "DM_ENG_DataModelConfiguration.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static const int _MAX_SYSTEM_ENTRY = 500;

static DM_CMN_Mutex_t _dataMutex = NULL;
static DM_CMN_Mutex_t _changesMutex = NULL;

static bool periodicInformChanged = false;
static bool acsParametersToUpdate = false;

static DM_ENG_ParameterValueStruct* diagnosticsRequested = NULL;

// File d'attente des notifications système
static DM_ENG_SystemNotificationStruct* pendingChanges = NULL;

static char* _getValue(char* prmName, const char* destName);
static bool _createParameter(char* newName, char* destName, DM_ENG_ParameterType type, char* args, char* configKey);
static void _computeNextPeriodicInformTime();
static int _loadComputedParameter(DM_ENG_Parameter* param, bool checkChange);

/**
 * Initializes the DM Engine.
 * 
 * @param dataPath Path to find the necessary data in working of the engine
 * @param level Indicates if the data must be resetted, if the BOOT event must be sent etc
 *
 * @return Returns 0 if OK or a fault code, 1 : missing data or file, 2 : I/O error, ..., 5 : ACS URL undefined, ...
 */
int DM_ENG_ParameterManager_init(const char* dataPath, DM_ENG_InitLevel level)
{
   DBG("DM_ENG_ParameterManager_init Begin - dataPath = %s, init level = %d", dataPath, level);
   if (level != DM_ENG_InitLevel_FACTORY_RESET)
   {
      int res = DM_ENG_ParameterData_init(dataPath, false);
      if (res == 0)
      {
         DM_ENG_Parameter* param;
         for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
         {
            // remise à zéro pour forcer réinitialisation système au démarrage
            if ((param->storageMode != DM_ENG_StorageMode_DM_ONLY) && (param->value != NULL))
            {
               free(param->value);
               param->value = NULL;
            }
         }
      }
      else
      {
         if ((res == 1) || (res == 2)) { level = DM_ENG_InitLevel_FACTORY_RESET; }
         else return res;
      }
   }
   if (level == DM_ENG_InitLevel_FACTORY_RESET)
   {
      int res = DM_ENG_ParameterData_init(dataPath, true);
      if (res != 0) return res;
   }
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(DM_TR106_ACS_URL);
   if ((param == NULL) || (param->value == NULL) || (strlen(param->value)==0))
   {
      EXEC_ERROR("*** ACS URL UNDEFINED ***");
      return 5;
   }
   DM_CMN_Thread_initMutex(&_dataMutex);
   DM_CMN_Thread_initMutex(&_changesMutex);

   DM_ENG_ParameterManager_lockDM();
   DM_ENG_InformMessageScheduler_init(level);
   if (level == DM_ENG_InitLevel_FACTORY_RESET)
   {
      DM_ENG_deleteAllSystemNotificationStruct(&pendingChanges); // au cas où il resterait des notif en attente
   }
   if (DM_ENG_ParameterData_getPeriodicInformTime() == 0)
   {
      _computeNextPeriodicInformTime();
   }
   DM_ENG_StatisticsModule_init(_createParameter, _getValue);
   DM_ENG_ParameterData_unLock();
   DBG("DM_ENG_ParameterManager_init - Done");
   return 0;
}

void DM_ENG_ParameterManager_release()
{
   DM_ENG_ParameterManager_lockDM();
   DM_ENG_StatisticsModule_exit();
   DM_ENG_InformMessageScheduler_exit();
   DM_CMN_Thread_destroyMutex(_changesMutex); _changesMutex = NULL;
   DM_CMN_Thread_destroyMutex(_dataMutex); _dataMutex = NULL;
   DM_ENG_ParameterData_release();
   DM_ENG_ParameterData_unLock();
   DM_ENG_StatisticsModule_join();
}

int DM_ENG_ParameterManager_getNumberOfParameters()
{
   return DM_ENG_ParameterData_getNumberOfParameters();
}

DM_ENG_Parameter* DM_ENG_ParameterManager_getFirstParameter()
{
   return DM_ENG_ParameterData_getFirst();
}

DM_ENG_Parameter* DM_ENG_ParameterManager_getNextParameter()
{
   return DM_ENG_ParameterData_getNext();
}

char* DM_ENG_ParameterManager_getFirstParameterName()
{
   return DM_ENG_ParameterData_getFirstName();
}

char* DM_ENG_ParameterManager_getNextParameterName()
{
   return DM_ENG_ParameterData_getNextName();
}

void DM_ENG_ParameterManager_resumeParameterLoopFrom(char* resumeName)
{
   DM_ENG_ParameterData_resumeParameterLoopFrom(resumeName);
}

DM_ENG_Parameter* DM_ENG_ParameterManager_getCurrentParameter()
{
   return DM_ENG_ParameterData_getCurrent();
}

/////////////////////////////////////////////////
////////////// Gestion des events ///////////////

static void _computeNextPeriodicInformTime()
{
   time_t periodicInformTime = DM_ENG_ParameterData_getPeriodicInformTime();
   time_t period = DM_ENG_ParameterManager_getPeriodicInformInterval();
   if (DM_ENG_ParameterManager_isPeriodicInformEnabled() && (period>0))
   {
      time_t now = time(NULL);
      time_t refTime = DM_ENG_ParameterManager_getPeriodicInformTime();
      if (refTime == 0) { refTime = periodicInformTime; }
      periodicInformTime = now + period;
      if ((refTime>0) && (refTime != now))
      {
         if (refTime < now) { periodicInformTime -= (now-refTime)%period; }
         else { periodicInformTime = now + (refTime-now)%period; }
      }
   }
   else
   {
      periodicInformTime = 0;
   }
   DM_ENG_ParameterData_setPeriodicInformTime(periodicInformTime);
}

static int _actionAfterDownload(const char* fileType, const char* fileName)
{
   int res = 0;
   if ((fileType != NULL) && (strcmp(fileType, DATA_MODEL_EXTENSION_FILE_TYPE) == 0))
   {
      res = DM_ENG_DataModelConfiguration_processConfig(fileName);
      if (res == 0) // on redémarre le module de stats
      {
         DM_ENG_StatisticsModule_restart();
      }
   }
   return res;
}

static int _applyTransferRequest(DM_ENG_TransferRequest* tr)
{
   int res = 0;
   if (tr->isDownload)
   {
      res = DM_ENG_Device_download(tr->fileType, tr->url, tr->username, tr->password, tr->fileSize,
              tr->targetFileName, tr->successURL, tr->failureURL, tr->transferId);
      if (res == 0)
      {
         res = _actionAfterDownload(tr->fileType, tr->targetFileName);
      }
   }
   else
   {
      res = DM_ENG_Device_upload(tr->fileType, tr->url, tr->username, tr->password, tr->transferId);
   }
   return res;
}

/*
 * on déplace tous les événements en attente vers events
 */
void DM_ENG_ParameterManager_manageEvents(DM_ENG_EventStruct* pendingEvt)
{
   DM_ENG_EventStruct* evt = pendingEvt;
   while (evt != NULL)
   {
      DM_ENG_ParameterData_addEvent(evt->eventCode, evt->commandKey);
      evt = evt->next;
   }
   DM_ENG_deleteAllEventStruct(&pendingEvt);
   time_t periodicInformTime = DM_ENG_ParameterData_getPeriodicInformTime();
   time_t now = time(NULL);
   if ((periodicInformTime != 0) && (periodicInformTime <= now))
   {
      if (!DM_ENG_ParameterData_isInEvents(DM_ENG_EVENT_PERIODIC)) // EVENT_PERIODIC n'est pas présent, on le rajoute
      {
         DM_ENG_ParameterData_addEvent(DM_ENG_EVENT_PERIODIC, NULL);
      }
      _computeNextPeriodicInformTime();
   }
   time_t scheduledInformTime = DM_ENG_ParameterData_getScheduledInformTime();
   while ((scheduledInformTime != 0) && (scheduledInformTime <= now))
   {
      if (!DM_ENG_ParameterData_isInEvents(DM_ENG_EVENT_SCHEDULED)) { DM_ENG_ParameterData_addEvent(DM_ENG_EVENT_SCHEDULED, NULL); }
      char* ck = DM_ENG_ParameterData_getScheduledInformCommandKey();
      DM_ENG_ParameterData_addEvent(DM_ENG_EVENT_M_SCHEDULE_INFORM, ck);
      DM_ENG_FREE(ck);
      DM_ENG_ParameterData_nextScheduledInformCommand();
      scheduledInformTime = DM_ENG_ParameterData_getScheduledInformTime();
   }
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   while (tr != NULL)
   {
      if ((tr->state == _NOT_YET_STARTED) && (tr->transferTime <= time(NULL)))
      {
         time_t start = time(NULL);
         int res = _applyTransferRequest(tr);
         time_t complete = (res==0 ? time(NULL) : 0);
         if (res==1) // download/upload asynchrone
         {
            DM_ENG_ParameterData_updateTransferRequest(tr->transferId, _IN_PROGRESS, 0, 0, 0);
         }
         else // res==0 ou code d'erreur 90XX
         {
            DM_ENG_ParameterData_updateTransferRequest(tr->transferId, _COMPLETED, res, start, complete);
         }
      }
      if (tr->state == _COMPLETED)
      {
         const char* transferEvent = (tr->commandKey == NULL ? DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE : DM_ENG_EVENT_TRANSFER_COMPLETE);
         if (!DM_ENG_ParameterData_isInEvents(transferEvent))
         {
            DM_ENG_ParameterData_addEvent(transferEvent, NULL);
         }
      }
      tr = tr->next;
   }
   if ((DM_ENG_ParameterData_getDownloadRequests() != NULL) && !DM_ENG_ParameterData_isInEvents(DM_ENG_EVENT_REQUEST_DOWNLOAD))
   {
      DM_ENG_ParameterData_addEvent(DM_ENG_EVENT_REQUEST_DOWNLOAD, NULL);
   }
   DM_ENG_ParameterData_sync();
}

/*
 * Met à jour la liste des événements en rajoutant ou supprimant au besoin le VALUE_CHANGE_EVENT
 * @param valueChanged Indique si VALUE_CHANGE_EVENT doit être présent
 */
void DM_ENG_ParameterManager_updateValueChangeEvent(bool valueChanged)
{
   if (DM_ENG_ParameterData_isInEvents(DM_ENG_EVENT_VALUE_CHANGE))
   {
      if (!valueChanged)
      // La notification de ce changement a déjà été faite ds un autre Inform Message (Inform Message périodique)
      {
         DM_ENG_ParameterData_deleteEvent(DM_ENG_EVENT_VALUE_CHANGE);
      }
   }
   else if (valueChanged)
      // au moins une valeur changée mais events ne contient pas VALUE_CHANGE_EVENT => on le rajoute
   {
      DM_ENG_ParameterData_addEvent(DM_ENG_EVENT_VALUE_CHANGE, NULL);
   }
}

/*
 * Complète la liste des événements avec les "M Download" correspondants aux résultats de transfert
 */
void DM_ENG_ParameterManager_addMTransferEvents()
{
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   while (tr != NULL)
   {
      if ((tr->state == _COMPLETED) && (tr->commandKey != NULL))
      {
         DM_ENG_ParameterData_addEvent((tr->isDownload ? DM_ENG_EVENT_M_DOWNLOAD : DM_ENG_EVENT_M_UPLOAD), tr->commandKey);
      }
      tr = tr->next;
   }
}

/*
 * Supprime tous les événements "M Download" de la liste events
 */
void DM_ENG_ParameterManager_deleteMTransferEvents()
{
   DM_ENG_ParameterData_deleteEvent(DM_ENG_EVENT_M_DOWNLOAD);
   DM_ENG_ParameterData_deleteEvent(DM_ENG_EVENT_M_UPLOAD);
}

/*
 * Teste si la liste des événements est vide
 */
bool DM_ENG_ParameterManager_isEventsEmpty()
{
   return (DM_ENG_ParameterData_getEvents() == NULL);
}

/*
 * Fournit un EventStruct*[] nouvellement créé.
 * L'appelant a la charge d'effectuer un free du tableau après utilisation.
 */
DM_ENG_EventStruct** DM_ENG_ParameterManager_getEventsTab()
{
   int nbEvents = 0;
   DM_ENG_EventStruct* events = DM_ENG_ParameterData_getEvents();
   DM_ENG_EventStruct* itEvt = events;
   while (itEvt != NULL) { nbEvents++; itEvt = itEvt->next; }
   DM_ENG_EventStruct** evt = (DM_ENG_EventStruct**)calloc(nbEvents+1, sizeof(DM_ENG_EventStruct*));
   evt[0] = events;
   int i;
   for (i=1; i<nbEvents; i++)
   {
      evt[i] = evt[i-1]->next;
   }
   return evt;
}

/*
 * Vide les events (appelé une fois que la transmission à l'ACS a été confirmé)
 */
void DM_ENG_ParameterManager_clearEvents()
{
   if (DM_ENG_ParameterData_getEvents() != NULL)
   {
      DM_ENG_ParameterData_deleteAllEvents();

      // on supprime également les flags associés à l'événement VALUE CHANGE
      DM_ENG_Parameter* param;
      for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
      {
         DM_ENG_Parameter_initState(param);
      }

      DM_ENG_ParameterData_resetRetryCount();
      DM_ENG_ParameterData_sync();
   }
}

DM_ENG_TransferRequest* DM_ENG_ParameterManager_getTransferResult()
{
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   while ((tr != NULL) && (tr->state != _COMPLETED)) { tr = tr->next; }
   return tr;
}

void DM_ENG_ParameterManager_removeTransferResult(unsigned int transferId)
{
   DM_ENG_ParameterData_removeTransferRequest(transferId);
   DM_ENG_ParameterData_sync();
}

void DM_ENG_ParameterManager_removeAllTransferResults()
{
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   while (tr != NULL)
   {
      DM_ENG_TransferRequest* next = tr->next;
      if (tr->state == _COMPLETED)
      {
         DM_ENG_ParameterData_removeTransferRequest(tr->transferId);
      }
      tr = next;
   }
   DM_ENG_ParameterData_sync();
}

DM_ENG_DownloadRequest* DM_ENG_ParameterManager_getFirstDownloadRequest()
{
   return DM_ENG_ParameterData_getDownloadRequests();
}

void DM_ENG_ParameterManager_removeFirstDownloadRequest()
{
   DM_ENG_ParameterData_removeFirstDownloadRequest();
   DM_ENG_ParameterData_sync();
}

void DM_ENG_ParameterManager_removeAllDownloadRequests()
{
   DM_ENG_DownloadRequest* dr = DM_ENG_ParameterData_getDownloadRequests();
   while (dr != NULL)
   {
      dr = dr->next;
      DM_ENG_ParameterData_removeFirstDownloadRequest();
   }
   DM_ENG_ParameterData_sync();
}

void DM_ENG_ParameterManager_convertNamesToLong(const char* objName, DM_ENG_ParameterValueStruct* paramVal[])
{
   int j;
   for (j = 0; paramVal[j] != NULL; j++)
   {
      char* prmName = (char*)paramVal[j]->parameterName;
      paramVal[j]->parameterName = DM_ENG_Parameter_getLongName(prmName, objName);
      free(prmName);
   }
}

static void _convertNamesToLong(const char* objName, char* names[])
{
   int j;
   for (j = 0; names[j] != NULL; j++)
   {
      char* prmName = names[j];
      names[j] = DM_ENG_Parameter_getLongName(prmName, objName);
      free(prmName);
   }
}

/////////////////////////////////////////////////

static char* _getValue(char* prmName, const char* destName)
{
   char* longName = DM_ENG_Parameter_getLongName(prmName, destName);
   DM_ENG_Parameter* prm = DM_ENG_ParameterData_getParameter(longName);
   if (prm != NULL)
   {
      if ((prm->storageMode == DM_ENG_StorageMode_DM_ONLY) && DM_ENG_Parameter_isValuable(longName))
      {
         if ((prm->type == DM_ENG_ParameterType_BOOLEAN) && ((prm->value == NULL) || (*prm->value == '\0')))
         // chaîne NULL ou vide considérée comme false
         {
            DM_ENG_Parameter_updateValue(prm, (char*)DM_ENG_FALSE_STRING, false);
         }
      }
      else if (prm->value == NULL)
      {
         if (prm->storageMode == DM_ENG_StorageMode_COMPUTED)
         {
            _loadComputedParameter(prm, false);
         }
         else
         {
            DM_ENG_ParameterManager_loadSystemParameter(prm, true);
         }
         prm = DM_ENG_ParameterData_getParameter(longName);
      }
   }
   free(longName);
   return (prm == NULL ? NULL : prm->value);
}

static void _relocateInstances(char* objName, unsigned int instanceNb)
{
   char* shortObjName = objName + strlen(DM_ENG_PARAMETER_PREFIX);
   int objLen = strlen(shortObjName);
   char* instanceName = (char*)calloc(objLen+12, sizeof(char));
   sprintf(instanceName, "%s%u.", shortObjName, instanceNb);
   int numLen = strlen(instanceName) - objLen - 1;
   char* prmName;
   for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
   {
      if (strncmp(prmName + strlen(DM_ENG_PARAMETER_PREFIX), instanceName, strlen(instanceName)) == 0)
      {
         DM_ENG_Parameter* param = DM_ENG_ParameterData_getCurrent();
         if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) && (param->definition != NULL))
         {
            if (*param->definition == '#')
            {
               if ((strncmp(param->definition+1, shortObjName, objLen) == 0) && (param->definition[objLen+1]=='.'))
               {
                  char* newExp = (char*)calloc(strlen(param->definition)+numLen+1, sizeof(char));
                  sprintf(newExp, "#%s%s", instanceName, param->definition+objLen+2);
                  free(param->definition);
                  param->definition = newExp;
                  DM_ENG_Parameter_setDataChanged(true);
               }
            }
            else if ((strncmp(param->definition, shortObjName, objLen) == 0) && (param->definition[objLen]=='.')) // ne pas modifier que le nom du début !!
            {
               char* newExp = (char*)calloc(strlen(param->definition)+numLen+1, sizeof(char));
               sprintf(newExp, "%s%s", instanceName, param->definition+objLen+1);
               free(param->definition);
               param->definition = newExp;
               DM_ENG_Parameter_setDataChanged(true);
            }
         }
      }
   }
   free(instanceName);
}

/*
 * Récupère le param en base après avoir créé de nouvelles instances si nécessaire
 */
static DM_ENG_Parameter* _getParameter(const char* paramName)
{
   DM_ENG_Parameter* res = NULL;

   int nbIter;
   char* proto = NULL;
   for (nbIter=0; nbIter<5; nbIter++) // 5 itérations max pour éviter les boucles infinies (normalement impossibles)
   {
      res = DM_ENG_ParameterData_getParameter(paramName);
      if (res != NULL) return res; // pas de pb, on a trouvé le paramètre en base ; sinon des instances sont à ajouter

      // on recherche le proto à instancier en supprimant itérativement les n° d'instance de droite à gauche
      // Ex : aa.bb.3.cc.4.dd.2.ee pourra être instancié à partir de aa.bb.3.cc..dd..ee -> instance n°4 de aa.bb.3.cc.
      // puis récursivement, à partir de aa.bb.3.cc.4.dd..ee -> instance n° 2 de aa.bb.3.cc.4.dd.
      proto = strdup(paramName);
      unsigned int instanceNb;
      int k = strlen(proto)-1;
      do
      {
         while ((k>0) && (proto[k] != '.')) { k--; } // on cherche le '.' le plus à droite
         if (k<=1) goto fin; // pas trouvé
         int i = k-1;
         while ((i>0) && (proto[i] >= '0') && (proto[i] <= '9')) { i--; } // on remonte tous les chiffres
         if (i<=0) goto fin; // pas trouvé
         if (proto[i] != '.') { k = i-1; continue /*do while*/; } // ce n'est pas une partie chiffrée
         if (k-i==1) goto fin; // nom incorrect ("..") -> pas trouvé
         proto[k] = '\0';
         if (!DM_ENG_stringToUint(proto+i+1, &instanceNb)) goto fin; // valeur incorrecte -> pas trouvé
         proto[k] = '.';
         if (proto[k+1] == '\0') // queue de chaîne vide -> ne pas terminer par ".."
         {
            proto[i+1] = '\0';
         }
         else
         {
            int j;
            for (j=i; proto[j]!='\0'; j++)
            {
               proto[j+1] = proto[j+k-i];
            }
         }
         if (DM_ENG_ParameterData_getParameter(proto) != NULL) // eureka ! le proto est défini
         {
            proto[i+1] = '\0'; // proto fournit l'objet à instancier
            break /*do while*/;
         }
         k = i-1; // on passe le '.' en i car on n'a jamais 2 parties chiffrées consécutives
      }
      while (true);

      DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(proto);
      if (param == NULL) goto fin; // pb : le proto existe mais pas le noeud objet. Ne devrait pas arriver
      DM_ENG_ParameterData_createInstance(param, instanceNb);
      _relocateInstances(proto, instanceNb);
      DM_ENG_FREE(proto);
      // puis on retente récursivement. On aura autant d'itérations que de niveaux d'instanciation nécessaires
   }

   if (nbIter >= 5)
   {
      WARN("Risk of infinite loop in Parameter Manager\n");
   }

fin :
   DM_ENG_FREE(proto);
   return res;
}

// Marque les sous-param de name référencés dans names
// Supprime les sous-param de name non référencés dans names (non remontées par Device_getObject() ou Device_getNames())
static void _markInstancesLoaded(const char* name, char* names[], bool withValues)
{
   int len = strlen(name);
   char* aSupprimer = NULL;
   char* prmName;

debut:
   for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
   {
      int prmLen = strlen(prmName);
      if ((prmLen>len) && (strncmp(prmName, name, len)==0) && (prmName[prmLen-1] == '.') && (strstr(prmName, "..") == 0))
        // filtre les sous-noeuds de name qui ne soient pas des protos
      {
         int i;
         bool trouve = false;
         for (i=0; !trouve && (names[i]!=NULL); i++)
         {
            trouve = (strncmp(prmName, names[i], prmLen)==0);
         }
         if (trouve)
         {
            DM_ENG_Parameter_markLoaded(DM_ENG_ParameterData_getCurrent(), withValues);
         }
         else if (DM_ENG_Parameter_isNodeInstance(prmName))
         {
            // si c'est un objet de type instance qui n'a pas été trouvé, on peut le supprimer
            // (Les paramètres fixes du data model même s'ils ne sont pas remontés ne sont pas supprimés)
            aSupprimer = strdup(prmName);
            break;
         }
      }
   }
   if (aSupprimer != NULL)
   {
      DM_ENG_ParameterData_deleteObject(aSupprimer);
      DM_ENG_FREE(aSupprimer);
      goto debut; // dans le cas où il y a eu un objet supprimé, on recommence
   }
}

static void _updateSystemValue(DM_ENG_Parameter* param, char* value, bool newValue)
{
   DM_ENG_Parameter_updateValue(param, (value == NULL ? (char*)DM_ENG_EMPTY : value), newValue); // NULL interptété comme EMPTY
}

/**
 * Updates the designated object with the given list of parameters as follows :
 * - the values associated with the parameters in the list are assigned to the parameters,
 * - the new instances referenced in the list are created,
 * - the existing instances non referenced in the list (by any parameter) are removed from the object.
 *
 * @param objName Name of the object to update. It must be a node name (terminated by ".").
 * @param parameterList List of parameters belonging to the object arborescence used for updating
 * @param pushedObject True if the object is pushed, false if it is just loaded
 *
 * N.B. None of the 2 parameters should be zero.
 */
static void _updateObject(const char* objName, DM_ENG_ParameterValueStruct* parameterList[], bool pushedObject)
{
   int nbVal = 0;
   while (parameterList[nbVal] != NULL) { nbVal++; }

   //char* paramNames[nbVal];
   char** paramNames = (char**)calloc(nbVal+1, sizeof(char*));
   int j;
   for (j = 0; parameterList[j] != NULL; j++)
   {
      paramNames[j] = (char*)parameterList[j]->parameterName;
      DM_ENG_Parameter* param = _getParameter(paramNames[j]);
      if (param != NULL) // sinon on laisse tomber
      {
         _updateSystemValue(param, parameterList[j]->value, pushedObject);
      }
   }

   _markInstancesLoaded((char*)objName, paramNames, true);
   free(paramNames);
}

// Retourne 0 si OK, 1 si aucun changement n'est apporté au data model et 900X en cas d'erreur
static int _loadNames(const char* name, const char* data)
{
   // On charge de le data model instancié depuis le système et on met à jour les données de DM
   char** paramNames = NULL;
   int res = DM_ENG_Device_getNames(name, data, &paramNames);
   if ((res == 0) && (paramNames != NULL))
   {
      _convertNamesToLong(name, paramNames);
      int j;
      for (j = 0; paramNames[j] != NULL; j++)
      {
         DM_ENG_Parameter* param = _getParameter(paramNames[j]);
         if (param != NULL) // sinon on laisse tomber
         {
            DM_ENG_Parameter_markLoaded(param, false);
         }
      }

      _markInstancesLoaded(name, paramNames, false);

      for (j = 0; paramNames[j] != NULL; j++)
      {
         free(paramNames[j]);
      }
      free(paramNames);
   }
   return res;
}

// effectue la mise à jour du paramètre système fourni, noeud ou feuille
static int _loadFinal(DM_ENG_Parameter* param, bool withValues)
{
   int res = 0;
   char* name = strdup(param->name);
   if (!DM_ENG_Parameter_isLoaded(param, withValues) // sinon rien à faire
    && (strstr(name, "..") == 0)) // param n'est pas un proto (sinon rien à faire)
   {
      bool isNode = (name[strlen(name)-1] == '.');
      res = 1;
      if (withValues)
      {
         if (isNode)
         {
            // On charge de le data model instancié depuis le système et on met à jour les données de DM
            DM_ENG_ParameterValueStruct** paramVal;
            char* data = NULL;
            if ((param->definition != NULL) && (strlen(param->definition) != 0))
            {
               DM_ENG_ComputedExp_eval(param->definition, _getValue, name, &data);
            }
            res = DM_ENG_Device_getObject(name, data, &paramVal);
            DM_ENG_FREE(data);
            if (res == 0)
            {
               DM_ENG_ParameterManager_convertNamesToLong(name, paramVal);
               _updateObject(name, paramVal, false); // false because the object is just loaded from system, not pushed
               DM_ENG_deleteTabParameterValueStruct(paramVal);
            }
         }
         else
         {
            res = DM_ENG_ParameterManager_loadLeafParameterValue(param, false);
         }
      }
      else if (isNode && ((param->loadingMode & 2)==2)) // si mode&2 == 0, il n'y a pas d'instanciation donc l'arborescence ne peut être modifiée
      {
         char* data = NULL;
         if ((param->definition != NULL) && (strlen(param->definition) != 0))
         {
            DM_ENG_ComputedExp_eval(param->definition, _getValue, name, &data);
         }
         res = _loadNames(name, data);
         DM_ENG_FREE(data);
      }

      if ((res == 0) || (res == 1)) // cas 1 : le data model est resté inchangé toutefois on marque le param comme étant à jour
      {
         param = DM_ENG_ParameterData_getParameter(name); // on récharge le param car les load ont pu modifier le pointeur
         if (param != NULL)
         {
            DM_ENG_Parameter_markLoaded(param, withValues);
            res = 0;
         }
         else
         {
            res = DM_ENG_INTERNAL_ERROR;
         }
      }
   }
   free(name);
   return res;
}

static void _razDiagnosticsResult(const char* objName, bool requested)
{
   int objLen = strlen(objName);
   char* aSupprimer = NULL;
   char* prmName;

debut:
   for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
   {
      int prmLen = strlen(prmName);
      if ((prmLen > objLen) && (strncmp(prmName, objName, objLen)==0))
      {
         if (DM_ENG_Parameter_isValuable(prmName))
         {
            DM_ENG_Parameter* param = DM_ENG_ParameterManager_getCurrentParameter();
            if (DM_ENG_Parameter_isDiagnosticsState(prmName) ? !requested : !param->writable)
            {
               // on réinitialise les param R/O + éventuellement DiagnosticsState à "None"
               DM_ENG_Parameter_resetValue(param);
            }
         }
         else if (DM_ENG_Parameter_isNodeInstance(prmName))
         {
            // Les objets de type instance doivent être supprimés
            aSupprimer = strdup(prmName);
            break;
         }
      }
   }
   if (aSupprimer != NULL)
   {
      DM_ENG_ParameterData_deleteObject(aSupprimer);
      DM_ENG_FREE(aSupprimer);
      goto debut; // dans le cas où il y a eu un objet supprimé, on recommence
   }
}

static char* _computeNumberOfIntances(char* objName)
{
   DM_ENG_ParameterManager_loadSystemParameters(objName, false);
   int objLen = strlen(objName);
   char* pName;
   int nbOf = 0;
   for (pName = DM_ENG_ParameterData_getFirstName(); pName!=NULL; pName = DM_ENG_ParameterData_getNextName())
   {
      if (strncmp(pName, objName, objLen)==0)
      {
         int j = objLen;
         while ((pName[j] != '\0') && (pName[j] != '.')) { j++; }
         if ((pName[j] == '.') && (j>objLen) &&  (pName[j+1] == '\0')) { nbOf++; }
      }
   }
   return DM_ENG_intToString(nbOf);
}

// le param fourni doit être COMPUTED
static int _loadComputedParameter(DM_ENG_Parameter* param, bool checkChange)
{
   if (DM_ENG_Parameter_isBeingEvaluated(param)) return DM_ENG_INTERNAL_ERROR; // le data model comporte une boucle

   bool bRes = true;
   char* oldValue = (param->value==NULL ? NULL : strdup(param->value));
   char* prmName = strdup(param->name);
   char* prmVal = NULL;
   if (param->definition!=NULL)
   {
      DM_ENG_Parameter_markBeingEvaluated(param); // pour éviter les boucles infinies

      if (*param->definition == '#') // compteur d'éléments, pas besoin des valeurs
      {
         char* longName = DM_ENG_Parameter_getLongName(param->definition+1, prmName);
         prmVal = _computeNumberOfIntances(longName);
         free(longName);
      }
      else // expression computed à calculer
      {
         bRes = DM_ENG_ComputedExp_eval(param->definition, _getValue, prmName, &prmVal);
      }

      param = DM_ENG_ParameterData_getParameter(prmName);
      DM_ENG_Parameter_unmarkBeingEvaluated(param);
   }
   if (bRes && (prmVal != NULL) && ((oldValue == NULL) || (strcmp(prmVal, oldValue) != 0))) // prmVal == NULL signifierait : pas de (nouvelle) valeur
   {
      _updateSystemValue(param, prmVal, checkChange); // si checkChange == false, le flag VALUE CHANGE n'est pas modifié (s'il était true, il le reste)
   }
   DM_ENG_Parameter_markLoaded(param, true);
   DM_ENG_FREE(prmName);
   DM_ENG_FREE(prmVal);
   DM_ENG_FREE(oldValue);
   return (bRes ? 0 : 1);
}

// le param fourni doit être COMPUTED
static bool _isValueChanged(char* shortname, const char* destName, bool* pPushed)
{
   bool changed = false;
   char* longName = DM_ENG_Parameter_getLongName(shortname, destName);
   DM_ENG_Parameter* prm = DM_ENG_ParameterData_getParameter(longName);
   free(longName);
   if (prm != NULL)
   {
      if (pPushed != NULL) { *pPushed = DM_ENG_Parameter_isPushed(prm); }
      changed = DM_ENG_Parameter_isValueChanged(prm);
   }
   return changed;
}

// le param fourni doit être COMPUTED
static void _checkComputedChanged(DM_ENG_Parameter* param)
{
   bool changed = false;
   bool pushed = false;
   char* prmName = strdup(param->name);
   char* bVal = NULL;
   if (param->definition!=NULL)
   {
      if (*param->definition == '#') // compteur d'éléments, pas besoin des valeurs
      {
         changed = _isValueChanged(param->definition+1, prmName, &pushed);
      }
      else // expression computed à calculer
      {
         DM_ENG_ComputedExp_isChanged(param->definition, _isValueChanged, _getValue, prmName, &changed, &pushed);
      }
   }
   if (changed)
   {
      param = DM_ENG_ParameterData_getParameter(prmName); // on recharge param
      if ((param->value != NULL) || (param->notification == DM_ENG_NotificationMode_PASSIVE) || (param->notification == DM_ENG_NotificationMode_ACTIVE))
        // on calcule la nouvelle valeur et on confirme ou non qu'elle a bien changé
      {
         _loadComputedParameter(param, true); // positionnera VALUE_CHANGED si la valeur a bien changé
      }
      else
      {
         DM_ENG_Parameter_updateValue(param, NULL, true); // efface la valeur et positionne VALUE_CHANGED
      }
   }
   if (pushed)
   {
      param = DM_ENG_ParameterData_getParameter(prmName); // on recharge param
      DM_ENG_Parameter_markPushed(param);
   }
   DM_ENG_FREE(prmName);
   DM_ENG_FREE(bVal);
}

static bool _createParameter(char* newName, char* destName, DM_ENG_ParameterType type, char* args, char* configKey)
{
   bool res = false;
   char* longName = DM_ENG_Parameter_getLongName(newName, destName);
   DM_ENG_Parameter* prm = DM_ENG_ParameterData_getParameter(longName);
   if (prm == NULL)
   {
      prm = DM_ENG_newDefinedParameter(longName, type, args, true); // paramètre interne (= hidden)
      if (configKey != NULL) { prm->configKey = strdup(configKey); }
      DM_ENG_ParameterData_addParameter(prm);
      DM_ENG_ParameterData_sync();
      res = true;
   }
   else if (configKey != NULL)
   {
      char* ck = configKey;
      while (*ck != '\0')
      {
         if (*ck == ',') { configKey = ck+1; } // ds le cas d'une liste de configKey, on ne s'occupe que du dernier
         ck++;
      }
      DM_ENG_Parameter_addConfigKey(prm, configKey);
   }
   free(longName);
   return res;
}

static int _loadSystemParameter(DM_ENG_Parameter* param, char** pData)
{
   int res = 0;
   if (((param->storageMode == DM_ENG_StorageMode_SYSTEM_ONLY) || (param->storageMode == DM_ENG_StorageMode_MIXED))
     && (param->value == NULL))
   {
      char* val = NULL;
      char* data = NULL;
      if ((param->definition != NULL) && (strlen(param->definition) != 0))
      {
         DM_ENG_ComputedExp_eval(param->definition, _getValue, param->name, &data);
         // recharger param ??
      }
      res = DM_ENG_Device_getValue(param->name, data, &val);
      if (pData == NULL) { DM_ENG_FREE(data); } else { *pData = data; }
      if (res==0)
      {
         _updateSystemValue(param, val, false);
         DM_ENG_FREE(val);
      }
   }
   return res;
}

// parcourt la liste des "sous-paramètres" pour effectuer les mises à jour des paramètres système
// si param == NULL, on parcourt tout le data model
static int _loadSubParameters(DM_ENG_Parameter* param, bool withValues)
{
   int res = 0;
   if ((param==NULL) || (param->name[strlen(param->name)-1] == '.'))
   {
      if ((param!=NULL) && ((param->loadingMode & 1) != 0))
      {
         res = _loadFinal(param, withValues);
      }
      else // itérer pour trouver un élément à mettre à jour
      {
         if (param != NULL) { DM_ENG_Parameter_markLoaded(param, withValues); } // on marque le noeud comme updated pour éviter de recommencer cette recherche par la suite
         char* name = strdup(param==NULL ? "" : param->name);
         int len = strlen(name);
         int count = 0;
         bool trouve = false;
         do
         {
            char* foundName = NULL;
            char* foundComputedName = NULL; // sert à prioriser les mises à jour de non computed
            char* prmName;
            for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
            {
               if (((len==0) || (((int)strlen(prmName)>len) && (strncmp(prmName, name, len)==0))) // filtre les sous-paramètres de param
                 && (strstr(prmName, "..") == 0) // on passe les protos
                 && ((foundName == NULL) || ((strlen(prmName)<strlen(foundName)) && (strncmp(prmName, foundName, strlen(prmName))==0)))) // permet de converger vers l'objet racine du groupe qui n'est pas nécessaire parcouru en 1er dans l'itération
               {
                  DM_ENG_Parameter* current = DM_ENG_ParameterData_getCurrent();
                  if ((((current->loadingMode & 1) != 0) || (prmName[strlen(prmName)-1] != '.'))
                   && !DM_ENG_Parameter_isLoaded(current, withValues)
                   && (strstr(prmName, "..")==0))
                  {
///// solution non optimisée /////
//                     DM_ENG_FREE(foundName);
//                     foundName = strdup(prmName);
///// solution optimisée pour réduire le nb de parcours complets du data model  /////
                     if (((current->loadingMode & 1) == 0) && (current->storageMode != DM_ENG_StorageMode_COMPUTED))
                     {                           // c'est nécessairement une feuille : la valeur peut être obtenue isolément,
                                                 // on le fait en cours d'itération pour une meilleure efficacité
                        res = _loadSystemParameter(current, NULL);
                        if (res != 0)
                        {
                           DM_ENG_FREE(foundName);
                           break; /* for prmName */
                        }
                     }
                     else if ((foundName == NULL) && (foundComputedName == NULL) && (current->storageMode == DM_ENG_StorageMode_COMPUTED))
                     {
                        foundComputedName = strdup(prmName);
                     }
                     else
                     {
                        DM_ENG_FREE(foundName);
                        foundName = strdup(prmName);
                     }
/////////////////////////////
                  }
               }
            }
            if (foundComputedName != NULL)
            {
               if (foundName == NULL) { foundName = foundComputedName; }
               else { DM_ENG_FREE(foundComputedName); }
            }
            trouve = (foundName != NULL);
            if (trouve)
            {
               DM_ENG_Parameter* foundParam = DM_ENG_ParameterData_getParameter(foundName);
               res = _loadFinal(foundParam, withValues);
               free(foundName);
            }
            if (count++ > _MAX_SYSTEM_ENTRY) // pour détecter les boucles infinies
            {
               res = DM_ENG_INTERNAL_ERROR;
               EXEC_ERROR("Infinite loop in updating parameter values\n");
            }
         }
         while ((res == 0) && trouve);
         free(name);
      }
   }
   else if (withValues)
   {
      res = DM_ENG_ParameterManager_loadLeafParameterValue(param, false);
   }
   return res;
}

/*
 * cherche le proto le plus profond (on cherche un n° d'instance de droite à gauche) dont name serait une instance
 * retourne -1 si pas de proto, sinon l'index de la sous-chaîne ".."
 */
static int _computeProto(char* name)
{
   int i = strlen(name)-1;
   int k = 0;
   while (i>=0)
   {
      if (k==0)
      {
         if (name[i]=='.')
         {
            k = i;
         }
      }
      else
      {
         if (name[i]=='.')
         {
            if (k-i==1)
            {
               k=i;
            }
            else // on a trouvé un noeud chiffré : condition 0 <= i < k-1 && name[i]=='.' && name[k]=='.' && chiffres entre les 2
            {
               int j;
               for (j=i; name[j]!='\0'; j++)
               {
                  name[j+1] = name[j+k-i];
               }
               break;
            }
         }
         else if ((name[i]<'0') || (name[i]>'9'))
         {
            k = 0;
         }
      }
      i--;
   }
   return i;
}

/**
 * Checks the parameter (COMPUTED or not) to obtain its value
 *
 * @param param Leaf Parameter
 * @param checkChange If it is necessary to check if the param changed (useful if param already has a value)
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, 9005, ...) according to the TR-069.
 */
int DM_ENG_ParameterManager_loadLeafParameterValue(DM_ENG_Parameter* param, bool checkChange)
{
   int res = 0;
   if (param->storageMode == DM_ENG_StorageMode_COMPUTED)
   {
      if (checkChange)
      {
         char* prmName = strdup(param->name);
         _checkComputedChanged(param);
         param = DM_ENG_ParameterData_getParameter(prmName);
         free(prmName);
      }
      if (!DM_ENG_Parameter_isLoaded(param, true))
      {
         res = _loadComputedParameter(param, checkChange);
      }
   }
   else
   {
      res = _loadSystemParameter(param, NULL);
   }
   return res;
}

/**
 * Load the system parameter. Load the corresponding values if withValues is true
 *
 * @param param Parameter or object
 * @param withValues If false, only names are loaded. If true, values are also loaded.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, 9005, ...) according to the TR-069.
 */
int DM_ENG_ParameterManager_loadSystemParameter(DM_ENG_Parameter* param, bool withValues)
{
   int res = 0;
   if (DM_ENG_Parameter_isLoaded(param, withValues)) return 0; // données déjà à jour, pas la peine de continuer

   // cas des données groupées : on cherche l'objet racine du groupe
   if ((param->loadingMode & 1) != 0)
   {
      DM_ENG_Parameter* parent = param;
      char* parentName = strdup(param->name);
      do
      {
         param = parent;
         int len = strlen(parentName)-1;
         do
         {
            len--;
         }
         while ((len > 0) && ((parentName[len] != '.') || (parentName[len-1] == '.')));
         if (len <= 0) break; // sortie normalement irréalisable
         parentName[len+1] = '\0';
         parent = DM_ENG_ParameterData_getParameter(parentName);
      }
      while ((parent != NULL) && ((parent->loadingMode & 1) != 0));
      free(parentName);

      res = _loadFinal(param, withValues);
   }
   else
   {
      res = _loadSubParameters(param, withValues);
   }
   return res;
}

/**
 * Load the system parameters associated to the provided name. Load the corresponding values if withValues is true
 *
 * @param name Name of a parameter or partial path name
 * @param withValues If false, only names are loaded. If true, values are also loaded.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, 9005, ...) according to the TR-069.
 */
int DM_ENG_ParameterManager_loadSystemParameters(const char* name, bool withValues)
{
   if (name == NULL) return DM_ENG_INVALID_PARAMETER_NAME;
   if (strlen(name) == 0)
   {
      return _loadSubParameters(NULL, withValues);
   }

   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if (param == NULL)
   {
      // Le paramètre n'est pas stocké dans la base mais ce peut être une instance système.
      // On cherche donc un prototype dont ce pourrait être une instance.
      char* proto = strdup(name);
      do
      {
         int i = _computeProto(proto); // i index de la 1ère sous-chaîne ".." du proto obtenu
         if (i<=0) break;
         param = DM_ENG_ParameterData_getParameter(proto);
      }
      while (param == NULL);
      free(proto);
      if (param == NULL)
      {
         DBG("No parameter found.");
         return DM_ENG_INVALID_PARAMETER_NAME; // nom invalide : aucun paramètre, objet ou proto avec ce nom
      }
   }

   return DM_ENG_ParameterManager_loadSystemParameter(param, withValues);
}

/**
 *
 * @param name Name of a requested Parameter.
 *
 * @return The parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterManager_getParameter(const char* name)
{
   return DM_ENG_ParameterData_getParameter(name);
}

/**
 *
 * @param name Name of a requested Parameter.
 *
 * @return Returns Value of the requested Parameter.
 */
int DM_ENG_ParameterManager_getParameterValue(const char* name, OUT char** pValue)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if ((param == NULL) || !DM_ENG_Parameter_isValuable(param->name)) return DM_ENG_INVALID_PARAMETER_NAME;
   int res = _loadSystemParameter(param, NULL);
   if (res == 0) { DM_ENG_Parameter_getValue(param, pValue); }
   return res;
}

static int _setParameterValue(DM_ENG_Parameter* param, char* value, char** pData)
{
   int res = DM_ENG_Parameter_checkBeforeSetValue(param, value);
   if (res == 0) { res = _loadSystemParameter(param, pData); } // charger la valeur précédente pour être capable de la restituer en cas d'erreur
   if (res == 0) { res = DM_ENG_Parameter_setValue(param, value); }
   return res;
}

/**
 * @param name Name of a parameter to set
 * @param value Value to set the named parameter
 *
 * @return Returns Status as follows :
 * - DM_ENG_ParameterStatus_APPLIED : Parameter changes have been validated and applied.
 * - DM_ENG_ParameterStatus_READY_TO_APPLY : Parameter changes have been validated and committed, but not yet applied
 * (for example, if a reboot is required before the new values are applied).
[GL]* - DM_ENG_ParameterStatus_UNDEFINED : Undefined status (?).
 *
 * @throws ReadOnlyParameterException if non writable parameter
 */
int DM_ENG_ParameterManager_setParameterValue(DM_ENG_EntityType entity, const char* name, DM_ENG_ParameterType type, char* value, OUT DM_ENG_ParameterStatus* pResult, OUT char** pSysName, OUT char** pData)
{
   int res = 0;
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);

   if ((param == NULL) || DM_ENG_Parameter_isNode(param->name)
     || ((*pResult == DM_ENG_ParameterStatus_UNDEFINED) // vérification uniquement pour le 1er appel (non récursif)
          && DM_ENG_Parameter_isHidden(param) && (entity != DM_ENG_EntityType_SYSTEM))) // paramètre caché, c'est comme s'il n'existait pas
   {
     DBG("DM_ENG_INVALID_PARAMETER_NAME");
     return DM_ENG_INVALID_PARAMETER_NAME;
   }

   if (entity == DM_ENG_EntityType_SYSTEM)
   {
      _updateSystemValue(param, value, true);
   }
   else
   {
      if (!param->writable || !DM_ENG_EntityType_belongsTo(entity, param->accessList)) {
        DBG("DM_ENG_READ_ONLY_PARAMETER");
        return DM_ENG_READ_ONLY_PARAMETER;
      }

      if ((param->type != type) && (type != DM_ENG_ParameterType_UNDEFINED)) {
        DBG("DM_ENG_INVALID_PARAMETER_TYPE");
        // return DM_ENG_INVALID_PARAMETER_TYPE; DO NOT RETURN
      }

      if (DM_ENG_Parameter_isBeingEvaluated(param)) // Pour éviter les boucles infinies
      {
         res = DM_ENG_INTERNAL_ERROR;
      }
      else
      {
         DM_ENG_Parameter_markBeingEvaluated(param);

         char* nextName = NULL; // nom pour set récursif dans le cas des redirections
         if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) && (param->definition != NULL))
         {
            nextName = DM_ENG_Parameter_getLongName(param->definition, name);
         }

         res = _setParameterValue(param, value, pData);
         *pResult = (param->immediateChanges ? DM_ENG_ParameterStatus_APPLIED : DM_ENG_ParameterStatus_READY_TO_APPLY);
         *pSysName = param->name;

         if ((res == 0) && (nextName != NULL))
         {
            if (pData != NULL) { DM_ENG_FREE(*pData); }
            DBG("Redirection to parameterName = %s, type = %d, Value = %s", nextName, type, value);
            res = DM_ENG_ParameterManager_setParameterValue(entity, nextName, type, value, pResult, pSysName, pData);
            param = DM_ENG_ParameterData_getParameter(name); // on recharge param
         }
         DM_ENG_FREE(nextName);
      }
      DM_ENG_Parameter_unmarkBeingEvaluated(param);
   }

   return res;
}

int DM_ENG_ParameterManager_addObject(DM_ENG_EntityType entity, char* objectName, OUT unsigned int* pInstanceNumber, OUT DM_ENG_ParameterStatus* pStatus)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(objectName);
   if ((param == NULL) || !DM_ENG_Parameter_isNode(param->name)) return DM_ENG_INVALID_PARAMETER_NAME;
   if (!param->writable) return DM_ENG_REQUEST_DENIED;
   *pStatus = (param->immediateChanges ? DM_ENG_ParameterStatus_APPLIED : DM_ENG_ParameterStatus_READY_TO_APPLY);

   if (param->storageMode == DM_ENG_StorageMode_DM_ONLY)
   {
      if ((unsigned int)param->minValue >= INT_MAX) return DM_ENG_RESOURCES_EXCEEDED;
        // le nouveau numero d'instance est tjrs incrémenté. Une fois le numero INT_MAX atteint, il n'est plus possible de créer
        // de nouvelles instances => mettre en place un mécanisme pour la réutilisation des instance numbers libérés
      param->minValue++;
      *pInstanceNumber = (unsigned int)param->minValue;
      DM_ENG_Parameter_commit(param); // interblocage possible ? Non, car pas de notification sur les noeuds
   }
   else if (entity != DM_ENG_EntityType_SYSTEM)
   {
      int ret = DM_ENG_Device_addObject(objectName, pInstanceNumber);
      if (ret != 0) return ret;
   }
   else
   {
      return DM_ENG_REQUEST_DENIED;
   }

   DM_ENG_Parameter_updateValue(param, NULL, true); // efface la valeur et positionne VALUE_CHANGED
                                                    // interblocage possible ? Non, comme ci-dessus
   DM_ENG_ParameterData_createInstance(param, *pInstanceNumber);
   _relocateInstances(objectName, *pInstanceNumber);

   return 0;
}

int DM_ENG_ParameterManager_deleteObject(DM_ENG_EntityType entity, char* objectName, OUT DM_ENG_ParameterStatus* pStatus)
{
   int res = 0;
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(objectName);
   if ((param == NULL) || !DM_ENG_Parameter_isNode(param->name)) return DM_ENG_INVALID_PARAMETER_NAME;
   int k=strlen(objectName)-3; // recherche de la position de l'avant-dernier '.'
   while ((k>0) && (objectName[k]!='.')) k--;
   if (k<=0) return DM_ENG_INVALID_PARAMETER_NAME;
   errno = 0;
   unsigned long l = strtoul(objectName+k+1, (char**)NULL, 10);
   if ((errno == ERANGE) || (l>UINT_MAX)) return DM_ENG_INVALID_PARAMETER_NAME;
   unsigned int instanceNumber = l;

   // recherche de l'objet prototype
   char* protoName = (char*) malloc(k+2);
   strncpy(protoName, objectName, k+1);
   protoName[k+1] = '\0';
   DM_ENG_Parameter* proto = DM_ENG_ParameterData_getParameter(protoName);
   free(protoName);
   if ((proto == NULL) || !DM_ENG_Parameter_isNode(proto->name)) return DM_ENG_INVALID_PARAMETER_NAME;
   if (!proto->writable) return DM_ENG_REQUEST_DENIED;
   *pStatus = (proto->immediateChanges ? DM_ENG_ParameterStatus_APPLIED : DM_ENG_ParameterStatus_READY_TO_APPLY);

   if ((proto->storageMode != DM_ENG_StorageMode_DM_ONLY) && (entity != DM_ENG_EntityType_SYSTEM))
   {
      // Suppression effective du paramètre
      res = DM_ENG_Device_deleteObject(proto->name, instanceNumber);
   }

   if ((res == 0)
    || (res == DM_ENG_INVALID_PARAMETER_NAME)) // L'instance n'est pas présente au niveau système, elle ne doit pas être dans le data model non plus
   {
      DM_ENG_Parameter_updateValue(proto, NULL, true); // efface la valeur et positionne VALUE_CHANGED
                                                       // interblocage possible ? Non, pas de notification sur les noeuds
      DM_ENG_ParameterData_deleteObject(objectName);
   }
   return res;
}

/**
 * @param parameterKey The value to set the ParameterKey parameter. The value of this argument is left
 * to the discretion of the server, and may be left empty.
 */
void DM_ENG_ParameterManager_setParameterKey(char* parameterKey)
{
   if (parameterKey != NULL)
   {
      DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(DM_ENG_PARAMETER_KEY_NAME);
      _setParameterValue(param, parameterKey, NULL);
      DM_ENG_Parameter_commit(param);
   }
}

/**
 * @param name Name of a parameter to commit
 */
void DM_ENG_ParameterManager_commitParameter(const char* name)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if ((param == NULL) || DM_ENG_Parameter_isNode(param->name)) { return; }

   char* nextName = NULL; // nom pour commit récursif dans le cas des redirections
   if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) && (param->definition != NULL))
   {
      nextName = DM_ENG_Parameter_getLongName(param->definition, name);
   }

   DM_ENG_Parameter_commit(param);

   char* objName;
   if (DM_ENG_Parameter_isDiagnosticsParameter(name, &objName))
   {
      DM_ENG_ParameterValueStruct* pvs = diagnosticsRequested;
      while ((pvs != NULL) && (strcmp(pvs->parameterName, objName) != 0)) { pvs = pvs->next; }
      if (pvs == NULL)
      {
         pvs = DM_ENG_newParameterValueStruct(objName, (DM_ENG_ParameterType)0, NULL);
         DM_ENG_addParameterValueStruct(&diagnosticsRequested, pvs);
      }
      if ((DM_ENG_Parameter_isDiagnosticsState(name)) && (strcmp(param->value, DM_ENG_REQUESTED_STATE) == 0))
      {
         pvs->type =(DM_ENG_ParameterType) 1; // type mis à 1 si le State est positionné à Requested (sinon il reste à 0)
      }
      free(objName);
   }
   else if ( (strcmp(name, DM_ENG_PERIODIC_INFORM_ENABLE_NAME) == 0)
          || (strcmp(name, DM_ENG_PERIODIC_INFORM_INTERVAL_NAME) == 0)
          || (strcmp(name, DM_ENG_PERIODIC_INFORM_TIME_NAME) == 0) )
   {
      periodicInformChanged = true;
   }
   else if (strcmp(name, DM_TR106_ACS_URL) == 0)
   {
      acsParametersToUpdate = true;
      DM_ENG_InformMessageScheduler_bootstrapInform();
   }
   else if ((strcmp(name, DM_TR106_ACS_USERNAME) == 0) || (strcmp(name, DM_TR106_ACS_PASSWORD) == 0))
   {
      acsParametersToUpdate = true;
   }
   else // paramètres de contrôle des objets 'statistiques'
   {
      int k = strlen(name)-1;
      while ((k>0) && (name[k]!='.')) { k--; }
      if (k>0)
      {
         objName = strdup(name);
         objName[k+1] = '\0';
         param = DM_ENG_ParameterData_getParameter(objName);
         if ((param == NULL) || (param->type != DM_ENG_ParameterType_STATISTICS)) // on essaie l'objet au dessus (utile par exemple pour les obj.Total.Reset)
         {
            do { k--; } while ((k>0) && (objName[k]!='.'));
            if (k>0)
            {
               objName[k+1] = '\0';
               param = DM_ENG_ParameterData_getParameter(objName);
            }
         }
         if ((param != NULL) && (param->type == DM_ENG_ParameterType_STATISTICS))
         {
            DM_ENG_StatisticsModule_performParameterSetting(objName, name);
         }
         free(objName);
      }
   }

   if (nextName != NULL)
   {
      DM_ENG_ParameterManager_commitParameter(nextName);
      free(nextName);
   }
}

/**
 * @param name Name of a parameter to unset
 */
void DM_ENG_ParameterManager_unsetParameter(const char* name)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if ((param == NULL) || DM_ENG_Parameter_isNode(param->name)) { return; }

   bool cancelled = DM_ENG_Parameter_cancelChange(param); // cancelled permet d'éviter les boucles infinies

   if (cancelled && (param->storageMode == DM_ENG_StorageMode_COMPUTED) && (param->definition != NULL))
   {
      char* defName = DM_ENG_Parameter_getLongName(param->definition, name);
      DM_ENG_ParameterManager_unsetParameter(defName);
      DM_ENG_FREE(defName);
   }
}

/**
 * Cancels the changes not validated when the device stopped.
 */
void DM_ENG_ParameterManager_cancelNotCommittedChanges()
{
   DM_ENG_Parameter* param;
   for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
   {
      DM_ENG_Parameter_cancelChange(param);
   }
}

/**
 * @return Mandatory parameters
 */
char** DM_ENG_ParameterManager_getMandatoryParameters()
{
   int nbParam = DM_ENG_ParameterData_getNumberOfParameters();
   char** resTmp = (char**)calloc(nbParam+1, sizeof(char*));
   int nbMand = 0;
   DM_ENG_Parameter* param;
   for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
   {
      if (param->mandatoryNotification) { resTmp[nbMand++] = strdup(param->name); }
   }
   char** res = (char**)calloc(nbMand+1, sizeof(char*));
   int i;
   for (i=0; i<nbMand+1; i++)
   {
      res[i] = resTmp[i];
   }
   free(resTmp);
   return res;
}

/**
 * @param name
 * @return
 */
bool DM_ENG_ParameterManager_isInformParameter(const char* name)
{
   bool res = false;
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   res = ((param!=NULL) && (param->mandatoryNotification || (param->notification == DM_ENG_NotificationMode_PASSIVE)
                                                         || (param->notification == DM_ENG_NotificationMode_ACTIVE)));
   return res;
}

/////////// Accès à des paramètres particuliers ////////////

/**
 * @return A particular parameter value
 */
bool DM_ENG_ParameterManager_isPeriodicInformEnabled()
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(DM_ENG_PERIODIC_INFORM_ENABLE_NAME);
   return (param->value!=NULL)
       && (strlen(param->value)==1 ? (*param->value==DM_ENG_TRUE_CHAR) : (strcasecmp(param->value, DM_ENG_TRUE_STRING)==0));
}

/**
 * @return A particular parameter value
 */
unsigned int DM_ENG_ParameterManager_getPeriodicInformInterval()
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(DM_ENG_PERIODIC_INFORM_INTERVAL_NAME);
   unsigned int res = UINT_MAX;
   if (param != NULL) { DM_ENG_stringToUint(param->value, &res); }
   else { WARN("Parameter %s missing", DM_ENG_PERIODIC_INFORM_INTERVAL_NAME); }
   return res;
}

/**
 * @return A particular parameter value
 */
time_t DM_ENG_ParameterManager_getPeriodicInformTime()
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(DM_ENG_PERIODIC_INFORM_TIME_NAME);
   time_t res = 0;
   if (param != NULL) { DM_ENG_dateStringToTime(param->value, &res); }
   return res;
}

/**
 * @return A particular parameter value
 */
char* DM_ENG_ParameterManager_getManufacturerName()
{
   char* res;
   DM_ENG_ParameterManager_getParameterValue(DM_ENG_DEVICE_MANUFACTURER_NAME, &res);
   return res;
}

/**
 * @return A particular parameter value
 */
char* DM_ENG_ParameterManager_getManufacturerOUI()
{
   char* res;
   DM_ENG_ParameterManager_getParameterValue(DM_ENG_DEVICE_MANUFACTURER_OUI_NAME, &res);
   return res;
}

/**
 * @return A particular parameter value
 */
char* DM_ENG_ParameterManager_getProductClass()
{
   char* res;
   DM_ENG_ParameterManager_getParameterValue(DM_ENG_DEVICE_PRODUCT_CLASS_NAME, &res);
   return res;
}

/**
 * @return A particular parameter value
 */
char* DM_ENG_ParameterManager_getSerialNumber()
{
   char* res;
   DM_ENG_ParameterManager_getParameterValue(DM_ENG_DEVICE_SERIAL_NUMBER_NAME, &res);
   return res;
}

////// Accès aux données persistantes (en plus des paramètres) ///////

/**
 * Add a scheduled Inform
 */
void DM_ENG_ParameterManager_addScheduledInformCommand(time_t time, char* commandKey)
{
   DM_ENG_ParameterData_addScheduledInformCommand(time, commandKey);
}

/**
 * Retourne l'échéance la plus proche, événement PERIODIC ou SCHEDULED, ou téléchargement planifié
 */
time_t DM_ENG_ParameterManager_getNextTimeout()
{
   time_t timeout = DM_ENG_ParameterData_getPeriodicInformTime();
   time_t scheduledInformTime = DM_ENG_ParameterData_getScheduledInformTime();
   if ((scheduledInformTime != 0) && ((timeout==0) || (scheduledInformTime < timeout))) { timeout = scheduledInformTime; }
   time_t transferTime = 0;
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   while (tr != NULL) // on recherche le 1er transfert non démarré
   {
      if (tr->state == _NOT_YET_STARTED)
      {
         transferTime = tr->transferTime;
         break;
      }
      tr = tr->next;
   }
   if ((transferTime != 0) && ((timeout==0) || (transferTime < timeout))) { timeout = transferTime; }
   return timeout;
}

/**
 * Delegation call for ParametersData.setRebootCommandKey()
 */
void DM_ENG_ParameterManager_setRebootCommandKey(char* commandKey)
{
   DM_ENG_ParameterData_setRebootCommandKey(commandKey);
}

/**
 * Add a Reboot commandKey
 */
void DM_ENG_ParameterManager_addRebootCommandKey(char* commandKey)
{
   DM_ENG_ParameterData_addRebootCommandKey(commandKey);
}

/**
 * Delegation call for ParametersData.getRebootCommandKey()
 */
char* DM_ENG_ParameterManager_getRebootCommandKey()
{
   return DM_ENG_ParameterData_getRebootCommandKey();
}

/**
 * Delegation call for ParametersData.getRetryCount()
 */
int DM_ENG_ParameterManager_getRetryCount()
{
   return DM_ENG_ParameterData_getRetryCount();
}

/**
 * Delegation call for ParametersData.incRetryCount()
 */
int DM_ENG_ParameterManager_incRetryCount()
{
   return DM_ENG_ParameterData_incRetryCount();
}

/*
 * Traite une requête de téléchargement
 */
int DM_ENG_ParameterManager_manageTransferRequest(bool isDownload, unsigned int delay, char* fileType, char* url, char* username, char* password,
        unsigned int fileSize, char* targetFileName, char* successURL, char* failureURL, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult)
{
   int res = 1;
   if (commandKey == NULL) { commandKey = (char*)DM_ENG_EMPTY; } // NULL is reserved for autonomous transfers
   DM_ENG_TransferRequest* tr = DM_ENG_newTransferRequest(isDownload, _NOT_YET_STARTED, time(NULL)+delay, fileType, url, username, password, fileSize, targetFileName, successURL, failureURL, commandKey, NULL);
   DM_ENG_ParameterData_insertTransferRequest(tr);
   time_t start = 0;
   time_t comp = 0;
   if (delay == 0)
   {
      tr->state = _IN_PROGRESS;
      time(&start);
      res = _applyTransferRequest(tr);
      if (res!=1) // requête traitée de façon synchrone (avec succès ou non), on la supprime de la file
      {
         time(&comp);
         DM_ENG_ParameterData_removeTransferRequest(tr->transferId);
      }
   }

   if ((res==0) || (res==1)) // res correspond alors au status
   {
      *pResult = DM_ENG_newTransferResultStruct((DM_ENG_ParameterStatus)res, start, comp);
      res=0;
   }
   return res;
}

/*
 * Construit la table des requêtes de téléchargement dans la file
 */
int DM_ENG_ParameterManager_getAllQueuedTransfers(OUT DM_ENG_AllQueuedTransferStruct** pResult[])
{
   DM_ENG_TransferRequest* tr = DM_ENG_ParameterData_getTransferRequests();
   int nbTr = 0;
   DM_ENG_TransferRequest* curr = tr;
   while (curr != NULL) { nbTr++; curr = curr->next; }
   *pResult = DM_ENG_newTabAllQueuedTransferStruct(nbTr);
   curr = tr;
   int i = 0;
   while (curr != NULL)
   {
      (*pResult)[i++] = DM_ENG_newAllQueuedTransferStruct(curr->isDownload, curr->state, curr->fileType, curr->fileSize, curr->targetFileName, curr->commandKey);
      curr = curr->next;
   }
   return 0;
}

//////////////////////////////////////////////////////
/////////// Gestion des accès concurrents ////////////
//////////////////////////////////////////////////////

// file des notifications de changements par le système
static void processChange(const char* name);
static void processValueChange(const char* name, char* value);
static void processTransferCompleteResult(DM_ENG_TransferRequest* result);
static void processRequestDownload(DM_ENG_DownloadRequest* dr);
static void processVendorSpecificEvent(char* eventCode);

static bool _inSession = false;

/**
 * Verrouille la ressource "paramètres" si disponible
 * @return true si la ressource a pu être verrouillée
 */
static bool tryLock()
{
   DM_CMN_Thread_lockMutex(_dataMutex);
   bool resOk = DM_ENG_ParameterData_tryLock();
   DM_CMN_Thread_unlockMutex(_dataMutex);
   return resOk;
}

/**
 * Comme DM_ENG_ParameterManager_lock(), sans appel au Device Adapter
 */
void DM_ENG_ParameterManager_lockDM()
{
   while (!tryLock())
   {
      DM_CMN_Thread_msleep(500);
   }
}

/**
 * Verrouille la ressource "paramètres"
 * en attendant qu'elle soit rendue disponible si elle ne l'est pas
 */
void DM_ENG_ParameterManager_lock()
{
   if (_dataMutex == NULL) return; // l'accès à la ressource est déjà clos

   DM_ENG_ParameterManager_lockDM();
   DM_ENG_Device_openSession();
   _inSession = true;
}

/**
 * Demande de déverrouillage de la ressource "Paramètres"
 * Si les données ont été modifiées, celles-ci sont tout d'abord sauvegardées.
 * Si la file d'attente des notifications système n'est pas vide, celles-ci sont traitées en priorité.
 */
void DM_ENG_ParameterManager_unlock()
{
   if (_dataMutex == NULL) return; // l'accès à la ressource est déjà clos

   DM_CMN_Thread_lockMutex(_dataMutex);
   if (periodicInformChanged) { _computeNextPeriodicInformTime(); }

   // effacement des valeurs des paramètres SYSTEM_ONLY
   DM_ENG_Parameter* param;
   for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
   {
      DM_ENG_Parameter_clearTemporaryValue(param, 1);
   }

   DM_ENG_ParameterData_sync();
   if (periodicInformChanged)
   {
      DM_ENG_InformMessageScheduler_changeOnPeriodicInform();
      periodicInformChanged = false;
   }

   // on traite prioritairement les changements notifiés par le système avant de déverrouiller
   while (true)
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_NotificationType type = DM_ENG_NONE;
      void* notification = NULL;
      if (pendingChanges != NULL)
      {
         DBG("Processing pending event before unlocking");
         type = pendingChanges->type;
         notification = pendingChanges->notification;
         DM_ENG_SystemNotificationStruct* next = pendingChanges->next;
         DM_ENG_deleteSystemNotificationStruct(pendingChanges);
         pendingChanges = next;
      }
      DM_CMN_Thread_unlockMutex(_changesMutex);

      if (type == DM_ENG_NONE) { break; }

      if (type == DM_ENG_PATH_CHANGE)
      {
         processChange((char*)notification);
         free(notification);
      }
      else if (type == DM_ENG_DATA_VALUE_CHANGE)
      {
         DM_ENG_ParameterValueStruct* pvs = (DM_ENG_ParameterValueStruct*)notification;
         processValueChange(pvs->parameterName, pvs->value);
         DM_ENG_deleteParameterValueStruct(pvs);
      }
      else if (type == DM_ENG_VALUES_UPDATED)
      {
         DM_ENG_ParameterValueStruct** paramList = (DM_ENG_ParameterValueStruct**)notification;
         DM_ENG_ParameterManager_updateParameterValues(paramList);
         DM_ENG_deleteTabParameterValueStruct(paramList);
      }
      else if (type == DM_ENG_SAMPLE_DATA)
      {
         DM_ENG_StatisticsModule_processSampleData((DM_ENG_SampleDataStruct*)notification);
      }
      else if (type == DM_ENG_TRANSFER_COMPLETE)
      {
         processTransferCompleteResult((DM_ENG_TransferRequest*)notification);
      }
      else if (type == DM_ENG_REQUEST_DOWNLOAD)
      {
         DM_ENG_DownloadRequest* dr = (DM_ENG_DownloadRequest*)notification;
         processRequestDownload(dr);
      }
      else if (type == DM_ENG_VENDOR_SPECIFIC_EVENT)
      {
         processVendorSpecificEvent((char*)notification);
      }
      DM_ENG_ParameterData_sync();
   }

   DBG("Unlocking...");

   // contrôle des changements sur les paramètres computed
   char* prmName;
   for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
   {
      param = DM_ENG_ParameterData_getCurrent();
      if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) && (!DM_ENG_Parameter_isValueChanged(param) || (param->value != NULL)))
      {
         char* resumeName = strdup(prmName);
         _checkComputedChanged(param);
         DM_ENG_ParameterManager_resumeParameterLoopFrom(resumeName);
         DM_ENG_FREE(resumeName);
      }
   }
   DM_ENG_ParameterData_sync();
   
   while (diagnosticsRequested != NULL)
   {
      bool requested = (diagnosticsRequested->type == 1);
      _razDiagnosticsResult(diagnosticsRequested->parameterName, requested);
      if (requested)
      {
         DM_ENG_Diagnostics_initDiagnostics(diagnosticsRequested->parameterName);
      }
      DM_ENG_ParameterValueStruct* next = diagnosticsRequested->next;
      DM_ENG_deleteParameterValueStruct(diagnosticsRequested);
      diagnosticsRequested = next;
   }
   if (acsParametersToUpdate)
   {
      DM_ENG_NotificationInterface_updateAcsParameters();
      acsParametersToUpdate = false;
   }
   if (_inSession)
   {
      DM_ENG_Device_closeSession();
      _inSession = false;
   }
   DM_ENG_ParameterData_unLock();
   DM_CMN_Thread_unlockMutex(_dataMutex);
}

/*
 * Lance un reboot si demandé (cette fonction doit être appelée hors session ou en fin de session)
 */
void DM_ENG_ParameterManager_checkReboot()
{
   char* ck = DM_ENG_ParameterData_getRebootCommandKey();
   if (ck != NULL)
   {
      DM_ENG_NotificationInterface_deactivateAll();
      DM_ENG_Device_reboot(*ck=='*');
   }
}

/**
 * Cette méthode doit être appelée lorsque les data sont verrouillées.
 * Elle permet la sauvegarde des données avant déverrouillage au cas où on aurait un plantage
 * ou un redémarrage avant un unlock().
 * 
 * Dans le même temps, les valeurs calculées sont effacées. Cette fct étant appelée après les modifs apportées au data model (via les RPC
 * SetParameterValues, AddObject etc.), cela permet de réactualiser les valeurs lors d'un prochain accès par GetParameterValues.
 */
void DM_ENG_ParameterManager_sync(bool resetComputed)
{
   if (resetComputed)
   {
      // effacement des valeurs calculées
      DM_ENG_Parameter* param;
      for (param = DM_ENG_ParameterData_getFirst(); param!=NULL; param = DM_ENG_ParameterData_getNext())
      {
         DM_ENG_Parameter_clearTemporaryValue(param, 0);
      }
   }

   DM_ENG_ParameterData_sync();
}

//////////////////////////////////////////////////////////////
/////////// Implémentation ParameterBackInterface ////////////
//////////////////////////////////////////////////////////////

static bool _checkValueChange(DM_ENG_Parameter* param)
{
   bool res = false;
   if (DM_ENG_Parameter_isValuable(param->name))
   {
      if (param->value == NULL) // inconnue => on considère qu'on a un changement
      {
         res = true;
         DM_ENG_Parameter_updateValue(param, NULL, true); // efface la valeur et positionne VALUE_CHANGED
      }
      else if (param->storageMode != DM_ENG_StorageMode_DM_ONLY)
      {
         char* actualValue;
         char* data = NULL;
         if ((param->definition != NULL) && (strlen(param->definition) != 0))
         {
            DM_ENG_ComputedExp_eval(param->definition, _getValue, param->name, &data);
         }
         int codeRetour = DM_ENG_Device_getValue(param->name, data, &actualValue);
         if (codeRetour==0)
         {
            if (actualValue==NULL) { actualValue = (char*)DM_ENG_EMPTY; } // NULL interprété comme Empty
            if (strcmp(actualValue, param->value) != 0)
            {
               res = true;
               _updateSystemValue(param, actualValue, true);
            }
         }
         DM_ENG_FREE(data);
         DM_ENG_FREE(actualValue);
      }
   }
   return res;
}

/**
 * @param name Parameter name. If the name ends with ".", this is a notification by group. In this case,
 * value changes are checked for all parameters in this group.
 */
static void processChange(const char* name)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if (param != NULL)
   {
      if (DM_ENG_Parameter_isNode(param->name))
      {
         char* prmName;
         for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
         {
            if (strncmp(prmName, name, strlen(name))==0)
            {
               _checkValueChange(DM_ENG_ParameterData_getCurrent());
               // si changement (resultat true) et ACS URL, setBootstrapSent(false);
            }
         }
      }
      else
      {
         // utile si l'URL est stocké au niveau système
         // if (name.equals(ACS_URL_PARAMETER_NAME)) { setBootstrapSent(false); }
         _checkValueChange(param);
      }
      DM_ENG_ParameterData_sync();
   }
}

/**
 * @param name Name of the parameter which changed. If the name ends with ".", this is a notification by group.
 * In this case, value changes are checked for all parameters in this group.
 */
void DM_ENG_ParameterManager_dataValueChanged(const char* name)
{
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("DataValueChanged - Begin");
      processChange(name);
      DBG("DataValueChanged - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place les noms de paramètres dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_PATH_CHANGE, strdup(name)));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/**
 * @param name Parameter name. The name represents necessarely a single parameter (it does not end with ".")
 * @param value Parameter value.
 */
static void processValueChange(const char* name, char* value)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if ((param != NULL) && !DM_ENG_Parameter_isNode(param->name))
   {
      _updateSystemValue(param, value, true);
      DM_ENG_ParameterData_sync();
   }
}

/**
 * @param name Name of the parameter which changed
 * @param value New value
 */
void DM_ENG_ParameterManager_dataNewValue(const char* name, char* value)
{
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("DataNewValue - Begin");
      processValueChange(name, value);
      DBG("DataNewValue - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_DATA_VALUE_CHANGE, DM_ENG_newParameterValueStruct(name, DM_ENG_ParameterType_UNDEFINED, value)));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

void DM_ENG_ParameterManager_updateParameterNames(const char* name, DM_ENG_ParameterValueStruct* paramList[])
{
   if (paramList != NULL)
   {
      // ajoute les noeuds nouveaux
      int j;
      for (j = 0; paramList[j] != NULL; j++)
      {
         _getParameter(paramList[j]->parameterName);
      }

      // supprime les noeuds obsolètes
      int len = strlen(name);
      char* aSupprimer = NULL;
      char* prmName;

      do
      {
         for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
         {
            int prmLen = strlen(prmName);
            if ((prmLen>len) && (strncmp(prmName, name, len)==0) && (prmName[prmLen-1] == '.') && (strstr(prmName, "..") == 0)
              && DM_ENG_Parameter_isNodeInstance(prmName))
              // filtre les sous-noeuds d'instance de name qui ne soient pas des protos
            {
               int i;
               bool trouve = false;
               for (i=0; !trouve && (paramList[i]!=NULL); i++)
               {
                  trouve = (strncmp(prmName, paramList[i]->parameterName, prmLen)==0);
               }
               if (!trouve)
               {
                  // si c'est un objet de type instance qui n'a pas été trouvé, on peut le supprimer
                  // (Les paramètres fixes du data model même s'ils ne sont pas remontés ne sont pas supprimés)
                  aSupprimer = strdup(prmName);
                  break;
               }
            }
         }
         if (aSupprimer == NULL) break;

         DM_ENG_ParameterData_deleteObject(aSupprimer);
         DM_ENG_FREE(aSupprimer);
      }
      while (true); // dans le cas où il y a eu un objet supprimé, on recommence
   }
}

void DM_ENG_ParameterManager_updateParameterValues(DM_ENG_ParameterValueStruct* paramList[])
{
   if (paramList != NULL)
   {
      int j;
      for (j = 0; paramList[j] != NULL; j++)
      {
         char* name = (char*)paramList[j]->parameterName;
         if (DM_ENG_Parameter_isNode(name))
         {
            // Si cet élement de la liste a un nom d'objet, on traite la demande, à partir de cet élément
            // comme une mise à jour de l'objet (avec ajouts et suppressions possibles d'instances)
            _updateObject(name, paramList+j+1, true);
            DM_ENG_Parameter_updateValue(DM_ENG_ParameterData_getParameter(name), NULL, true); // efface la valeur et positionne VALUE_CHANGED
            break;
         }
         else
         {
            DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
            if (param != NULL)
            {
               _updateSystemValue(param, paramList[j]->value, true);
            }
         }
      }
      DM_ENG_ParameterData_sync();
   }
}

/**
 * Notify the DM Engine that parameter values are updated.
 *
 * @param parameterList List of name-value pairs that are updated
 */
void DM_ENG_ParameterManager_parameterValuesUpdated(DM_ENG_ParameterValueStruct* parameterList[])
{
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("ParameterValuesUpdated - Begin");
      DM_ENG_ParameterManager_updateParameterValues(parameterList);
      DBG("ParameterValuesUpdated - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_VALUES_UPDATED, DM_ENG_copyTabParameterValueStruct(parameterList)));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/*
 * traite le TransferComplete
 */
static void processTransferCompleteResult(DM_ENG_TransferRequest* result)
{
   bool autonomous = true;
   if (result->transferId == 0)
   {
      DM_ENG_ParameterData_insertTransferRequest(result);
   }
   else
   {
      DM_ENG_TransferRequest* updated = DM_ENG_ParameterData_updateTransferRequest(result->transferId, _COMPLETED, result->faultCode, result->startTime, result->completeTime);
      autonomous = (updated->commandKey == NULL);
      DM_ENG_deleteTransferRequest(result);

      if (updated->faultCode == 0)
      {
         updated->faultCode = _actionAfterDownload(updated->fileType, updated->targetFileName);
      }
   }
   DM_ENG_ParameterData_sync();
   DM_ENG_InformMessageScheduler_transferComplete(autonomous);
}

/**
 * Notify the DM Engine that a transfer operation completes.
 *
 * @param transferId An integer that uniquely identifies the transfer request in the queue of transfers
 * @param code Code résultat du téléchargement : 0 si OK, code d'erreur 90XX sinon
 * @param startTime Date de démarrage du téléchargement
 * @param completeTime Date de fin du téléchargement
 */
void DM_ENG_ParameterManager_transferComplete(unsigned int transferId, unsigned int code, time_t startTime, time_t completeTime)
{
   DM_ENG_TransferRequest* result = DM_ENG_newTransferRequest(true, _COMPLETED, 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL);
   result->transferId = transferId;
   DM_ENG_updateTransferRequest(result, _COMPLETED, code, startTime, completeTime);
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("TransferComplete - Begin");
      processTransferCompleteResult(result);
      DBG("TransferComplete - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_TRANSFER_COMPLETE, result));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/**
 * Notify the DM Engine that an autonomous transfer is in progress
 *
 * @param announceURL The URL on which the CPE listened to the announcements
 * @param transferURL URL specifying the source or destination location
 * @param isDownload Direction of the transfer : true = download, false = upload
 * @param fileType File type as specified in TR-069
 * @param fileSize Size of the file to be transfered in bytes
 * @param targetFileName File name to be used on the target file system
 *
 * @return A numerical transferId that must be used to notify the transfer completion
 *
 * N.B. This call is processed synchronously to return the transferId (It waits until the engine is available)
 */
unsigned int DM_ENG_ParameterManager_autonomousTransferInProgress(char* announceURL, char* transferURL, bool isDownload, char* fileType, unsigned int fileSize, char* targetFileName)
{
   DM_ENG_TransferRequest* result = DM_ENG_newTransferRequest(isDownload, _IN_PROGRESS, 0, fileType, transferURL, NULL, NULL, fileSize, targetFileName, NULL, NULL, NULL, announceURL);
   DM_ENG_ParameterManager_lockDM();
   DM_ENG_ParameterData_insertTransferRequest(result);
   DM_ENG_ParameterData_sync();
   DM_ENG_ParameterManager_unlock();
   return result->transferId;
}

/**
 * Notify the DM Engine that an autonomous transfer is in progress
 *
 * @param announceURL The URL on which the CPE listened to the announcements
 * @param transferURL URL specifying the source or destination location
 * @param isDownload Direction of the transfer : true = download, false = upload
 * @param fileType File type as specified in TR-069
 * @param fileSize Size of the file to be transfered in bytes
 * @param targetFileName File name to be used on the target file system
 * @param code 0 if the transfer was successful, otherwize à fault code
 * @param startTime Date and time the transfer was started in UTC
 * @param completeTime Date and time the transfer was completed and applied in UTC
 */
void DM_ENG_ParameterManager_autonomousTransferComplete(char* announceURL, char* transferURL, bool isDownload, char* fileType, unsigned int fileSize, char* targetFileName, unsigned int code, time_t startTime, time_t completeTime)
{
   DM_ENG_TransferRequest* result = DM_ENG_newTransferRequest(isDownload, _COMPLETED, 0, fileType, transferURL, NULL, NULL, fileSize, targetFileName, NULL, NULL, NULL, announceURL);
   DM_ENG_updateTransferRequest(result, _COMPLETED, code, startTime, completeTime);
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("AutonomousTransferComplete - Begin");
      processTransferCompleteResult(result);
      DBG("AutonomousTransferComplete - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_TRANSFER_COMPLETE, result));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/*
 * traite le Request Download
 */
static void processRequestDownload(DM_ENG_DownloadRequest* dr)
{
   DM_ENG_ParameterData_addDownloadRequest(dr);
   DM_ENG_ParameterData_sync();
   DM_ENG_InformMessageScheduler_requestDownload();
}

/**
 * Notify the DM Engine that a download is requested
 *
 * @param fileType The file type
 * @param args Name-value pairs
 */
void DM_ENG_ParameterManager_requestDownload(const char* fileType, DM_ENG_ArgStruct* args[])
{
   DM_ENG_DownloadRequest* dr = DM_ENG_newDownloadRequest(fileType, args);
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("RequestDownload - Begin");
      processRequestDownload(dr);
      DBG("RequestDownload - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_REQUEST_DOWNLOAD, dr));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/*
 * traite le Vendor Specific Event
 */
static void processVendorSpecificEvent(char* eventCode)
{
   DM_ENG_InformMessageScheduler_vendorSpecificEvent(eventCode);
   free( eventCode );
}

/**
 * @param OUI Organizationally Unique Identifier
 * @param event Name of an event
 */
void DM_ENG_ParameterManager_vendorSpecificEvent(const char* OUI, const char* event)
{
   char* eventCode = (char*)calloc(strlen(OUI) + strlen(event) + 4, sizeof(char));
   strcpy( eventCode, "X " );
   strcat( eventCode, OUI );
   strcat( eventCode, " " );
   strcat( eventCode, event );
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("VendorSpecificEvent - Begin");
      processVendorSpecificEvent(eventCode);
      DBG("VendorSpecificEvent - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_VENDOR_SPECIFIC_EVENT, eventCode));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}

/**
 * Permet la synchronisation sur l'arrêt du scheduler
 */
void DM_ENG_ParameterManager_join()
{
   DM_ENG_InformMessageScheduler_join();
}

/**
 *
 */
void DM_ENG_ParameterManager_sampleData(DM_ENG_SampleDataStruct* sampleData)
{
   bool acquired = tryLock();
   if (acquired)
   {
      DBG("SampleData - Begin");
      DM_ENG_StatisticsModule_processSampleData(sampleData);
      DBG("SampleData - End");
      DM_ENG_ParameterManager_unlock();
   }
   else // on place le couple (nom, valeur) dans une file d'attente
   {
      DM_CMN_Thread_lockMutex(_changesMutex);
      DM_ENG_addSystemNotificationStruct(&pendingChanges, DM_ENG_newSystemNotificationStruct(DM_ENG_SAMPLE_DATA, sampleData));
      DM_CMN_Thread_unlockMutex(_changesMutex);
   }
}
