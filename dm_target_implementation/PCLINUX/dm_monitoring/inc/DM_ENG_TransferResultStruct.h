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
 * File        : DM_ENG_TransferRequestStruct.h
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
 * @file DM_ENG_TransferRequestStruct.h
 *
 * @brief 
 *
 **/

#ifndef _DM_ENG_TRANSFER_RESULT_STRUCT_H_
#define _DM_ENG_TRANSFER_RESULT_STRUCT_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterStatus.h"

typedef struct _DM_ENG_TransferResultStruct
{
   DM_ENG_ParameterStatus status;
   time_t startTime; // N.B. temps donnés avec précision à la seconde
   time_t completeTime;
} __attribute((packed)) DM_ENG_TransferResultStruct;

DM_ENG_TransferResultStruct* DM_ENG_newTransferResultStruct(DM_ENG_ParameterStatus st, time_t start, time_t comp);
void DM_ENG_deleteTransferResultStruct(DM_ENG_TransferResultStruct* result);

#endif
