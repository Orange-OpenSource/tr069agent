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
 * File        : DM_ENG_DeviceIdStruct.c
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
 * @file DM_ENG_DeviceIdStruct.c
 *
 * @brief Structure that uniquely identifies the CPE. Used in Inform Messages.
 *
 **/

#include "DM_ENG_DeviceIdStruct.h"
#include <stdlib.h>
#include <string.h>

DM_ENG_DeviceIdStruct *DM_ENG_newDeviceIdStruct(char* mn, char* oui, char* pc, char* sn)
{
   DM_ENG_DeviceIdStruct* res = (DM_ENG_DeviceIdStruct*) malloc(sizeof(DM_ENG_DeviceIdStruct));
   res->manufacturer = (mn==NULL ? NULL : strdup(mn));
   res->OUI = (oui==NULL ? NULL : strdup(oui));
   res->productClass = (pc==NULL ? NULL : strdup(pc));
   res->serialNumber = (sn==NULL ? NULL : strdup(sn));
   return res;
}

void DM_ENG_deleteDeviceIdStruct(DM_ENG_DeviceIdStruct* id)
{
   if (id->manufacturer!=NULL) { free(id->manufacturer); }
   if (id->OUI!=NULL) { free(id->OUI); }
   if (id->productClass!=NULL) { free(id->productClass); }
   if (id->serialNumber!=NULL) { free(id->serialNumber); }
   free(id);
}
