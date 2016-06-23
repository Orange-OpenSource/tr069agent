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
 * File        : DM_ENG_RPCInterface.c
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
 * @file DM_ENG_RPCInterface.c
 *
 * @brief The RPC Interface.
 * 
 * This header file defines the RPC Interface which provides similar functions to the TR-069 RPC methods, plus
 * somme functions for the notification by Inform messages and for the session management.  
 *
 */

#include "DM_ENG_RPCInterface.h"
#include "DM_ENG_ParameterManager.h"
#include "DM_ENG_NotificationInterface.h"
#include "DM_ENG_InformMessageScheduler.h"
#include "DM_ENG_Device.h"
#include "DM_ENG_Global.h"
#include "CMN_Trace.h"

static DM_ENG_EntityType _currentEntity = (DM_ENG_EntityType)-1; // Entity which has locked the access to the parameter manager 

static bool _toLockForOneCall(DM_ENG_EntityType entity)
{
   return ( ((entity != DM_ENG_EntityType_ACS) || !DM_ENG_InformMessageScheduler_isSessionOpened(true))
          && (entity != _currentEntity) );
}

/**
 * Activates the notification.
 * 
 * @param entity Entity number
 * @param inform Inform callback
 * @param transferComplete callback
 */
void DM_ENG_ActivateNotification(DM_ENG_EntityType entity, DM_ENG_F_INFORM inform, DM_ENG_F_TRANSFER_COMPLETE transferComplete, DM_ENG_F_REQUEST_DOWNLOAD requestDownload, DM_ENG_F_UPDATE_ACS_PARAMETERS updateAcsParameters)
{
   DM_ENG_NotificationInterface_activate(entity, inform, transferComplete, requestDownload, updateAcsParameters);
}

/**
 * Deactivate the notification
 * @param entity Entity number
 */
void DM_ENG_DeactivateNotification(DM_ENG_EntityType entity)
{
   DM_ENG_NotificationInterface_deactivate(entity);
}

/**
 * Indicates that the session is opened.
 * 
 * @param entity Entity number
 */
void DM_ENG_SessionOpened(DM_ENG_EntityType entity)
{
   if ((entity==DM_ENG_EntityType_ACS) && DM_ENG_InformMessageScheduler_isSessionOpened(true))
   {
      DM_ENG_ParameterManager_sync(false); // Les événements et données de l'Inform Message ont bien été transmis à l'ACS,
                                           // on peut valider leur suppression des données persistantes
   }
}

/**
 * Asks if the DM Engine is ready to close the session (No more transfer complete to send)
 * 
 * @param entity Entity number
 * 
 * @return True if the DM Engine is ready, false otherwise
 */
bool DM_ENG_IsReadyToClose(DM_ENG_EntityType entity)
{
   bool res = true;
   if ((entity==DM_ENG_EntityType_ACS) && DM_ENG_InformMessageScheduler_isSessionOpened(true))
   {
      res = DM_ENG_InformMessageScheduler_isReadyToClose();
   }
   return res;
}

/**
 * Notify the DM Engine that the session is closed.
 * 
 * @param entity Entity number
 * @param success True if the session succeeded, false otherwise
 */
void DM_ENG_SessionClosed(DM_ENG_EntityType entity, bool success)
{
   if (entity==DM_ENG_EntityType_ACS)
   {
      DM_ENG_InformMessageScheduler_sessionClosed(success);
      DM_ENG_ParameterManager_checkReboot();
   }
}

const char* _methods[MAX_SUPPORTED_RPC_METHODS+1];

/**
 * Returns Array of strings containing the names of each of the RPC methods the CPE supports.
 * 
 * @param entity Entity number
 * @param pMethods Pointer to null terminated array to get the result 
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_GetRPCMethods(DM_ENG_EntityType entity UNUSED, OUT const char** pMethods[])
{
   int i=0;
   if (DM_ENG_IS_GETRPCMETHODS_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETRPCMETHODS;
     i++;
   }
   if (DM_ENG_IS_SETPARAMETERVALUES_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_SETPARAMETERVALUES;
     i++;
   }   
   if (DM_ENG_IS_GETPARAMETERVALUES_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETPARAMETERVALUES;
     i++;
   }   
   if (DM_ENG_IS_GETPARAMETERNAMES_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETPARAMETERNAMES;
     i++;
   } 
   if (DM_ENG_IS_SETPARAMETERATTRIBUTES_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_SETPARAMETERATTRIBUTES;
     i++;
   }   
   if (DM_ENG_IS_GETPARAMETERATTRIBUTES_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETPARAMETERATTRIBUTES;
     i++;
   }   
   if (DM_ENG_IS_ADDOBJECT_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_ADDOBJECT;
     i++;
   }
   if (DM_ENG_IS_DELETEOBJECT_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_DELETEOBJECT;
     i++;
   }
   if (DM_ENG_IS_REBOOT_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_REBOOT;
     i++;
   }
   if (DM_ENG_IS_DOWNLOAD_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_DOWNLOAD;
     i++;
   }   
   if (DM_ENG_IS_UPLOAD_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_UPLOAD;
     i++;
   }   
   if (DM_ENG_IS_FACTORYRESET_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_FACTORYRESET;
     i++;
   }
   if (DM_ENG_IS_GETQUEUEDTRANSFERS_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETQUEUEDTRANSFERS;
     i++;
   }
   if (DM_ENG_IS_GETALLQUEUEDTRANSFERS_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_GETALLQUEUEDTRANSFERS;
     i++;
   }
   if (DM_ENG_IS_SCHEDULEINFORM_SUPPORTED) {
     _methods[i] = SUPPORTED_RPC_SCHEDULEINFORM;
     i++;
   }
   _methods[i] = NULL;
   *pMethods = _methods;
   return 0;
}

/**
 * This function may be used by the ACS to modify the value of one or more CPE Parameters.
 *
 * @param entity Entity number
 * 
 * @param parameterList Array of name-value pairs as specified. For each name-value pair, the CPE is instructed
 * to set the Parameter specified by the name to the corresponding value.
 * 
 * @param parameterKey The value to set the ParameterKey parameter. This MAY be used by the server to identify
 * Parameter updates, or left empty.
 * 
 * @param pStatus Pointer to get the status defined as follows :
 * - DM_ENG_ParameterStatus_APPLIED (=0) : Parameter changes have been validated and applied.
 * - DM_ENG_ParameterStatus_READY_TO_APPLY (=1) : Parameter changes have been validated and committed, but not yet applied
 * (for example, if a reboot is required before the new values are applied).
 * - DM_ENG_ParameterStatus_UNDEFINED : Undefined status (for example, parameterList is empty).
 *
 *  Note : The result is DM_ENG_ParameterStatus_APPLIED if all the parameter changes are APPLIED.
 * The result is DM_ENG_ParameterStatus_READY_TO_APPLY if one or more parameter changes is READY_TO_APPLY.
 * 
 * @param pFaults Pointer to get the faults struct if any, as specified in TR-069
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_SetParameterValues(DM_ENG_EntityType entity, DM_ENG_ParameterValueStruct* parameterList[], char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus, OUT DM_ENG_SetParameterValuesFault** pFaults[])
{
   int codeRetour = 0;
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   DM_ENG_NotificationInterface_setKnownParameterList(entity, parameterList);
	*pStatus = DM_ENG_ParameterStatus_UNDEFINED;
	DM_ENG_SetParameterValuesFault* faultsList = NULL;
	int nbFaults = 0;
	int res = 0;
   if (entity == DM_ENG_EntityType_SYSTEM)
   {
      // Si c'est un appel de l'entité SYSTEM alors cet appel est traité comme une mise à jour système (émise de l'API basse)
      DM_ENG_ParameterManager_updateParameterValues(parameterList);
      *pStatus = DM_ENG_ParameterStatus_APPLIED;
   }
   else
   {
      int size = 0;
      while (parameterList[size] != NULL) { size++; }
      DM_ENG_SystemParameterValueStruct** systemParameterList = DM_ENG_newTabSystemParameterValueStruct(size);
      int iSysPrm = 0;
      int i;
   	for (i=0; i<size; i++)
   	{
         char* name = (char*)parameterList[i]->parameterName;
         DM_ENG_ParameterStatus stat = DM_ENG_ParameterStatus_UNDEFINED;
         char* systemName = NULL;
         char* systemData = NULL;
         DBG("parameterName = %s", name);
         res = DM_ENG_ParameterManager_loadSystemParameters(name, false);
         if (res == 0)
         {
   	      DBG("parameterName = %s, type = %d, Value = %s", name, parameterList[i]->type, parameterList[i]->value);
            res = DM_ENG_ParameterManager_setParameterValue( entity, name, parameterList[i]->type, parameterList[i]->value, &stat, &systemName, &systemData);
         }
         if ((res == 0) || (res == 1))
         {
            if (*pStatus != DM_ENG_ParameterStatus_READY_TO_APPLY)
            {
               *pStatus = stat;
            }
            if (res == 1)
            {
               systemParameterList[iSysPrm++] = DM_ENG_newSystemParameterValueStruct(systemName, parameterList[i]->value, systemData);
            }
         }
         else
         {
            DBG("Error res != 0");
            DM_ENG_addSetParameterValuesFault(&faultsList, DM_ENG_newSetParameterValuesFault(name, res));
            nbFaults++;
            // break; !!
         }
         DM_ENG_FREE(systemData);
      }
      if ((nbFaults == 0) && (iSysPrm > 0))
      {
         res = DM_ENG_Device_setValues(systemParameterList);
         if (res != 0)
         {
            DBG("Error res != 0");
            nbFaults = iSysPrm;
            for (i=0; i<nbFaults; i++)
            {
               DM_ENG_addSetParameterValuesFault(&faultsList, DM_ENG_newSetParameterValuesFault(systemParameterList[i]->parameterName, res));
            }
         }
      }
      DM_ENG_deleteTabSystemParameterValueStruct(systemParameterList);
      if ((nbFaults == 0) && (entity == DM_ENG_EntityType_ACS)) // ParameterKey only set by ACS
      {
         DM_ENG_ParameterManager_setParameterKey(parameterKey); // a priori pas d'erreur possible !!
      }

      for (i=0; i<size; i++)
      {
         if (nbFaults == 0)
         {
            DBG("nbFaults == 0");
            DM_ENG_ParameterManager_commitParameter( parameterList[i]->parameterName );
         } else {
            DM_ENG_ParameterManager_unsetParameter( parameterList[i]->parameterName );
         }
      }
   }

   if (lockedForThisCall) {
      DM_ENG_ParameterManager_unlock();
   } else  {
      DM_ENG_ParameterManager_sync(true);
   }

   if (nbFaults == 0)
   {
      *pFaults = NULL;
   } else {
      *pFaults = (DM_ENG_SetParameterValuesFault**)calloc(nbFaults+1, sizeof(DM_ENG_SetParameterValuesFault*));
      int j;
      for (j=0; j<nbFaults; j++)
      {
         (*pFaults)[j] = faultsList;
         faultsList = faultsList->next;
      }
      //(*pFaults)[nbFaults] = NULL; // inutile
      codeRetour = DM_ENG_INVALID_ARGUMENTS;
   }
   return codeRetour;
}

/*
 * Sert à filter les valeurs remontées à l'ACS. 3 cas : Password, ConnectionRequestPassword, STUNPassword remontés comme chaînes vides.
 */
static char* _filteredValue(DM_ENG_EntityType entity, DM_ENG_Parameter* param)
{
   return ( ((entity == DM_ENG_EntityType_ACS) || (entity == DM_ENG_EntityType_SUBSCRIBER))
          ? DM_ENG_Parameter_getFilteredValue(param)
          : param->value );
}

/**
 * This function may be used by the ACS to obtain the value of one or more CPE parameters.
 * 
 * @param entity Entity number
 * @param parameterNames Array of strings, each representing the name of the requested parameter.
 * @param Pointer to the array of the ParameterValueStruct.
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_GetParameterValues(DM_ENG_EntityType entity, char* parameterNames[], OUT DM_ENG_ParameterValueStruct** pResult[])
{
   DBG("DM_ENG_GetParameterValues - Begin");
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   int codeRetour = 0;
   DM_ENG_ParameterValueStruct* pvsList = NULL;
   int nbTrouves = 0;
   int i;
   for (i=0; parameterNames[i]!=NULL; i++)
   {
      DBG("DM_ENG_GetParameterValues - Param[%d] = %s", i, parameterNames[i]);
      if (entity != DM_ENG_EntityType_SYSTEM)
      {
         codeRetour = DM_ENG_ParameterManager_loadSystemParameters(parameterNames[i], true);
         if (codeRetour != 0) break;
      }

      bool isTopName = (strlen(parameterNames[i]) == 0);
      DM_ENG_Parameter* param = (isTopName ? NULL : DM_ENG_ParameterManager_getParameter(parameterNames[i]));
      if (!isTopName && ((param==NULL) || DM_ENG_Parameter_isHidden(param)))
      {
         codeRetour = DM_ENG_INVALID_PARAMETER_NAME;
         break;
      }
      if (isTopName || (parameterNames[i][strlen(parameterNames[i])-1] == '.')) // case of partial path name
      {
         char* prmName;
         for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
         {
            if ((isTopName || (strncmp(prmName, parameterNames[i], strlen(parameterNames[i]))==0))
               && DM_ENG_Parameter_isValuable(prmName)) {
               param = DM_ENG_ParameterManager_getCurrentParameter();
               if (!DM_ENG_Parameter_isHidden(param))
               {
                  DM_ENG_addParameterValueStruct(&pvsList, DM_ENG_newParameterValueStruct(param->name, param->type, _filteredValue(entity, param)));
                  nbTrouves++;
               }
            }
         }
      }
      else
      {
         DM_ENG_addParameterValueStruct(&pvsList, DM_ENG_newParameterValueStruct(param->name, param->type, _filteredValue(entity, param)));
         nbTrouves++;
      }
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   if (codeRetour != 0)
   {
      DM_ENG_deleteAllParameterValueStruct(&pvsList);
   }
   else // pas d'erreur
   {
      *pResult = (DM_ENG_ParameterValueStruct**)calloc(nbTrouves+1, sizeof(DM_ENG_ParameterValueStruct*));
      for (i=0; i<nbTrouves; i++)
      {
         (*pResult)[i] = pvsList;
         pvsList = pvsList->next;
      }
   }
   return codeRetour;
}

/**
 * This function may be used by the ACS to obtain the value of one or more CPE parameters.
 * 
 * @param entity Entity number
 * @param path Parameter path
 * @param nextLevel If true, the response must only contains the parameters in the next level. If false, the response must also
 * contains the parameter below.
 * @param pResult Pointer to the array of the ParameterInfoStruct.
 *  
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_GetParameterNames(DM_ENG_EntityType entity, char* path, bool nextLevel, OUT DM_ENG_ParameterInfoStruct** pResult[])
{
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   int codeRetour = 0;
   if (entity != DM_ENG_EntityType_SYSTEM)
   {
      codeRetour = DM_ENG_ParameterManager_loadSystemParameters(path, false);
   }
   if (codeRetour == 0)
   {
      size_t pathLen = (path==NULL ? 0 : strlen(path));
      bool isNode = (pathLen==0) || (path[pathLen-1] == '.');
   	if ((path==NULL) || ((pathLen!=0) && (DM_ENG_ParameterManager_getParameter(path)==NULL))
       || (nextLevel && !isNode) || (strstr(path, "..")!=NULL))
      {
         codeRetour = DM_ENG_INVALID_PARAMETER_NAME;
      }
      else
      {
         DM_ENG_Parameter* param = DM_ENG_ParameterManager_getParameter(path);
         if ((param != NULL) && (DM_ENG_Parameter_isHidden(param)))
         {
            codeRetour = DM_ENG_INVALID_PARAMETER_NAME;
         }
         else
         {         
            DM_ENG_ParameterInfoStruct* infoList = NULL;
            int nbTrouves = 0;
            if (!isNode)
            {
               if (param!=NULL)
               {
                  DM_ENG_addParameterInfoStruct(&infoList, DM_ENG_newParameterInfoStruct(param->name, param->writable));
                  nbTrouves = 1;
               }
            }
            else
            {
               char* prmName;
               for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
               {
                  if (((pathLen==0) || (strncmp(prmName, path, pathLen)==0)) && (strstr(prmName, "..")==NULL))
                  {
                     char* dotAfter = strchr(prmName+pathLen, '.');
                     bool isNextLevel = (strlen(prmName) > pathLen) && ((dotAfter==NULL) || (dotAfter[1]=='\0'));
                     if (nextLevel && !isNextLevel) { continue; }
   
                     param = DM_ENG_ParameterManager_getCurrentParameter();
                     if (!DM_ENG_Parameter_isHidden(param))
                     {
                        DM_ENG_addParameterInfoStruct(&infoList, DM_ENG_newParameterInfoStruct(param->name, param->writable));
                        nbTrouves++;
                     }
                  }
               }
            }

            *pResult = (DM_ENG_ParameterInfoStruct**)calloc(nbTrouves+1, sizeof(DM_ENG_ParameterInfoStruct*));
            int i;
            for (i=0; i<nbTrouves; i++)
            {
               (*pResult)[i] = infoList;
               infoList = infoList->next;
            }
         }
      }
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   return codeRetour;
}

/**
 * This function may be used by the ACS to create a new instance of a multi-instance object.
 *
 * @param entity Entity number
 * @param objectName The path name of the collection objects
 * @param parameterKey The value to set the ParameterKey parameter
 * @param pInstanceNumber Pointer to an integer to obtain the new instance number 
 * @param pStatus Pointer to integer to obtain the status as specified.
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_AddObject(DM_ENG_EntityType entity, char* objectName, char* parameterKey, OUT unsigned int* pInstanceNumber, OUT DM_ENG_ParameterStatus* pStatus)
{
   int codeRetour = 0;
   *pStatus = DM_ENG_ParameterStatus_UNDEFINED;
   if (objectName == NULL) { return DM_ENG_INVALID_PARAMETER_NAME; }
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   codeRetour = DM_ENG_ParameterManager_addObject(entity, objectName, pInstanceNumber, pStatus);
   if ((codeRetour == 0) && (entity == DM_ENG_EntityType_ACS))
   {
      DM_ENG_ParameterManager_setParameterKey(parameterKey);
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   else { DM_ENG_ParameterManager_sync(true); }
   return codeRetour;
}

/**
 * This function may be used by the ACS to remove a particular instance of an object.
 *
 * @param entity Entity number
 * @param objectName The path name of the object instance to be removed
 * @param parameterKey The value to set the ParameterKey parameter
 * @param pStatus Pointer to integer to obtain the status as specified.
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_DeleteObject(DM_ENG_EntityType entity, char* objectName, char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus)
{
   int codeRetour = 0;
   *pStatus = DM_ENG_ParameterStatus_UNDEFINED;
   if (objectName == NULL) { return DM_ENG_INVALID_PARAMETER_NAME; }
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   codeRetour = DM_ENG_ParameterManager_deleteObject(entity, objectName, pStatus);
   if ((codeRetour == 0) && (entity == DM_ENG_EntityType_ACS))
   {
      DM_ENG_ParameterManager_setParameterKey(parameterKey);
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   else { DM_ENG_ParameterManager_sync(true); }
   return codeRetour;
}

/**
 * This function may be used by the ACS to modify attributes associated with on or more parameters.
 *
 * @param entity Entity number
 * @param pasList List of changes to be made to the attributes for a set of parameters
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_SetParameterAttributes(DM_ENG_EntityType entity, DM_ENG_ParameterAttributesStruct* pasList[])
{
   if (entity != DM_ENG_EntityType_ACS) { return DM_ENG_REQUEST_DENIED; } // Only ACS is allowed to modif attributes 

   int codeRetour = 0;
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

	DM_ENG_ParameterAttributesStruct* backPasList = NULL;
	int i;
	for (i=0; pasList[i]!=NULL; i++)
	{
	   char* name = pasList[i]->parameterName;
      DM_ENG_NotificationMode notif = (pasList[i]->notification == (DM_ENG_NotificationMode)-1 ? DM_ENG_NotificationMode_UNDEFINED : pasList[i]->notification);
      if (notif>DM_ENG_NotificationMode_UNDEFINED)
      {
         codeRetour = DM_ENG_INVALID_ARGUMENTS;
         break;
      }
      codeRetour = DM_ENG_ParameterManager_loadSystemParameters(name, false);
      if (codeRetour != 0) break;
      bool isTopName = (strlen(name)==0);
      DM_ENG_Parameter* param = NULL;
      if (!isTopName)
      {
         param = DM_ENG_ParameterManager_getParameter(name);
         if ((param == NULL) || (strstr(name, "..")!=NULL) || DM_ENG_Parameter_isHidden(param))
         {
            codeRetour = DM_ENG_INVALID_PARAMETER_NAME;
            break;
         }
      }
      if (isTopName || (name[strlen(name)-1] == '.')) // case of partial path name
      {
         char* prmName;
         for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
         {
            if ((isTopName || (strncmp(prmName, name, strlen(name))==0))
             && (prmName[strlen(prmName)-1] != '.') && (strstr(prmName, "..")==NULL))
            {
               DM_ENG_Parameter* prm = DM_ENG_ParameterManager_getCurrentParameter();
               if (!DM_ENG_Parameter_isHidden(prm))
               {
                  DM_ENG_addParameterAttributesStruct(&backPasList, DM_ENG_newParameterAttributesStruct(prm->name, prm->notification, prm->accessList));
                  codeRetour = DM_ENG_Parameter_setAttributes(prm, notif, pasList[i]->accessList);
                  if (codeRetour != 0) break;
               }
            }
         }
      }
      else
      {
         DM_ENG_addParameterAttributesStruct(&backPasList, DM_ENG_newParameterAttributesStruct(param->name, param->notification, param->accessList));
         codeRetour = DM_ENG_Parameter_setAttributes(param, notif, pasList[i]->accessList);
      }
      if (codeRetour != 0) break;
   }
   while (backPasList != NULL) // on libère backPasList
   {
      DM_ENG_ParameterAttributesStruct* pas = backPasList;
      if (codeRetour != 0) // si erreur, on remet les attributs dans l'état initial
      {
         DM_ENG_Parameter* param = DM_ENG_ParameterManager_getParameter(pas->parameterName);
         DM_ENG_Parameter_setAttributes(param, pas->notification, pas->accessList);
      }
      backPasList = backPasList->next;
      DM_ENG_deleteParameterAttributesStruct(pas);
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   else { DM_ENG_ParameterManager_sync(false); }
   return codeRetour;
}

/**
 * This function may be used by the ACS to read the attributes associated with on or more parameters.
 *
 * @param entity Entity number
 * @param names Array of string, each representing the name of a requested parameter
 * @param pResult Pointer to array of ParameterAttributesStruct to obtain the result
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_GetParameterAttributes(DM_ENG_EntityType entity, char* names[], OUT DM_ENG_ParameterAttributesStruct** pResult[])
{
   int codeRetour = 0;
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   DM_ENG_ParameterAttributesStruct* pasList = NULL;
   int nbPas = 0;
   int i;
   for (i=0; names[i]!=NULL; i++)
   {
      if (entity != DM_ENG_EntityType_SYSTEM)
      {
         codeRetour = DM_ENG_ParameterManager_loadSystemParameters(names[i], false);
         if (codeRetour != 0) break;
      }
      bool isTopName = (strlen(names[i])==0);
      DM_ENG_Parameter* param = NULL;
      if (!isTopName)
      {
         param = DM_ENG_ParameterManager_getParameter(names[i]);
         if ((param == NULL) || (strstr(names[i], "..")!=NULL) || DM_ENG_Parameter_isHidden(param))
         {
            codeRetour = DM_ENG_INVALID_PARAMETER_NAME;
            break;
         }
      }
      if (isTopName || (names[i][strlen(names[i])-1] == '.'))
      {
         char* prmName;
         for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
         {
            if ((isTopName || (strncmp(prmName, names[i], strlen(names[i]))==0))
             && (prmName[strlen(prmName)-1] != '.') && (strstr(prmName, "..")==NULL))
            {
               DM_ENG_Parameter* prm = DM_ENG_ParameterManager_getCurrentParameter();
               if (!DM_ENG_Parameter_isHidden(prm))
               {
                  DM_ENG_addParameterAttributesStruct(&pasList, DM_ENG_newParameterAttributesStruct(prm->name, prm->notification, prm->accessList));
                  nbPas++;
               }
            }
         }
      }
      else
      {
         DM_ENG_addParameterAttributesStruct(&pasList, DM_ENG_newParameterAttributesStruct(param->name, param->notification, param->accessList));
         nbPas++;
      }
   }

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   if (codeRetour != 0)
   {
      DM_ENG_deleteAllParameterAttributesStruct(&pasList);
   }
   else
   {
      *pResult = (DM_ENG_ParameterAttributesStruct**)calloc(nbPas+1, sizeof(DM_ENG_ParameterAttributesStruct*));
      for (i=0; i<nbPas; i++)
      {
         (*pResult)[i] = pasList;
         pasList = pasList->next;
      }
   }
   return codeRetour;
}

/**
 * This function may be used by the ACS to request the CPE to schedule an Inform method call.
 *
 * @param entity Entity number
 * @param delay Delay in seconds
 * @param commandKey Command key string associated with this command
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_ScheduleInform(DM_ENG_EntityType entity, unsigned int delay, char* commandKey)
{
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   int res = DM_ENG_InformMessageScheduler_scheduleInform(delay, commandKey);

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   return res;
}

/**
 * This function may be used to request the CPE to call the Inform method with the CONNECTION REQUEST event.
 *
 * @param entity Entity number
 *
 * @return 0
 */
int DM_ENG_RequestConnection(DM_ENG_EntityType entity)
{
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   DM_ENG_InformMessageScheduler_requestConnection();

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   return 0;
}

/**
 * This function may be used by the ACS to cause the CPE to download the specified file from the designated location.
 *
 * @param entity Entity number
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the source file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param fileSize Size of the file to be downloaded in bytes
 * @param targetFileName Name of the file to be used on the target file system
 * @param delay Delay in seconds to perform the download opération
 * @param successURL URL the CPE may redirect the user's browser to if the download completes successfully
 * @param failureURL URL the CPE may redirect the user's browser to if the download does not complete successfully
 * @param commandKey String used to refer to a particular download
 * @param pResult Pointer to a DM_ENG_TransferResultStruct to obtain the result of the command
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_Download(DM_ENG_EntityType entity, char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName,
		  unsigned int delay, char* successURL, char* failureURL, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult)
{
   if ((entity == DM_ENG_EntityType_SYSTEM) && (delay == 0)) // pour empêcher les appels récursifs au système (via Device Adapter)
   {
      return DM_ENG_REQUEST_DENIED;
   }

   int res = 0;
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   res = DM_ENG_ParameterManager_manageTransferRequest(true, delay, fileType, url, username, password, fileSize, targetFileName, successURL, failureURL, commandKey, pResult);

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   return res;
}

/**
 * This function may be used by the ACS to cause the CPE to upload the specified file to the designated location.
 *
 * @param entity Entity number
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the destination file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param delay Delay in seconds to perform the upload opération
 * @param commandKey String used to refer to a particular upload
 * @param pResult Pointer to a DM_ENG_TransferResultStruct to obtain the result of the command
 *
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069
 */
int DM_ENG_Upload(DM_ENG_EntityType entity, char* fileType, char* url, char* username, char* password, unsigned int delay, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult)
{
   if ((entity == DM_ENG_EntityType_SYSTEM) && (delay == 0)) // pour empêcher les appels récursifs au système (via Device Adapter)
   {
      return DM_ENG_REQUEST_DENIED;
   }

   int res = 0;
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   res = DM_ENG_ParameterManager_manageTransferRequest(false, delay, fileType, url, username, password, 0, NULL, NULL, NULL, commandKey, pResult);

   if (lockedForThisCall) { DM_ENG_ParameterManager_unlock(); }
   return res;
}

/**
 * This function may be used by the ACS to cause the CPE to reboot.
 * 
 * @param entity Entity number
 * @param commandKey The string to return in the CommandKey element of the InformStruct
 *
 * @return 0
 */
int DM_ENG_Reboot(DM_ENG_EntityType entity, char* commandKey)
{
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   DM_ENG_ParameterManager_addRebootCommandKey(commandKey);

   if (lockedForThisCall)
   {
      DM_ENG_ParameterManager_unlock();
      DM_ENG_ParameterManager_checkReboot();
   }
   return 0;
}

/**
 * This function may be used by the ACS to reset the CPE to its factory default state. 
 * 
 * @param entity Entity number
 *
 * @return 0
 */
int DM_ENG_FactoryReset(DM_ENG_EntityType entity)
{
   return DM_ENG_Reboot(entity, "*");
}

/**
 * This function may be used by the ACS to determine the status of previously requested downloads or uploads. 
 * 
 * @param entity Entity number
 * @param pResult Pointer to an array of DM_ENG_AllQueuedTransferStruct to obtain the result of the command
 *
 * @return 0 if OK, else a fault code (9000, ...) as specified in TR-069
 */
int DM_ENG_GetAllQueuedTransfers(DM_ENG_EntityType entity, OUT DM_ENG_AllQueuedTransferStruct** pResult[])
{
   bool lockedForThisCall = _toLockForOneCall(entity);
   if (lockedForThisCall) { DM_ENG_ParameterManager_lock(); }

   int res = DM_ENG_ParameterManager_getAllQueuedTransfers(pResult);

   if (lockedForThisCall)
   {
      DM_ENG_ParameterManager_unlock();
      DM_ENG_ParameterManager_checkReboot();
   }
   return res;
}

/**
 * This function may be used by an entity to lock the parameters management for a sequence of calls (not included in an ACS session).
 * 
 * @param entity Entity number
 */
void DM_ENG_Lock(DM_ENG_EntityType entity)
{
   if (entity != DM_ENG_EntityType_SYSTEM)
   {
      DM_ENG_ParameterManager_lock();
   }
   else
   {
      DM_ENG_ParameterManager_lockDM();
   }
   _currentEntity = entity;
}

/**
 * This function may be used by an entity to unlock the parameters management after a sequence of calls (not included in a ACS session).
 * 
 * @param entity Entity number
 */
void DM_ENG_Unlock()
{
   _currentEntity = (DM_ENG_EntityType)-1;
   DM_ENG_ParameterManager_unlock();
}
