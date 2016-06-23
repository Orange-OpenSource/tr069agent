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
 * File        : DM_ENG_ParameterValueStruct.c
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
 * @file DM_ENG_ParameterValueStruct.c
 *
 * @brief The ParameterValueStruct is based on the defined structure in TR-069 with an supplementary "type" field.
 *
 */

#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_Common.h"

/**
 * Constructs a new ParameterValueStruct object.
 * 
 * @param name Parameter name
 * @param type Parameter type as defined in DM_ENG_ParameterType.h. Can be left to UNDEFINED.
 * @param val Parameter value (as a string)
 * 
 * @return A pointer to the new ParameterValueStruct object
 */
DM_ENG_ParameterValueStruct* DM_ENG_newParameterValueStruct(const char* name, DM_ENG_ParameterType type, char* val)
{
   DM_ENG_ParameterValueStruct* res = (DM_ENG_ParameterValueStruct*) malloc(sizeof(DM_ENG_ParameterValueStruct));
   res->parameterName = strdup(name);
   res->type = type;
   res->value = (val==NULL ? NULL : strdup(val));
   res->timestamp = 0;
   res->next = NULL;
   return res;
}

/**
 * Deletes the ParameterValueStruct object.
 * 
 * @param pvs A pointer to a ParameterValueStruct object
 * 
 */
void DM_ENG_deleteParameterValueStruct(DM_ENG_ParameterValueStruct* pvs)
{
   free((char*)pvs->parameterName);
   if (pvs->value != NULL) { free(pvs->value); }
   free(pvs);
}

/**
 * Appends a new ParameterValueStruct object to the list 
 * 
 * @param pPvs Pointer to the first pointer of the list.
 * @param newPvs A pointer to a ParameterValueStruct object
 * 
 */
void DM_ENG_addParameterValueStruct(DM_ENG_ParameterValueStruct** pPvs, DM_ENG_ParameterValueStruct* newPvs)
{
   DM_ENG_ParameterValueStruct* pvs = *pPvs;
   if (pvs==NULL)
   {
      *pPvs=newPvs;
   }
   else
   {
      while (pvs->next != NULL) { pvs = pvs->next; }
      pvs->next = newPvs;
   }
}

/**
 * Deletes all the elements of the list 
 * 
 * @param pPvs Pointer to the first pointer of the list.
 * 
 */
void DM_ENG_deleteAllParameterValueStruct(DM_ENG_ParameterValueStruct** pPvs)
{
   DM_ENG_ParameterValueStruct* pvs = *pPvs;
   while (pvs != NULL)
   {
      DM_ENG_ParameterValueStruct* pv = pvs;
      pvs = pvs->next;
      DM_ENG_deleteParameterValueStruct(pv);
   }
   *pPvs = NULL;
}

/**
 * Allocates space for a null terminated array which may contains ParameterValueStruct pointer elements.
 * 
 * @param size Maximum number of array elements
 * 
 * @return An array pointer
 */
DM_ENG_ParameterValueStruct** DM_ENG_newTabParameterValueStruct(int size)
{
   return (DM_ENG_ParameterValueStruct**)calloc(size+1, sizeof(DM_ENG_ParameterValueStruct*));
}

/**
 * Copies an array of ParameterValueStruct pointer
 * 
 * @param tPvs An array pointer
 * 
 * @return An array pointer
 */
DM_ENG_ParameterValueStruct** DM_ENG_copyTabParameterValueStruct(DM_ENG_ParameterValueStruct* tPvs[])
{
   int size = 0;
   while (tPvs[size] != NULL) { size++; }
   DM_ENG_ParameterValueStruct** newTab = (DM_ENG_ParameterValueStruct**)calloc(size+1, sizeof(DM_ENG_ParameterValueStruct*));
   int i=0;
   while (tPvs[i] != NULL) { newTab[i] = DM_ENG_newParameterValueStruct(tPvs[i]->parameterName, tPvs[i]->type, tPvs[i]->value); i++; }
   return newTab; 
}

/**
 * Releases the space allocated for an array of ParameterValueStruct pointer
 * 
 * @param tPvs An array pointer
 */
void DM_ENG_deleteTabParameterValueStruct(DM_ENG_ParameterValueStruct* tPvs[])
{
   int i=0;
   while (tPvs[i] != NULL) { DM_ENG_deleteParameterValueStruct(tPvs[i++]); }
   free(tPvs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DM_ENG_ParameterName* DM_ENG_newParameterName(const char* aName)
{
   DM_ENG_ParameterName* res = (DM_ENG_ParameterName*) malloc(sizeof(DM_ENG_ParameterName));
   res->name = strdup(aName);
   res->next = NULL;
   return res;
}

void DM_ENG_deleteParameterName(DM_ENG_ParameterName* pn)
{
   free((char*)pn->name);
   free(pn);
}

void DM_ENG_addParameterName(DM_ENG_ParameterName** pPn, DM_ENG_ParameterName* newPn)
{
   DM_ENG_ParameterName* pn = *pPn;
   if (pn==NULL)
   {
      *pPn = newPn;
   }
   else
   {
      while (pn->next != NULL) { pn = pn->next; }
      pn->next = newPn;
   }
}

void DM_ENG_deleteAllParameterName(DM_ENG_ParameterName** pPn)
{
   DM_ENG_ParameterName* pn = *pPn;
   while (pn != NULL)
   {
      DM_ENG_ParameterName* p = pn;
      pn = pn->next;
      DM_ENG_deleteParameterName(p);
   }
   *pPn = NULL;
}

DM_ENG_ParameterName** DM_ENG_newTabParameterName(int size)
{
   return (DM_ENG_ParameterName**)calloc(size+1, sizeof(DM_ENG_ParameterName*));
}

void DM_ENG_deleteTabParameterName(DM_ENG_ParameterName* tPn[])
{
   int i=0;
   while (tPn[i] != NULL) { DM_ENG_deleteParameterName(tPn[i++]); }
   free(tPn);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Constructs a new SystemParameterValueStruct object.
 * 
 * @param name Parameter name
 * @param val Parameter value (as a string)
 * @param data System data
 * 
 * @return A pointer to the new SystemParameterValueStruct object
 */
DM_ENG_SystemParameterValueStruct* DM_ENG_newSystemParameterValueStruct(const char* name, char* val, char* data)
{
   DM_ENG_SystemParameterValueStruct* res = (DM_ENG_SystemParameterValueStruct*) malloc(sizeof(DM_ENG_SystemParameterValueStruct));
   res->parameterName = strdup(name);
   res->value = (val==NULL ? NULL : strdup(val));
   res->systemData = (data==NULL ? NULL : strdup(data));
   return res;
}

/**
 * Deletes the SystemParameterValueStruct object.
 * 
 * @param spvs A pointer to a SystemParameterValueStruct object
 * 
 */
void DM_ENG_deleteSystemParameterValueStruct(DM_ENG_SystemParameterValueStruct* spvs)
{
   free((char*)spvs->parameterName);
   if (spvs->value != NULL) { free(spvs->value); }
   if (spvs->systemData != NULL) { free(spvs->systemData); }
   free(spvs);
}

/**
 * Allocates space for a null terminated array which may contains SystemParameterValueStruct pointer elements.
 * 
 * @param size Maximum number of array elements
 * 
 * @return An array pointer
 */
DM_ENG_SystemParameterValueStruct** DM_ENG_newTabSystemParameterValueStruct(int size)
{
   return (DM_ENG_SystemParameterValueStruct**)calloc(size+1, sizeof(DM_ENG_SystemParameterValueStruct*));
}

/**
 * Releases the space allocated for an array of SystemParameterValueStruct pointer
 * 
 * @param tPvs An array pointer
 */
void DM_ENG_deleteTabSystemParameterValueStruct(DM_ENG_SystemParameterValueStruct* tSpvs[])
{
   int i=0;
   while (tSpvs[i] != NULL) { DM_ENG_deleteSystemParameterValueStruct(tSpvs[i++]); }
   free(tSpvs);
}
