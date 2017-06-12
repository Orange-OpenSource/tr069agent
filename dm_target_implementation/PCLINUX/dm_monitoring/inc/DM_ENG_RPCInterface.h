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
 * File        : DM_ENG_RPCInterface.h
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
 * @file DM_ENG_RPCInterface.h
 *
 * @brief The RPC Interface.
 * 
 * This header file defines the RPC Interface which provides similar functions to the TR-069 RPC methods, plus
 * somme functions for the notification by Inform messages and for the session management.  
 *
 */

#ifndef _DM_ENG_RPC_INTERFACE_H_
#define _DM_ENG_RPC_INTERFACE_H_

#include "DM_ENG_NotificationInterface.h"
#include "DM_ENG_ParameterInfoStruct.h"
#include "DM_ENG_ParameterAttributesStruct.h"
#include "DM_ENG_SetParameterValuesFault.h"
#include "DM_ENG_AllQueuedTransferStruct.h"
#include "DM_ENG_TransferResultStruct.h"
#include "DM_ENG_Error.h"

int DM_ENG_GetRPCMethods(DM_ENG_EntityType entity, OUT const char** pMethods[]);
int DM_ENG_SetParameterValues(DM_ENG_EntityType entity, DM_ENG_ParameterValueStruct* parameterList[], char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus, OUT DM_ENG_SetParameterValuesFault** pFaults[]);
int DM_ENG_GetParameterValues(DM_ENG_EntityType entity, char* parameterNames[], OUT DM_ENG_ParameterValueStruct** pResult[]);
int DM_ENG_GetParameterNames(DM_ENG_EntityType entity, char* path, bool nextLevel, OUT DM_ENG_ParameterInfoStruct** pResult[]);
int DM_ENG_AddObject(DM_ENG_EntityType entity, char* objectName, char* parameterKey, OUT unsigned int* pInstanceNumber, OUT DM_ENG_ParameterStatus* pStatus);
int DM_ENG_DeleteObject(DM_ENG_EntityType entity, char* objectName, char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus);
int DM_ENG_SetParameterAttributes(DM_ENG_EntityType entity, DM_ENG_ParameterAttributesStruct* pasList[]);
int DM_ENG_GetParameterAttributes(DM_ENG_EntityType entity, char* names[], OUT DM_ENG_ParameterAttributesStruct** pResult[]);
int DM_ENG_ScheduleInform(DM_ENG_EntityType entity, unsigned int delay, char* commandKey);
int DM_ENG_RequestConnection(DM_ENG_EntityType entity);
int DM_ENG_Download(DM_ENG_EntityType entity, char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName,
        unsigned int delay, char* successURL, char* failureURL, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult);
int DM_ENG_Upload(DM_ENG_EntityType entity, char* fileType, char* url, char* username, char* password, unsigned int delay, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult);
int DM_ENG_Reboot(DM_ENG_EntityType entity, char* commandKey);
int DM_ENG_FactoryReset(DM_ENG_EntityType entity);
int DM_ENG_GetAllQueuedTransfers(DM_ENG_EntityType entity, OUT DM_ENG_AllQueuedTransferStruct** pResult[]);

void DM_ENG_ActivateNotification(DM_ENG_EntityType entity, DM_ENG_F_INFORM inform, DM_ENG_F_TRANSFER_COMPLETE transferComplete, DM_ENG_F_REQUEST_DOWNLOAD requestDownload, DM_ENG_F_UPDATE_ACS_PARAMETERS updateAcsParameters);
void DM_ENG_DeactivateNotification(DM_ENG_EntityType entity);
void DM_ENG_SessionOpened(DM_ENG_EntityType entity);
bool DM_ENG_IsReadyToClose(DM_ENG_EntityType entity);
void DM_ENG_SessionClosed(DM_ENG_EntityType entity, bool success);

void DM_ENG_Lock(DM_ENG_EntityType entity);
void DM_ENG_Unlock();

#endif
