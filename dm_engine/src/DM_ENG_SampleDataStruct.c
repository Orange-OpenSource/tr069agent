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
 * File        : DM_ENG_SampleDataStruct.c
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
 * @file DM_ENG_SampleDataStruct.c
 *
 * @brief This structure represents sampled data provided by low level notification (via the Device Adapter)  
 *
 */

#include "DM_ENG_SampleDataStruct.h"

/**
 * Constructs a new Sampled Data Structure.
 * 
 * @param obj Path of the statistics object
 * @param params List of DM_ENG_ParameterValueStruct
 * @param ts Time stamp
 * @param cont Continuity indicator
 * @param sd Indicator of suspect data
 * 
 * @return A pointer to the new SampleDataStruct object
 */
DM_ENG_SampleDataStruct* DM_ENG_newSampleDataStruct(const char* obj, DM_ENG_ParameterValueStruct* params[], time_t ts, bool cont, bool sd)
{
   DM_ENG_SampleDataStruct* res = (DM_ENG_SampleDataStruct*) malloc(sizeof(DM_ENG_SampleDataStruct));
   res->statObject = strdup(obj);
   res->parameterList = (params==NULL ? NULL : DM_ENG_copyTabParameterValueStruct(params));
   res->timestamp = ts;
   res->continued = cont;
   res->suspectData = sd;
   return res;
}

/**
 * Deletes the Sampled Data Structure.
 * 
 * @param as A pointer to a SampleDataStruct object
 * 
 */
void DM_ENG_deleteSampleDataStruct(DM_ENG_SampleDataStruct* sd)
{
   free((char*)sd->statObject);
   if (sd->parameterList != NULL) { DM_ENG_deleteTabParameterValueStruct(sd->parameterList); }
   free(sd);
}
