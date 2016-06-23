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
 * File        : DM_ENG_SetParameterValuesFault.c
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
 * @file DM_ENG_SetParameterValuesFault.c
 *
 * @brief SetParameterValuesFault structure as defined in TR-069.
 *
 */

#include "DM_ENG_SetParameterValuesFault.h"
#include "DM_ENG_Common.h"

DM_ENG_SetParameterValuesFault* DM_ENG_newSetParameterValuesFault(const char* name, unsigned int code)
{
   DM_ENG_SetParameterValuesFault* res = (DM_ENG_SetParameterValuesFault*) malloc(sizeof(DM_ENG_SetParameterValuesFault));
   res->parameterName = strdup(name);
   res->faultCode = code;
   res->faultString = (const char*)DM_ENG_getFaultString(code);
   res->next = NULL;
   return res;
}

void DM_ENG_deleteSetParameterValuesFault(DM_ENG_SetParameterValuesFault* fault)
{
   free((char*)fault->parameterName);
   free(fault);
}

void DM_ENG_addSetParameterValuesFault(DM_ENG_SetParameterValuesFault** pFault, DM_ENG_SetParameterValuesFault* newFault)
{
   DM_ENG_SetParameterValuesFault* fault = *pFault;
   if (fault==NULL)
   {
      *pFault = newFault;
   }
   else
   {
      while (fault->next != NULL) { fault = fault->next; }
      fault->next = newFault;
   }
}

void DM_ENG_deleteAllSetParameterValuesFault(DM_ENG_SetParameterValuesFault** pFault)
{
   DM_ENG_SetParameterValuesFault* fault = *pFault;
   while (fault != NULL)
   {
      DM_ENG_SetParameterValuesFault* fa = fault;
      fault = fault->next;
      DM_ENG_deleteSetParameterValuesFault(fa);
   }
   *pFault = NULL;
}

DM_ENG_SetParameterValuesFault** DM_ENG_newTabSetParameterValuesFault(int size)
{
   return (DM_ENG_SetParameterValuesFault**)calloc(size+1, sizeof(DM_ENG_SetParameterValuesFault*));
}

void DM_ENG_deleteTabSetParameterValuesFault(DM_ENG_SetParameterValuesFault* tFault[])
{
   int i=0;
   while (tFault[i] != NULL) { DM_ENG_deleteSetParameterValuesFault(tFault[i++]); }
   free(tFault);
}
