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
 * File        : DM_ENG_TransferRequest.c
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
 * @file DM_ENG_TransferRequest.c
 *
 * @brief Data on a scheduled transfer request.
 *
 * These data are managed in a persistent manner when a transfer request is received with a non-zero delay. 
 **/

#include "DM_ENG_TransferRequest.h"
#include <stdlib.h>
#include <string.h>

DM_ENG_TransferRequest* DM_ENG_newTransferRequest(bool d, int s, time_t t, char* ft, char* u, char* un, char* pw,
         unsigned int fs, char* tfn, char* su, char* fu, char* ck, char* au)
{
   DM_ENG_TransferRequest* res = (DM_ENG_TransferRequest*) malloc(sizeof(DM_ENG_TransferRequest));
   res->isDownload = d;
   res->state = s;
   res->transferTime = t;
   res->fileType = (ft==NULL ? NULL : strdup(ft));
   res->url = (u==NULL ? NULL : strdup(u));
   res->username = (un==NULL ? NULL : strdup(un));
   res->password = (pw==NULL ? NULL : strdup(pw));
   res->fileSize = fs;
   res->targetFileName = (tfn==NULL ? NULL : strdup(tfn));
   res->successURL = (su==NULL ? NULL : strdup(su));
   res->failureURL = (fu==NULL ? NULL : strdup(fu));
   res->commandKey = (ck==NULL ? NULL : strdup(ck));
   res->announceURL = (au==NULL ? NULL : strdup(au));

   res->faultCode = 0;
   res->startTime = 0;
   res->completeTime = 0;
   res->transferId = 0;
   res->next = NULL;
   return res;
}

void DM_ENG_updateTransferRequest(DM_ENG_TransferRequest* dr, int state, unsigned int code, time_t start, time_t comp)
{
   dr->state = state;
   dr->faultCode = code;
   dr->startTime = start;
   dr->completeTime = comp;   
}

void DM_ENG_deleteTransferRequest(DM_ENG_TransferRequest* dr)
{
   if (dr->fileType!=NULL) { free(dr->fileType); }
   if (dr->url!=NULL) { free(dr->url); }
   if (dr->username!=NULL) { free(dr->username); }
   if (dr->password!=NULL) { free(dr->password); }
   if (dr->targetFileName!=NULL) { free(dr->targetFileName); }
   if (dr->successURL!=NULL) { free(dr->successURL); }
   if (dr->failureURL!=NULL) { free(dr->failureURL); }
   if (dr->commandKey!=NULL) { free(dr->commandKey); }
   if (dr->announceURL!=NULL) { free(dr->announceURL); }
   free(dr);
}

void DM_ENG_deleteAllTransferRequest(DM_ENG_TransferRequest** pDR)
{
   DM_ENG_TransferRequest* dr = *pDR;
   while (dr != NULL)
   {
      DM_ENG_TransferRequest* d = dr;
      dr = dr->next;
      DM_ENG_deleteTransferRequest(d);
   }
   *pDR = NULL;
}

void DM_ENG_addTransferRequest(DM_ENG_TransferRequest** pDR, DM_ENG_TransferRequest* newDR)
{
   DM_ENG_TransferRequest* dr = *pDR;
   if (dr==NULL)
   {
      *pDR = newDR;
   }
   else
   {
      // insère le newDR en respectant l'ordre croissant des transferTime
      while ((dr->next != NULL) && (dr->next->transferTime <= newDR->transferTime)) { dr = dr->next; }
      if (dr->next != NULL) { newDR->next = dr->next; }
      dr->next = newDR;
   }
}
