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
 * File        : DM_ENG_DiagnosticsLauncher.c
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
 * @file DM_ENG_DiagnosticsLauncher.c
 *
 * @brief 
 *
 **/

#include "DM_ENG_DiagnosticsLauncher.h"
#include "DM_ENG_ParameterManager.h"
#include "DM_ENG_ParameterBackInterface.h"
#include "DM_ENG_Device.h"
#include "DM_CMN_Thread.h"
#include "CMN_Trace.h"

char* DM_ENG_NONE_STATE      = "None";
char* DM_ENG_REQUESTED_STATE = "Requested";
char* DM_ENG_COMPLETE_STATE  = "Complete";
char* DM_ENG_ERROR_STATE     = "Error";
char* DM_ENG_ERROR_CANNOT_RESOLVE_STATE = "Error_CannotResolveHostName";
char* DM_ENG_ERROR_MAX_HOP_STATE  = "Error_MaxHopCountExceeded";
char* DM_ENG_ERROR_INTERNAL_STATE = "Error_Internal";
char* DM_ENG_ERROR_OTHER_STATE    = "Error_Other";

char* DM_ENG_DIAGNOSTICS_OBJECT_SUFFIX = "Diagnostics.";
char* DM_ENG_DIAGNOSTICS_STATE_SUFFIX = ".DiagnosticsState";

static DM_CMN_ThreadId_t _diagnosticsLauncherId = 0;

/*
 *
 */
static void *launchDiagnostics(void *data)
{
   DM_ENG_ParameterManager_lock();
   DBG("Diagnostics - Launching");
   const char* objName = (const char*)data;
   int objLen = strlen(objName);
   DM_ENG_ParameterValueStruct* paramList = NULL;
   int nbParam = 0;
   char* prmName;
   for (prmName = DM_ENG_ParameterManager_getFirstParameterName(); prmName!=NULL; prmName = DM_ENG_ParameterManager_getNextParameterName())
   {
      if (((int)strlen(prmName) > objLen) && (strncmp(prmName, objName, objLen)==0) && DM_ENG_Parameter_isValuable(prmName))
      {
         DM_ENG_Parameter* param = DM_ENG_ParameterManager_getCurrentParameter();
         DM_ENG_addParameterValueStruct(&paramList, DM_ENG_newParameterValueStruct(param->name, param->type, param->value));
         nbParam++;
      }
   }
   DM_ENG_ParameterValueStruct** paramTab = DM_ENG_newTabParameterValueStruct(nbParam);
   int i;
   for (i=0; i<nbParam; i++)
   {
      paramTab[i] = paramList;
      paramList = paramList->next;
   }
   DBG("Diagnostics - Launched");
   DM_ENG_ParameterManager_unlock();

   DM_ENG_ParameterValueStruct** result = NULL;
   int res = DM_ENG_Device_performDiagnostics(objName, paramTab, &result);
   if (res == 0) // résultat rendu de façon synchrone
   {
      DM_ENG_ParameterManager_convertNamesToLong(objName, result);
      DM_ENG_ParameterManager_parameterValuesUpdated(result);
      DM_ENG_deleteTabParameterValueStruct(result);
   }
   DM_ENG_deleteTabParameterValueStruct(paramTab);
   DM_ENG_FREE(data);
   return 0;
}

void DM_ENG_Diagnostics_initDiagnostics(const char* name)
{
   DM_CMN_Thread_create(launchDiagnostics, strdup(name), false, &_diagnosticsLauncherId);
}
