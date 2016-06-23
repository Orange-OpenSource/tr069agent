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
 * File        : DM_ENG_ParameterManager.h
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
 * @file DM_ENG_ParameterManager.h
 *
 * @brief 
 *
 **/

#ifndef _DM_ENG_PARAMETER_MANAGER_H_
#define _DM_ENG_PARAMETER_MANAGER_H_

#include "DM_ENG_Parameter.h"
#include "DM_ENG_ParameterStatus.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_EventStruct.h"
#include "DM_ENG_EntityType.h"
#include "DM_ENG_TransferResultStruct.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_DownloadRequest.h"
#include "DM_ENG_AllQueuedTransferStruct.h"

int DM_ENG_ParameterManager_getNumberOfParameters();
DM_ENG_Parameter* DM_ENG_ParameterManager_getParameter(const char* name);
DM_ENG_Parameter* DM_ENG_ParameterManager_getFirstParameter();
DM_ENG_Parameter* DM_ENG_ParameterManager_getNextParameter();
char* DM_ENG_ParameterManager_getFirstParameterName();
char* DM_ENG_ParameterManager_getNextParameterName();
void DM_ENG_ParameterManager_resumeParameterLoopFrom(char* resumeName);
DM_ENG_Parameter* DM_ENG_ParameterManager_getCurrentParameter();
char** DM_ENG_ParameterManager_getMandatoryParameters();
bool DM_ENG_ParameterManager_isInformParameter(const char* name);
int DM_ENG_ParameterManager_loadSystemParameter(DM_ENG_Parameter* param, bool withValues);
int DM_ENG_ParameterManager_loadSystemParameters(const char* name, bool withValues);
int DM_ENG_ParameterManager_loadLeafParameterValue(DM_ENG_Parameter* param, bool checkChange);
void DM_ENG_ParameterManager_updateParameterNames(const char* name, DM_ENG_ParameterValueStruct* parameterList[]);
void DM_ENG_ParameterManager_updateParameterValues(DM_ENG_ParameterValueStruct* parameterList[]);
int DM_ENG_ParameterManager_getParameterValue(const char* name, OUT char** pValue);
int DM_ENG_ParameterManager_setParameterValue(DM_ENG_EntityType entity, const char* name, DM_ENG_ParameterType type, char* value, OUT DM_ENG_ParameterStatus* pResult, OUT char** pSysName, OUT char** pData);
void DM_ENG_ParameterManager_setParameterKey(char* parameterKey);
void DM_ENG_ParameterManager_commitParameter(const char* name);
void DM_ENG_ParameterManager_unsetParameter(const char* name);
int DM_ENG_ParameterManager_addObject(DM_ENG_EntityType entity, char* objectName, OUT unsigned int* pInstanceNumber, OUT DM_ENG_ParameterStatus* pStatus);
int DM_ENG_ParameterManager_deleteObject(DM_ENG_EntityType entity, char* objectName, OUT DM_ENG_ParameterStatus* pStatus);

void DM_ENG_ParameterManager_setRebootCommandKey(char* commandKey);
void DM_ENG_ParameterManager_addRebootCommandKey(char* commandKey);
char* DM_ENG_ParameterManager_getRebootCommandKey();
int DM_ENG_ParameterManager_getRetryCount();
int DM_ENG_ParameterManager_incRetryCount();
void DM_ENG_ParameterManager_checkReboot();

bool DM_ENG_ParameterManager_isPeriodicInformEnabled(); // non utilisé en externe !!
unsigned int DM_ENG_ParameterManager_getPeriodicInformInterval(); // id.
time_t DM_ENG_ParameterManager_getPeriodicInformTime(); // id.
void DM_ENG_ParameterManager_addScheduledInformCommand(time_t time, char* commandKey);
time_t DM_ENG_ParameterManager_getNextTimeout();
int DM_ENG_ParameterManager_manageTransferRequest(bool isDownload, unsigned int delay, char* fileType, char* url, char* username, char* password,
        unsigned int fileSize, char* targetFileName, char* successURL, char* failureURL, char* commandKey, OUT DM_ENG_TransferResultStruct** pResult);
int DM_ENG_ParameterManager_getAllQueuedTransfers(OUT DM_ENG_AllQueuedTransferStruct** pResult[]);
char* DM_ENG_ParameterManager_getManufacturerName();
char* DM_ENG_ParameterManager_getManufacturerOUI();
char* DM_ENG_ParameterManager_getProductClass();
char* DM_ENG_ParameterManager_getSerialNumber();

void DM_ENG_ParameterManager_manageEvents(DM_ENG_EventStruct* evt);
void DM_ENG_ParameterManager_updateValueChangeEvent(bool valueChanged);
bool DM_ENG_ParameterManager_isEventsEmpty();
DM_ENG_EventStruct** DM_ENG_ParameterManager_getEventsTab();
void DM_ENG_ParameterManager_clearEvents();
DM_ENG_TransferRequest* DM_ENG_ParameterManager_getTransferResult();
void DM_ENG_ParameterManager_removeTransferResult(unsigned int transferId);
void DM_ENG_ParameterManager_removeAllTransferResults();
void DM_ENG_ParameterManager_addMTransferEvents();
void DM_ENG_ParameterManager_deleteMTransferEvents();
DM_ENG_DownloadRequest* DM_ENG_ParameterManager_getFirstDownloadRequest();
void DM_ENG_ParameterManager_removeFirstDownloadRequest();
void DM_ENG_ParameterManager_removeAllDownloadRequests();

void DM_ENG_ParameterManager_lockDM();
void DM_ENG_ParameterManager_lock();
void DM_ENG_ParameterManager_unlock();
void DM_ENG_ParameterManager_sync(bool resetComputed);

void DM_ENG_ParameterManager_convertNamesToLong(const char* objName, DM_ENG_ParameterValueStruct* paramVal[]);

#endif
