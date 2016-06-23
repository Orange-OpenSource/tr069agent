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
 * File        : DM_ENG_AllQueuedTransferStruct.h
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
 * @file DM_ENG_AllQueuedTransferStruct.h
 *
 * @brief Data on a scheduled transfer request.
 *
 */

#ifndef _DM_ENG_ALL_QUEUED_TRANSFER_STRUCT_H_
#define _DM_ENG_ALL_QUEUED_TRANSFER_STRUCT_H_

#include "DM_ENG_Common.h"

/**
 * Data structure representing a scheduled transfer request
 */
typedef struct _DM_ENG_AllQueuedTransferStruct
{
   /** Direction of the transfer : 0 = download, 1 = upload */
   bool isDownload;
   /** Current state of the transfer : 1 (Not yet started), 2 (In progress), 3 (Completed) */
   int state;
   /** File type as provided in the transfer command */
   char* fileType;
   /** Size of the file to be transfered in bytes */
   unsigned int fileSize;
   /** File name to be used on the target file system */
   char* targetFileName;
   /** Key associated to the transfer command */
   char* commandKey;
   struct _DM_ENG_AllQueuedTransferStruct* next;

} __attribute((packed)) DM_ENG_AllQueuedTransferStruct;

DM_ENG_AllQueuedTransferStruct* DM_ENG_newAllQueuedTransferStruct(bool d, int s, char* ft, unsigned int fs, char* tfn, char* ck);
void DM_ENG_deleteAllQueuedTransferStruct(DM_ENG_AllQueuedTransferStruct* dr);
DM_ENG_AllQueuedTransferStruct** DM_ENG_newTabAllQueuedTransferStruct(int size);
void DM_ENG_deleteTabAllQueuedTransferStruct(DM_ENG_AllQueuedTransferStruct* tInfo[]);

#endif
