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
 * File        : DM_SystemCalls.h
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
 * @file DM_SystemCalls.h
 *
 * @brief
 *
 **/


#ifndef _DM_SYSTEM_CALLS_H_
#define _DM_SYSTEM_CALLS_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterValueStruct.h"

void DM_SystemCalls_reboot(bool factoryReset);
char* DM_SystemCalls_getIPAddress(); // Attention, the value is not copied !
char* DM_SystemCalls_getMACAddress(); 
char* DM_SystemCalls_getSystemData(const char* name, const char* data); // Attention, the value IS copied !
bool DM_SystemCalls_getSystemObjectData(const char* objName, OUT DM_ENG_ParameterValueStruct** pParamVal[]);
bool DM_SystemCalls_setSystemData(const char* name, const char* value);
void DM_SystemCalls_freeSessionData();
int DM_SystemCalls_ping(char* host, int numberOfRepetitions, long timeout, unsigned int dataSize, unsigned int dscp,
                        bool* pFoundHost, int* pRecus, int* pPerdus, int* pMoy, int* pMin, int* pMax);
int DM_SystemCalls_traceroute(char* host, long timeout, int dataBlockSize, int maxHopCount,
                              bool* pFoundHost, int* pResponseTime, char** pRouteHops[]);
int DM_SystemCalls_download(char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName);

#endif /*_DM_SYSTEM_CALLS_H_*/
