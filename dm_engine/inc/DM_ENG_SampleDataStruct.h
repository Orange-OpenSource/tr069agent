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
 * File        : DM_ENG_SampleDataStruct.h
 *
 * Created     : 04/02/2010
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
 * @file DM_ENG_SampleDataStruct.h
 *
 * @brief This structure represents sampled data provided by low level notification (via the Device Adapter)  
 *
 */

#ifndef _DM_ENG_SAMPLE_DATA_STRUCT_H_
#define _DM_ENG_SAMPLE_DATA_STRUCT_H_

#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_Common.h"

/**
 * Sampled Data Structure
 */
typedef struct _DM_ENG_SampleDataStruct
{
   const char* statObject;
   DM_ENG_ParameterValueStruct** parameterList;
   time_t timestamp;
   bool continued;
   bool suspectData;

} __attribute((packed)) DM_ENG_SampleDataStruct;

DM_ENG_SampleDataStruct* DM_ENG_newSampleDataStruct(const char* obj, DM_ENG_ParameterValueStruct* params[], time_t ts, bool cont, bool sd);
void DM_ENG_deleteSampleDataStruct(DM_ENG_SampleDataStruct* dr);

#endif
