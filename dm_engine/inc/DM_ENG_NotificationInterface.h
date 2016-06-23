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
 * File        : DM_ENG_NotificationInterface.h
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
 * @file DM_ENG_NotificationInterface.h
 *
 * @brief 
 *
 **/

#ifndef _DM_ENG_NOTIFICATION_INTERFACE_H_
#define _DM_ENG_NOTIFICATION_INTERFACE_H_

#include "DM_ENG_EntityType.h"
#include "DM_ENG_Common.h"
#include "DM_ENG_DeviceIdStruct.h"
#include "DM_ENG_EventStruct.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_TransferCompleteStruct.h"
#include "DM_ENG_ArgStruct.h"

typedef enum _DM_ENG_NotificationStatus
{
   DM_ENG_COMPLETED = 0,
   DM_ENG_SESSION_OPENING,
   DM_ENG_SESSION_OPENED,
   DM_ENG_CANCELLED

} DM_ENG_NotificationStatus;

typedef int (*DM_ENG_F_INFORM)(DM_ENG_DeviceIdStruct* id, DM_ENG_EventStruct* events[], DM_ENG_ParameterValueStruct* parameterList[], time_t currentTime, unsigned int retryCount);
typedef int (*DM_ENG_F_TRANSFER_COMPLETE)(DM_ENG_TransferCompleteStruct* tcs);
typedef int (*DM_ENG_F_REQUEST_DOWNLOAD)(const char* fileType, DM_ENG_ArgStruct* args[]);
typedef void (*DM_ENG_F_UPDATE_ACS_PARAMETERS)(char* url, char* username, char* password);

typedef struct _DM_ENG_NotificationInterface
{
   DM_ENG_ParameterValueStruct* knownParameters;
   DM_ENG_ParameterName* knownParametersToRemove;
   DM_ENG_F_INFORM inform;
   DM_ENG_F_TRANSFER_COMPLETE transferComplete;
   DM_ENG_F_REQUEST_DOWNLOAD requestDownload;

} DM_ENG_NotificationInterface;

void DM_ENG_NotificationInterface_activate(DM_ENG_EntityType entity,
   DM_ENG_F_INFORM inform,
   DM_ENG_F_TRANSFER_COMPLETE transferComplete,
   DM_ENG_F_REQUEST_DOWNLOAD requestDownload,
   DM_ENG_F_UPDATE_ACS_PARAMETERS updateAcsParameters);
void DM_ENG_NotificationInterface_deactivate(DM_ENG_EntityType entity);
void DM_ENG_NotificationInterface_deactivateAll();
void DM_ENG_NotificationInterface_updateAcsParameters();
bool DM_ENG_NotificationInterface_isReady();
void DM_ENG_NotificationInterface_setKnownParameterList(DM_ENG_EntityType entity, DM_ENG_ParameterValueStruct* parameterList[]);
void DM_ENG_NotificationInterface_clearKnownParameters(DM_ENG_EntityType entity);
DM_ENG_NotificationStatus DM_ENG_NotificationInterface_inform(DM_ENG_DeviceIdStruct* id, DM_ENG_EventStruct* events[], DM_ENG_ParameterValueStruct* parameterList[], time_t currentTime, unsigned int retryCount);
DM_ENG_NotificationStatus DM_ENG_NotificationInterface_transferComplete(DM_ENG_TransferCompleteStruct* tcs);
DM_ENG_NotificationStatus DM_ENG_NotificationInterface_requestDownload(const char* fileType, DM_ENG_ArgStruct* args[]);

#endif
