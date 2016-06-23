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
 * File        : DM_ENG_DownloadRequest.h
 *
 * Created     : 16/03/2009
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
 * @file DM_ENG_DownloadRequest.h
 *
 * @brief The DownloadRequest represents a download request from the CPE
 *
 */

#ifndef _DM_ENG_DOWNLOAD_REQUEST_H_
#define _DM_ENG_DOWNLOAD_REQUEST_H_

#include "DM_ENG_ArgStruct.h"

/**
 * DownloadRequest Structure
 */
typedef struct _DM_ENG_DownloadRequest
{
   const char* fileType;
   DM_ENG_ArgStruct** args;
   struct _DM_ENG_DownloadRequest* next;

} __attribute((packed)) DM_ENG_DownloadRequest;

DM_ENG_DownloadRequest* DM_ENG_newDownloadRequest(const char* fileType, DM_ENG_ArgStruct* args[]);
void DM_ENG_deleteDownloadRequest(DM_ENG_DownloadRequest* dr);
void DM_ENG_addDownloadRequest(DM_ENG_DownloadRequest** pDr, DM_ENG_DownloadRequest* newDr);

#endif
