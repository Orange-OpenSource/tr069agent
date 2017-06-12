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
 * File        : DM_ENG_TransferRequest.h
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
 * @file DM_ENG_TransferRequest.h
 *
 * @brief Data on a scheduled transfer request.
 *
 * These data are managed in a persistent manner when a transfer request is received with a non-zero delay. 
 */

#ifndef _DM_ENG_TRANSFER_REQUEST_H_
#define _DM_ENG_TRANSFER_REQUEST_H_

#include "DM_ENG_Common.h"

#define FIRMWARE_UPGRADE_IMAGE_FILE_TYPE "1 Firmware Upgrade Image"
#define WEB_CONTENT_FILE_TYPE            "2 Web Content"
#define VENDOR_CONFIGURATION_FILE_TYPE   "3 Vendor Configuration File"
#define TONE_FILE_TYPE                   "4 Tone File"
#define RINGER_FILE_TYPE                 "5 Ringer File"
#define SOFTWARE_MODULE_TYPE             "6 Software Module"
#define DATA_MODEL_EXTENSION_FILE_TYPE   "7 Data Model Extension File"

/**
 * Constantes définissant les états des requêtes
 */
typedef enum _DM_ENG_TransferRequestState
{
   _UNDEFINED = 0,
   _NOT_YET_STARTED = 1,
   _IN_PROGRESS = 2,
   _COMPLETED = 3

} DM_ENG_TransferRequestState;

/**
 * Data structure representing a scheduled transfer request
 */
typedef struct _DM_ENG_TransferRequest
{
   /** Direction of the transfer : true = download, false = upload */
   bool isDownload;
   /** Current state of the transfer : 1 (Not yet started), 2 (In progress), 3 (Completed) */
   int state;
   /** Time to perform the transfer deducted from the specified delay */
   time_t transferTime;
   /** File type as provided in the transfer command */
   char* fileType;
   /** URL specifying the source or destination location */
   char* url;
   /** Username to authenticate with de file server */
   char* username;
   /** Password to authenticate with de file server */
   char* password;
   /** Size of the file to be transfered in bytes */
   unsigned int fileSize;
   /** File name to be used on the target file system */
   char* targetFileName;
   /** URL to browse if the transfer succeeds or empty */
   char* successURL;
   /** URL to browse if the transfer fails or empty */
   char* failureURL;
   /** The CommandKey argument passed to the CPE in the Download or Upload method call that initiated the transfer. */
   char* commandKey;
   /** The URL on which the CPE listened to the announcements */
   char* announceURL;

   /* Result of the transfer when it is completed successfully or not */

   /** either 0 or error code as defined in TR-069 */ 
   unsigned int faultCode;
   /** Date and time transfer was started in UTC. Unknown value if error code is non-zero */
   time_t startTime; // N.B. temps donnés avec précision à la seconde
   /** Date and time transfer was fully completed in UTC. Unknown value if error code is non-zero */
   time_t completeTime;

   /* An integer that uniquely identifies the transfer request in the queue of transfers  */ 
   unsigned int transferId;
   struct _DM_ENG_TransferRequest* next;

} __attribute((packed)) DM_ENG_TransferRequest;

DM_ENG_TransferRequest* DM_ENG_newTransferRequest(bool d, int s, time_t t, char* ft, char* u, char* un, char* pw,
         unsigned int fs, char* tfn, char* su, char* fu, char* ck, char* au);
void DM_ENG_updateTransferRequest(DM_ENG_TransferRequest* dr, int state, unsigned int code, time_t start, time_t comp);
void DM_ENG_deleteTransferRequest(DM_ENG_TransferRequest* dr);
void DM_ENG_deleteAllTransferRequest(DM_ENG_TransferRequest** pDR);
void DM_ENG_addTransferRequest(DM_ENG_TransferRequest** pDR, DM_ENG_TransferRequest* newDR);

#endif
