/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'.
 *---------------------------------------------------------------------------
 * File        : DM_ENG_TransferCompleteStruct.c
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
 * @file DM_ENG_TransferCompleteStruct.c
 *
 * @brief TransferComplete data. 
 *
 * A structure representing the completion (either successful or unsuccessful) of a file transfer initiated
 * by an earlier method call.
 */

#include "DM_ENG_TransferCompleteStruct.h"
#include "DM_ENG_Common.h"

DM_ENG_TransferCompleteStruct* DM_ENG_newTransferCompleteStruct(bool d, char* ck, unsigned int code, time_t start, time_t comp)
{
   DM_ENG_TransferCompleteStruct* res = (DM_ENG_TransferCompleteStruct*) malloc(sizeof(DM_ENG_TransferCompleteStruct));
   res->isDownload = d;
   res->commandKey = (ck==NULL ? NULL : strdup(ck));
   res->faultCode = code;
   res->faultString = (const char*)DM_ENG_getFaultString(code);
   res->startTime = start;
   res->completeTime = comp;
   res->next = NULL;
   return res;
}

void DM_ENG_deleteTransferCompleteStruct(DM_ENG_TransferCompleteStruct* tcs)
{
   free(tcs->commandKey);
   free(tcs);
}

void DM_ENG_addTransferCompleteStruct(DM_ENG_TransferCompleteStruct** pTcs, DM_ENG_TransferCompleteStruct* newTcs)
{
   DM_ENG_TransferCompleteStruct* tcs = *pTcs;
   if (tcs==NULL)
   {
      *pTcs = newTcs;
   }
   else
   {
      while (tcs->next != NULL) { tcs = tcs->next; }
      tcs->next = newTcs;
   }
}

void DM_ENG_deleteAllTransferCompleteStruct(DM_ENG_TransferCompleteStruct** pTcs)
{
   DM_ENG_TransferCompleteStruct* tcs = *pTcs;
   while (tcs != NULL)
   {
      DM_ENG_TransferCompleteStruct* tc = tcs;
      tcs = tcs->next;
      DM_ENG_deleteTransferCompleteStruct(tc);
   }
   *pTcs = NULL;
}
