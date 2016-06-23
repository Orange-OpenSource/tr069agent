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
 * File        : DM_ENG_TransferRequestStruct.c
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
 * @file DM_ENG_TransferRequestStruct.c
 *
 * @brief 
 *
 **/

#include "DM_ENG_TransferResultStruct.h"
#include <stdlib.h>

DM_ENG_TransferResultStruct* DM_ENG_newTransferResultStruct(DM_ENG_ParameterStatus st, time_t start, time_t comp)
{
   DM_ENG_TransferResultStruct* res = (DM_ENG_TransferResultStruct*) malloc(sizeof(DM_ENG_TransferResultStruct));
   res->status = st;
   res->startTime = start;
   res->completeTime = comp;
   return res;
}

void DM_ENG_deleteTransferResultStruct(DM_ENG_TransferResultStruct* result)
{
   free(result);
}
