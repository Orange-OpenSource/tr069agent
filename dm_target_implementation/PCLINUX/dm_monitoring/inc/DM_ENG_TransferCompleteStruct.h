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
 * File        : DM_ENG_TransferCompleteStruct.h
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
 * @file DM_ENG_TransferCompleteStruct.h
 *
 * @brief TransferComplete data. 
 *
 * A structure representing the completion (either successful or unsuccessful) of a file transfer initiated
 * by an earlier method call or in an autonomous manner.
 */

#ifndef _DM_ENG_TRANSFER_COMPLETE_STRUCT_H_
#define _DM_ENG_TRANSFER_COMPLETE_STRUCT_H_

#include "DM_ENG_Common.h"

/**
 * A structure representing the completion (either successful or unsuccessful) of a file transfer initiated
 * by an earlier method call.
 */
typedef struct _DM_ENG_TransferCompleteStruct
{
   /** The URL on which the CPE listened to the announcements */
   char* announceURL;
   /** URL specifying the source or destination location */
   char* transferURL;
   /** File type as provided in the transfer command */
   char* fileType;
   /** Size of the file to be transfered in bytes */
   unsigned int fileSize;
   /** File name to be used on the target file system */
   char* targetFileName;

   /** Direction of the transfer : true = download, false = upload */
   bool isDownload;
   /** The CommandKey argument passed to the CPE in the Download method call that initiated the transfer. */
   char* commandKey;
   /** either 0 or error code as defined in TR-069 */ 
   unsigned int faultCode;
   /** either NULL or human readable text description corresponding to the error code */ 
   const char* faultString;
   /** Date and time transfer was started in UTC. Unknown value if error code is non-zero */
   time_t startTime; // N.B. temps donnés avec précision à la seconde
   /** Date and time transfer was fully completed in UTC. Unknown value if error code is non-zero */
   time_t completeTime;
   struct _DM_ENG_TransferCompleteStruct* next;

} __attribute((packed)) DM_ENG_TransferCompleteStruct;

DM_ENG_TransferCompleteStruct* DM_ENG_newTransferCompleteStruct(bool d, char* ck, unsigned int code, time_t start, time_t comp);
void DM_ENG_deleteTransferCompleteStruct(DM_ENG_TransferCompleteStruct* tcs);
void DM_ENG_addTransferCompleteStruct(DM_ENG_TransferCompleteStruct** pTcs, DM_ENG_TransferCompleteStruct* newTcs);
void DM_ENG_deleteAllTransferCompleteStruct(DM_ENG_TransferCompleteStruct** pTcs);

#endif
