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
 * File        : DM_ENG_ParameterAttributesStruct.h
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
 * @file DM_ENG_ParameterAttributesStruct.h
 *
 * @brief The DM_ENG_ParameterAttributesStruct is based on the defined SetParameterAttributesStruct structure in TR-069.
 *
 */

#ifndef _DM_ENG_PARAMETER_ATTRIBUTES_STRUCT_H_
#define _DM_ENG_PARAMETER_ATTRIBUTES_STRUCT_H_

#include "DM_ENG_NotificationMode.h"

/**
 * Data structure representing the attributes of a parameter
 */
typedef struct _DM_ENG_ParameterAttributesStruct
{
   /** Parameter name */
   char* parameterName;
   /** Notification mode 0, 1, 2 or 3 (=DM_ENG_NotificationMode_UNDEFINED) */
   DM_ENG_NotificationMode notification;
   /** Array list of the entities for which write access to the specified parameter is granted */ 
   char** accessList;
   struct _DM_ENG_ParameterAttributesStruct* next;

} __attribute((packed)) DM_ENG_ParameterAttributesStruct;

DM_ENG_ParameterAttributesStruct* DM_ENG_newParameterAttributesStruct(char* name, DM_ENG_NotificationMode notif, char* al[]);
void DM_ENG_deleteParameterAttributesStruct(DM_ENG_ParameterAttributesStruct* attr);
void DM_ENG_deleteAllParameterAttributesStruct(DM_ENG_ParameterAttributesStruct** pAttr);
DM_ENG_ParameterAttributesStruct** DM_ENG_newTabParameterAttributesStruct(int size);
void DM_ENG_deleteTabParameterAttributesStruct(DM_ENG_ParameterAttributesStruct* tAttr[]);
void DM_ENG_addParameterAttributesStruct(DM_ENG_ParameterAttributesStruct** pAttr, DM_ENG_ParameterAttributesStruct* newAttr);

#endif
