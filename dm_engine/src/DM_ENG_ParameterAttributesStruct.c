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
 * File        : DM_ENG_ParameterAttributesStruct.c
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
 * @file DM_ENG_ParameterAttributesStruct.c
 *
 * @brief The DM_ENG_ParameterAttributesStruct is based on the defined SetParameterAttributesStruct structure in TR-069.
 *
 */

#include "DM_ENG_ParameterAttributesStruct.h"
#include "DM_ENG_EntityType.h"
#include "DM_ENG_Common.h"

DM_ENG_ParameterAttributesStruct* DM_ENG_newParameterAttributesStruct(char* name, DM_ENG_NotificationMode notif, char* al[])
{
   DM_ENG_ParameterAttributesStruct* res = (DM_ENG_ParameterAttributesStruct*) malloc(sizeof(DM_ENG_ParameterAttributesStruct));
   res->parameterName = strdup(name);
   res->notification = notif;
   res->accessList = (al == (char**)DM_ENG_DEFAULT_ACCESS_LIST ? (char**)DM_ENG_DEFAULT_ACCESS_LIST : DM_ENG_copyTabString(al));
   res->next = NULL;
   return res;
}

void DM_ENG_deleteParameterAttributesStruct(DM_ENG_ParameterAttributesStruct* attr)
{
   free(attr->parameterName);
   if ((attr->accessList != NULL) && (attr->accessList != (char**)DM_ENG_DEFAULT_ACCESS_LIST)) { DM_ENG_deleteTabString(attr->accessList); }
   free(attr);
}

void DM_ENG_deleteAllParameterAttributesStruct(DM_ENG_ParameterAttributesStruct** pAttr)
{
   DM_ENG_ParameterAttributesStruct* attr = *pAttr;
   while (attr != NULL)
   {
      DM_ENG_ParameterAttributesStruct* at = attr;
      attr = attr->next;
      DM_ENG_deleteParameterAttributesStruct(at);
   }
   *pAttr = NULL;
}

DM_ENG_ParameterAttributesStruct** DM_ENG_newTabParameterAttributesStruct(int size)
{
   return (DM_ENG_ParameterAttributesStruct**)calloc(size+1, sizeof(DM_ENG_ParameterAttributesStruct*));
}

void DM_ENG_deleteTabParameterAttributesStruct(DM_ENG_ParameterAttributesStruct* tAttr[])
{
   int i=0;
   while (tAttr[i] != NULL) { DM_ENG_deleteParameterAttributesStruct(tAttr[i++]); }
   free(tAttr);
}

void DM_ENG_addParameterAttributesStruct(DM_ENG_ParameterAttributesStruct** pAttr, DM_ENG_ParameterAttributesStruct* newAttr)
{
   DM_ENG_ParameterAttributesStruct* attr = *pAttr;
   if (attr==NULL)
   {
      *pAttr = newAttr;
   }
   else
   {
      while (attr->next != NULL) { attr = attr->next; }
      attr->next = newAttr;
   }
}
