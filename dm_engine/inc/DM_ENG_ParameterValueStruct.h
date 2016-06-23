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
 * File        : DM_ENG_ParameterValueStruct.h
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
 * @file DM_ENG_ParameterValueStruct.h
 *
 * @brief The ParameterValueStruct is based on the defined structure in TR-069 with an supplementary "type" field.
 *
 */

#ifndef _DM_ENG_PARAMETER_VALUE_STRUCT_H_
#define _DM_ENG_PARAMETER_VALUE_STRUCT_H_

#include "DM_ENG_ParameterStatus.h"
#include "DM_ENG_ParameterType.h"
#include <time.h>

/**
 * Parameter Value Structure
 */
typedef struct _DM_ENG_ParameterValueStruct
{
   /** The name of the parameter */
   const char* parameterName; // COMP
   /** The type of the parameter */
   DM_ENG_ParameterType type;
   /** The value of the parameter */
   char* value; // COMP
   /** Field for a possible timestamp */
   time_t timestamp;

   struct _DM_ENG_ParameterValueStruct* next;

} __attribute((packed)) DM_ENG_ParameterValueStruct;

DM_ENG_ParameterValueStruct* DM_ENG_newParameterValueStruct(const char* name, DM_ENG_ParameterType type, char* val);
void DM_ENG_deleteParameterValueStruct(DM_ENG_ParameterValueStruct* pvs);
void DM_ENG_addParameterValueStruct(DM_ENG_ParameterValueStruct** pPvs, DM_ENG_ParameterValueStruct* newPvs);
void DM_ENG_deleteAllParameterValueStruct(DM_ENG_ParameterValueStruct** pPvs);
DM_ENG_ParameterValueStruct** DM_ENG_newTabParameterValueStruct(int size);
DM_ENG_ParameterValueStruct** DM_ENG_copyTabParameterValueStruct(DM_ENG_ParameterValueStruct* tPvs[]);
void DM_ENG_deleteTabParameterValueStruct(DM_ENG_ParameterValueStruct* tPvs[]);

typedef struct _DM_ENG_ParameterName
{
   const char* name; // COMP
   struct _DM_ENG_ParameterName* next;

} __attribute((packed)) DM_ENG_ParameterName;

DM_ENG_ParameterName* DM_ENG_newParameterName(const char* aName);
void DM_ENG_deleteParameterName(DM_ENG_ParameterName* pn);
void DM_ENG_addParameterName(DM_ENG_ParameterName** pPn, DM_ENG_ParameterName* newPn);
void DM_ENG_deleteAllParameterName(DM_ENG_ParameterName** pPn);
DM_ENG_ParameterName** DM_ENG_newTabParameterName(int size);
void DM_ENG_deleteTabParameterName(DM_ENG_ParameterName* tPn[]);

/**
 * Variant of Parameter Value Structure with a systemData field
 */
typedef struct _DM_ENG_SystemParameterValueStruct
{
   /** The name of the parameter */
   const char* parameterName; // COMP
   /** The value of the parameter */
   char* value; // COMP
   /** The system data */
   char* systemData; // COMP

} __attribute((packed)) DM_ENG_SystemParameterValueStruct;

DM_ENG_SystemParameterValueStruct* DM_ENG_newSystemParameterValueStruct(const char* name, char* val, char* data);
void DM_ENG_deleteSystemParameterValueStruct(DM_ENG_SystemParameterValueStruct* spvs);
DM_ENG_SystemParameterValueStruct** DM_ENG_newTabSystemParameterValueStruct(int size);
void DM_ENG_deleteTabSystemParameterValueStruct(DM_ENG_SystemParameterValueStruct* tSpvs[]);

#endif
