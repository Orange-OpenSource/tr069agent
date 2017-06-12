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
 * File        : DM_ENG_ParameterInfoStruct.h
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
 * @file DM_ENG_ParameterInfoStruct.h
 *
 * @brief The ParameterInfoStruct as defined in TR-069.
 *
 **/

#ifndef _DM_ENG_PARAMETER_INFO_STRUCT_H_
#define _DM_ENG_PARAMETER_INFO_STRUCT_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterStatus.h"

/**
 * Parameter Information Structure
 */
typedef struct _DM_ENG_ParameterInfoStruct
{
   /** Full name of a parameter or partial path */
   char* parameterName;
   /** Whether or not the parameter value can be overwritten using the SetParameterValues method */
   bool writable;
   struct _DM_ENG_ParameterInfoStruct* next;

} __attribute((packed)) DM_ENG_ParameterInfoStruct;

DM_ENG_ParameterInfoStruct* DM_ENG_newParameterInfoStruct(char* name, bool wr);
void DM_ENG_deleteParameterInfoStruct(DM_ENG_ParameterInfoStruct* info);
void DM_ENG_deleteAllParameterInfoStruct(DM_ENG_ParameterInfoStruct** pInfo);
DM_ENG_ParameterInfoStruct** DM_ENG_newTabParameterInfoStruct(int size);
void DM_ENG_deleteTabParameterInfoStruct(DM_ENG_ParameterInfoStruct* tInfo[]);
void DM_ENG_addParameterInfoStruct(DM_ENG_ParameterInfoStruct** pInfo, DM_ENG_ParameterInfoStruct* newInfo);

#endif
