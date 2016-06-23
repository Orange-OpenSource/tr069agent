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
 * File        : DM_ENG_AllQueuedTransferStruct.c
 *
 * Created     : 18/09/2008
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
 * @file DM_ENG_AllQueuedTransferStruct.c
 *
 * @brief Data on a scheduled transfer request.
 *
 **/

#include "DM_ENG_AllQueuedTransferStruct.h"
#include <stdlib.h>
#include <string.h>

DM_ENG_AllQueuedTransferStruct* DM_ENG_newAllQueuedTransferStruct(bool d, int s, char* ft, unsigned int fs, char* tfn, char* ck)
{
   DM_ENG_AllQueuedTransferStruct* res = (DM_ENG_AllQueuedTransferStruct*) malloc(sizeof(DM_ENG_AllQueuedTransferStruct));
   res->isDownload = d;
   res->state = s;
   res->fileType = (ft==NULL ? NULL : strdup(ft));
   res->fileSize = fs;
   res->targetFileName = (tfn==NULL ? NULL : strdup(tfn));
   res->commandKey = (ck==NULL ? NULL : strdup(ck));
   res->next = NULL;
   return res;
}

void DM_ENG_deleteAllQueuedTransferStruct(DM_ENG_AllQueuedTransferStruct* dr)
{
   if (dr->fileType!=NULL) { free(dr->fileType); }
   if (dr->targetFileName!=NULL) { free(dr->targetFileName); }
   if (dr->commandKey!=NULL) { free(dr->commandKey); }
   free(dr);
}

DM_ENG_AllQueuedTransferStruct** DM_ENG_newTabAllQueuedTransferStruct(int size)
{
   return (DM_ENG_AllQueuedTransferStruct**)calloc(size+1, sizeof(DM_ENG_AllQueuedTransferStruct*));
}

void DM_ENG_deleteTabAllQueuedTransferStruct(DM_ENG_AllQueuedTransferStruct* tTransfers[])
{
   int i=0;
   while (tTransfers[i] != NULL) { DM_ENG_deleteAllQueuedTransferStruct(tTransfers[i++]); }
   free(tTransfers);
}
