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
 * File        : DM_ComponentCalls.h
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
 * @file DM_ComponentCalls.h
 *
 * @brief 
 *
 * Functions to be defined to provide access to data of a specific component (e.g. A/V decoder, Homeplug interface, ...)
 *
 **/


#ifndef _DM_COMPONENT_CALLS_H_
#define _DM_COMPONENT_CALLS_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_SampleDataStruct.h"

char* DM_ComponentCalls_getComponentData(const char* name, const char* data); // Attention, the value IS copied !
bool DM_ComponentCalls_getComponentObjectData(const char* objName, OUT DM_ENG_ParameterValueStruct** pParamVal[]);
bool DM_ComponentCalls_setComponentData(const char* name, const char* value);
void DM_ComponentCalls_freeSessionData();

int DM_ENG_ComponentCalls_startSampling(const char* objName, time_t timeRef, unsigned int sampleInterval, const char* args);
int DM_ENG_ComponentCalls_stopSampling(const char* objName);
int DM_ENG_ComponentCalls_getSampleData(const char* objName, time_t timeStampBefore, const char* data, OUT DM_ENG_SampleDataStruct** pSampleData);

#endif /*_DM_COMPONENT_CALLS_H_*/
