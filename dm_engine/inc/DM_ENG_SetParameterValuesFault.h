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
 * File        : DM_ENG_SetParameterValuesFault.h
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
 * @file DM_ENG_SetParameterValuesFault.h
 *
 * @brief SetParameterValuesFault structure as defined in TR-069.
 *
 */

#ifndef _DM_ENG_SET_PARAMETER_VALUES_FAULT_H_
#define _DM_ENG_SET_PARAMETER_VALUES_FAULT_H_

/**
 * SetParameterValuesFault
 */
typedef struct _DM_ENG_SetParameterValuesFault
{
   /** Full path name of the parameter in error */
   const char* parameterName; // COMP
   /** Error code associated with the particular parameter */
   unsigned int faultCode;
   /** Human readable description of the fault */
   const char* faultString; // REF CONST
   struct _DM_ENG_SetParameterValuesFault* next;

} __attribute((packed)) DM_ENG_SetParameterValuesFault;

DM_ENG_SetParameterValuesFault* DM_ENG_newSetParameterValuesFault(const char* name, unsigned int code);
void DM_ENG_deleteSetParameterValuesFault(DM_ENG_SetParameterValuesFault* fault);
void DM_ENG_addSetParameterValuesFault(DM_ENG_SetParameterValuesFault** pFault, DM_ENG_SetParameterValuesFault* newFault);
void DM_ENG_deleteAllSetParameterValuesFault(DM_ENG_SetParameterValuesFault** pFault);
DM_ENG_SetParameterValuesFault** DM_ENG_newTabSetParameterValuesFault(int size);
void DM_ENG_deleteTabSetParameterValuesFault(DM_ENG_SetParameterValuesFault* tFault[]);

#endif
