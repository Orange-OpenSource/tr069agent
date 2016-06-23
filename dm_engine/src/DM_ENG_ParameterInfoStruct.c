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
 * File        : DM_ENG_ParameterInfoStruct.c
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
 * @file DM_ENG_ParameterInfoStruct.c
 *
 * @brief The ParameterInfoStruct as defined in TR-069.
 *
 **/

#include "DM_ENG_ParameterInfoStruct.h"

DM_ENG_ParameterInfoStruct* DM_ENG_newParameterInfoStruct(char* name, bool wr)
{
   DM_ENG_ParameterInfoStruct* res = (DM_ENG_ParameterInfoStruct*) malloc(sizeof(DM_ENG_ParameterInfoStruct));
   res->parameterName = strdup(name);
   res->writable = wr;
   res->next = NULL;
   return res;
}

void DM_ENG_deleteParameterInfoStruct(DM_ENG_ParameterInfoStruct* info)
{
   free(info->parameterName);
   free(info);
}

void DM_ENG_deleteAllParameterInfoStruct(DM_ENG_ParameterInfoStruct** pInfo)
{
   DM_ENG_ParameterInfoStruct* info = *pInfo;
   while (info != NULL)
   {
      DM_ENG_ParameterInfoStruct* in = info;
      info = info->next;
      DM_ENG_deleteParameterInfoStruct(in);
   }
   *pInfo = NULL;
}

DM_ENG_ParameterInfoStruct** DM_ENG_newTabParameterInfoStruct(int size)
{
   return (DM_ENG_ParameterInfoStruct**)calloc(size+1, sizeof(DM_ENG_ParameterInfoStruct*));
}

void DM_ENG_deleteTabParameterInfoStruct(DM_ENG_ParameterInfoStruct* tInfo[])
{
   int i=0;
   while (tInfo[i] != NULL) { DM_ENG_deleteParameterInfoStruct(tInfo[i++]); }
   free(tInfo);
}

void DM_ENG_addParameterInfoStruct(DM_ENG_ParameterInfoStruct** pInfo, DM_ENG_ParameterInfoStruct* newInfo)
{
   DM_ENG_ParameterInfoStruct* info = *pInfo;
   if (info==NULL)
   {
      *pInfo = newInfo;
   }
   else
   {
      while (info->next != NULL) { info = info->next; }
      info->next = newInfo;
   }
}
