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
 * File        : DM_ENG_NotificationInterface.c
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
 * @file DM_ENG_NotificationInterface.c
 *
 * @brief 
 *
 **/
 
#include "DM_ENG_NotificationInterface.h"
#include "DM_ENG_ParameterManager.h"
#include "DM_ENG_Global.h"

static char** mandatoryParameterList = NULL;
static DM_ENG_NotificationInterface* listeners[2] = { NULL, NULL };

static DM_ENG_F_UPDATE_ACS_PARAMETERS _updateAcsParameters = NULL;

void DM_ENG_NotificationInterface_activate(DM_ENG_EntityType entity, DM_ENG_F_INFORM inform, DM_ENG_F_TRANSFER_COMPLETE transferComplete, DM_ENG_F_REQUEST_DOWNLOAD requestDownload, DM_ENG_F_UPDATE_ACS_PARAMETERS updateAcsParameters)
{
   DM_ENG_NotificationInterface_deactivate(entity); // Au cas où une notif antérieure serait encore enregistrée
   switch (entity)
   {
      case DM_ENG_EntityType_ACS :
      case DM_ENG_EntityType_SELFCARE_APP :
         listeners[entity] = (DM_ENG_NotificationInterface*) malloc(sizeof(DM_ENG_NotificationInterface));
         listeners[entity]->knownParameters = NULL;
         listeners[entity]->knownParametersToRemove = NULL;
         listeners[entity]->inform = inform;
         listeners[entity]->transferComplete = transferComplete;
         listeners[entity]->requestDownload = requestDownload;
         break;
      case DM_ENG_EntityType_SYSTEM :
      case DM_ENG_EntityType_SUBSCRIBER :
      case DM_ENG_EntityType_ANY :
         break;
   }

   if (updateAcsParameters != NULL) { _updateAcsParameters = updateAcsParameters; }
}

void DM_ENG_NotificationInterface_deactivateAll()
{
   DM_ENG_NotificationInterface_deactivate(DM_ENG_EntityType_ACS);
   DM_ENG_NotificationInterface_deactivate(DM_ENG_EntityType_SELFCARE_APP);
   if (mandatoryParameterList != NULL)
   {
      int i;
      for (i=0; mandatoryParameterList[i] != NULL; i++) { free(mandatoryParameterList[i]); }      
      free(mandatoryParameterList);
      mandatoryParameterList = NULL;
   }
}

void DM_ENG_NotificationInterface_deactivate(DM_ENG_EntityType entity)
{
   if (listeners[entity] != NULL)
   {
      switch (entity)
      {
         case DM_ENG_EntityType_ACS :
         case DM_ENG_EntityType_SELFCARE_APP :
            DM_ENG_deleteAllParameterValueStruct(&listeners[entity]->knownParameters);
            DM_ENG_deleteAllParameterName(&listeners[entity]->knownParametersToRemove);
            free(listeners[entity]);
            listeners[entity] = NULL;
            break;
         case DM_ENG_EntityType_SYSTEM :
         case DM_ENG_EntityType_SUBSCRIBER :
         case DM_ENG_EntityType_ANY :
            break;
      }
   }
}

void DM_ENG_NotificationInterface_updateAcsParameters()
{
   if (_updateAcsParameters != NULL)
   {
      // Provide to DM_COM the updated ACS parameters
      char* url = NULL;
      char* username = NULL;
      char* password = NULL;
      DM_ENG_ParameterManager_getParameterValue(DM_TR106_ACS_URL, &url);
      DM_ENG_ParameterManager_getParameterValue(DM_TR106_ACS_USERNAME, &username);
      DM_ENG_ParameterManager_getParameterValue(DM_TR106_ACS_PASSWORD, &password);
      (_updateAcsParameters)(url, username, password);
      DM_ENG_FREE(url);
      DM_ENG_FREE(username);
      DM_ENG_FREE(password);
   }
}

bool DM_ENG_NotificationInterface_isReady()
{
   return (listeners[0] != NULL) || (listeners[1] != NULL);
}

/**
 * @param mpl The mandatory parameter list.
 * @param cpl The request parameter settings.
 */
void DM_ENG_NotificationInterface_setKnownParameterList(DM_ENG_EntityType entity, DM_ENG_ParameterValueStruct* parameterList[])
{
   if ((entity > 1) || (listeners[entity]==NULL)) return;
   if (mandatoryParameterList == NULL) { mandatoryParameterList = DM_ENG_ParameterManager_getMandatoryParameters(); }
   DM_ENG_deleteAllParameterValueStruct(&listeners[entity]->knownParameters);
   int i;
   for (i=0; parameterList[i] != NULL; i++)
   {
      if (DM_ENG_ParameterManager_isInformParameter(parameterList[i]->parameterName))
      {
         DM_ENG_addParameterValueStruct(&listeners[entity]->knownParameters, DM_ENG_newParameterValueStruct(parameterList[i]->parameterName, parameterList[i]->type, parameterList[i]->value));
      }
   }
   for (i=0; mandatoryParameterList[i] != NULL; i++)
   {
      DM_ENG_ParameterValueStruct* pvs = listeners[entity]->knownParameters;
      while (pvs != NULL)
      {
         if (strcmp(mandatoryParameterList[i], pvs->parameterName)==0) break;
         pvs = pvs->next;
      }
      if (pvs == NULL) // pas déjà présent, on le rajoute
      {
         DM_ENG_Parameter* param = DM_ENG_ParameterManager_getParameter(mandatoryParameterList[i]);
         if (param==NULL) continue; // cas normalement impossible
         DM_ENG_ParameterManager_loadLeafParameterValue(param, false);
         param = DM_ENG_ParameterManager_getParameter(mandatoryParameterList[i]); // pour repositionner le param courant pouvant être modifié par le refresh
         DM_ENG_addParameterValueStruct(&listeners[entity]->knownParameters, DM_ENG_newParameterValueStruct(param->name, param->type, param->value));
      }
   }
}

void DM_ENG_NotificationInterface_clearKnownParameters(DM_ENG_EntityType entity)
{
   DM_ENG_ParameterName* pn = listeners[entity]->knownParametersToRemove;
   if (pn != NULL)
   {
      do {
         DM_ENG_ParameterValueStruct* pvsPrec = NULL;
         DM_ENG_ParameterValueStruct* pvs = listeners[entity]->knownParameters;
         while (pvs != NULL)
         {
            if (strcmp(pvs->parameterName, pn->name) == 0)
            {
               if (pvsPrec == NULL) { listeners[entity]->knownParameters = pvs->next; }
               else { pvsPrec->next = pvs->next; }
               DM_ENG_deleteParameterValueStruct(pvs);
               break;
            }
            pvsPrec = pvs;
            pvs = pvs->next;
         }
         pn = pn->next;
      } while (pn != NULL);
      DM_ENG_deleteAllParameterName(&listeners[entity]->knownParametersToRemove);
   }
//   else printf("knownParametersToRemove==NULL Ca arrive !\n");
}

/*
 *
 */
DM_ENG_NotificationStatus DM_ENG_NotificationInterface_inform(DM_ENG_DeviceIdStruct* id, DM_ENG_EventStruct* events[], DM_ENG_ParameterValueStruct* parameterList[], time_t currentTime, unsigned int retryCount)
{
   DM_ENG_NotificationStatus status = DM_ENG_CANCELLED;
   int entity;
   for (entity=0; entity<2; entity++)
   {
      if ((listeners[entity] != NULL) && (listeners[entity]->inform != NULL))
      {
         DM_ENG_NotificationStatus res = DM_ENG_CANCELLED;
         if (listeners[entity]->knownParameters != NULL)
         {
            DM_ENG_EventStruct** newEvents = events;
            bool change = false;
            listeners[entity]->knownParametersToRemove = NULL;
            DM_ENG_ParameterValueStruct** newParameterList = (DM_ENG_ParameterValueStruct**)calloc(DM_ENG_tablen((void**)parameterList)+1, sizeof(DM_ENG_ParameterValueStruct*));
            int iNewParameterList = 0;
            int i;
            for (i = 0; parameterList[i] != NULL; i++)
            {
               const char* name = parameterList[i]->parameterName;
               char* newVal = parameterList[i]->value;
               char* knownVal = NULL;
               bool isKnown = false;
               DM_ENG_ParameterValueStruct* pvs = listeners[entity]->knownParameters;
               while (pvs != NULL)
               {
                  if (strcmp(pvs->parameterName, name) == 0)
                  {
                     isKnown = true;
                     knownVal = pvs->value;
                     break;
                  }
                  pvs = pvs->next;
               }
               if (isKnown)
               {
                  bool isMandatory = false;
                  int k;
                  for (k=0; !isMandatory && (mandatoryParameterList[k]!=NULL); k++)
                  {
                     isMandatory = (strcmp(mandatoryParameterList[k], name) == 0);
                  }
                  if (isMandatory)
                  {
                     if (!change && (strcmp(name, DM_ENG_PARAMETER_KEY_NAME) != 0))
                     {
                        change = ((newVal==NULL) || (knownVal==NULL) ? (newVal!=knownVal) : (strcmp(newVal, knownVal)!=0));
                     }
                     newParameterList[iNewParameterList++] = parameterList[i];
                  }
                  else
                  {
                     if ((newVal==NULL) || (knownVal==NULL) ? (newVal!=knownVal) : (strcmp(newVal, knownVal)!=0))
                     {
                        change = true;
                        newParameterList[iNewParameterList++] = parameterList[i];
                     }
                     DM_ENG_addParameterName(&listeners[entity]->knownParametersToRemove, DM_ENG_newParameterName(name)); // dupliquer name ? !!
                  }
               }
               else
               {
                  change = true;
                  newParameterList[iNewParameterList++] = parameterList[i];
               }
            }
            int nbKnown = 0;
            DM_ENG_ParameterValueStruct* pv = listeners[entity]->knownParameters;
            while (pv != NULL) { nbKnown++; pv = pv->next; }
            if (nbKnown <= DM_ENG_tablen((void**)mandatoryParameterList))
               // ttes les modif demandées (sur des paramètres notifiables) sont remontées
            {
               DM_ENG_deleteAllParameterValueStruct(&listeners[entity]->knownParameters);
            }
            if (change || (DM_ENG_tablen((void**)events)>1) || (strcmp(events[0]->eventCode, DM_ENG_EVENT_VALUE_CHANGE)!=0))
               // si changement ou événement non limité à VALUE_CHANGE_EVENT
            {
               if (!change)
                  // si pas de changement, supprimer le VALUE_CHANGE_EVENT si présent
               {
                  int k = -1;
                  int i;
                  for (i=0; events[i]!=NULL; i++)
                  {
                     if (strcmp(events[i]->eventCode, DM_ENG_EVENT_VALUE_CHANGE)==0)
                     {
                        k = i;
                        break;
                     }
                  }
                  if (k!=-1)
                  {
                     int nbNew = DM_ENG_tablen((void**)events)-1;
                     newEvents = (DM_ENG_EventStruct**)calloc(nbNew+1, sizeof(DM_ENG_EventStruct*));
                     int i;
                     for (i=0; i<nbNew; i++)
                     {
                        newEvents[i] = events[i<k ? i : i+1];
                     }
                  }
               }
               res = (DM_ENG_NotificationStatus)(listeners[entity]->inform)(id, newEvents, newParameterList, currentTime, retryCount);
               if (newEvents != events) { free(newEvents); }
            }
            else // pas d'Inform Message à envoyer
            {
               res = DM_ENG_COMPLETED;
            }
            free(newParameterList);
            if (res == DM_ENG_COMPLETED) { DM_ENG_NotificationInterface_clearKnownParameters((DM_ENG_EntityType)entity); }
         }
         else
         {
            res = (DM_ENG_NotificationStatus)(listeners[entity]->inform)(id, events, parameterList, currentTime, retryCount);
         }
         if ((DM_ENG_EntityType)entity==DM_ENG_EntityType_ACS) { status = res; } // seul le résultat de l'envoi vers l'ACS compte
      }
   }
   return status;
}

DM_ENG_NotificationStatus DM_ENG_NotificationInterface_transferComplete(DM_ENG_TransferCompleteStruct* tcs)
{
   DM_ENG_NotificationStatus status = DM_ENG_CANCELLED;
   int entity;
   for (entity=0; entity<2; entity++)
   {
      if ((listeners[entity]!=NULL) && (listeners[entity]->transferComplete!=NULL))
      {
         DM_ENG_NotificationStatus res = (DM_ENG_NotificationStatus)(listeners[entity]->transferComplete)(tcs);
         if ((DM_ENG_EntityType)entity==DM_ENG_EntityType_ACS) { status = res; } // seul le résultat de l'envoi vers l'ACS compte
      }
   }
   return status;
}

DM_ENG_NotificationStatus DM_ENG_NotificationInterface_requestDownload(const char* fileType, DM_ENG_ArgStruct* args[])
{
   DM_ENG_NotificationStatus status = DM_ENG_CANCELLED;
   int entity;
   for (entity=0; entity<2; entity++)
   {
      if ((listeners[entity]!=NULL) && (listeners[entity]->requestDownload!=NULL))
      {
         DM_ENG_NotificationStatus res = (DM_ENG_NotificationStatus)(listeners[entity]->requestDownload)(fileType, args);
         if (entity==DM_ENG_EntityType_ACS) { status = res; } // seul le résultat de l'envoi vers l'ACS compte
      }
   }
   return status;
}
