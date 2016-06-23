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
 * File        : DM_IPC_DBusTest.c
 *
 * Created     : 18/07/2011
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
 * @file DM_IPC_DBusTest.h
 *
 * @brief Test of the RPC Interface via DBus
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "DM_IPC_RPCInterface.h"
#include "DM_ENG_Common.h"

static void _testGetParameterValues()
{
   char* parameterNames[3] = { "Device.DeviceInfo.", "Device.ManagementServer.", NULL };
   DM_ENG_ParameterValueStruct** result;
   printf("DM_IPC_GetParameterValues( %s ... )\n", parameterNames[0]);
   int res = DM_IPC_GetParameterValues(parameterNames, &result);
   if (res != 0)
   {
      printf("Erreur %d\n", res);
   }
   else
   {
      int nbParam = DM_ENG_tablen((void**)result);
      int i;
      for (i=0; i<nbParam; i++)
      {
         printf("%d: %s=%s\n", i, result[i]->parameterName, result[i]->value);
      }
      DM_ENG_deleteTabParameterValueStruct(result);
   }
}

static void _testSetParameterValues()
{
   DM_ENG_ParameterValueStruct* parameterList[3];
   parameterList[0] = DM_ENG_newParameterValueStruct("Device.ManagementServer.PeriodicInformInterval", DM_ENG_ParameterType_INT, "31");
   parameterList[1] = DM_ENG_newParameterValueStruct("Device.DeviceInfo.ProvisioningCode", DM_ENG_ParameterType_STRING, "pr31");
   parameterList[2] = NULL;
   char* parameterKey = "0031";
   DM_ENG_ParameterStatus status;
   DM_ENG_SetParameterValuesFault** faults;
   printf("DM_IPC_SetParameterValues( %s=%s ... )\n", parameterList[0]->parameterName, parameterList[0]->value);
   int res = DM_IPC_SetParameterValues(parameterList, parameterKey, &status, &faults);
   if (res != 0)
   {
      printf("Erreur %d\n", res);
      if (res == 9003)
      {
         int nbFaults = DM_ENG_tablen((void**)faults);
         int i;
         for (i=0; i<nbFaults; i++)
         {
            printf("%d: %s:%d\n", i, faults[i]->parameterName, faults[i]->faultCode);
         }
         DM_ENG_deleteTabSetParameterValuesFault(faults);
      }
   }
   else
   {
      printf("SetParameterValues OK - status=%d\n", status);
   }
   DM_ENG_deleteParameterValueStruct(parameterList[0]);
   DM_ENG_deleteParameterValueStruct(parameterList[1]);
}

static void _testVendorSpecificEvent()
{
   char* OUI = "123456";
   char* event = "IPC_Test";
   int res = DM_IPC_VendorSpecificEvent(OUI, event);
   if (res != 0)
   {
      printf("Erreur %d\n", res);
   }
   else
   {
      printf("VendorSpecificEvent OK\n");
   }
}

/*
 * Test
 *
 * An index argument between 0 and 9 may be given. The default value is 0.
 * If several subscribers are using the IPC Interface, they must have different index.
 *
 */
int main()
{
   int res = DM_IPC_InitConnection();
   if (res == 0)
   {
      printf("Test GetParameterValues...\n");
      _testGetParameterValues();

      printf("Test SetParameterValues (Entrée pour continuer)...");
      getchar();
      printf("\n");

      _testSetParameterValues();

      printf("Test Vendor Specific Event (Entrée pour continuer)...");
      getchar();
      printf("\n");

      _testVendorSpecificEvent();

      DM_IPC_CloseConnection();
   }

   return 0;
}
