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
 * File        : DM_COM_GenericHttpClientInterface.h
 *
 * Created     : 2008/06/05
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
 * @file GenericHttpClientInterface.h
 *
 * @brief Definition the Generic HTTP API
 *
 * This file defines the generic API
 * used to acces to the HTTP Services
 *
 */
 
#ifndef _GENERIC_HTTP_CLIENT_INTERFACE_H
#define _GENERIC_HTTP_CLIENT_INTERFACE_H

#include "dm_com.h"

// Http Close Mode definition
#define NORMAL_CLOSE    (0) // Normal close mode
#define IMMEDIATE_CLOSE (1) // Use this value when the agent is stopped



typedef struct httpgetfiledatatype{
  char * fileURLStr;            // URL of the file to download
  char * usernameStr;           // Username for authentication
  char * passwordStr;           // Password for authentication
  int    filSize;               // File size
  char * targetFileNameStr;     // Local File Name
} __attribute((packed)) httpGetFileDataType;

typedef struct httpputfiledatatype{
  char * destURLStr;            // URL of the destination upload
  char * usernameStr;           // Username for authentication
  char * passwordStr;           // Password for authentication
  char * localFileNameStr;      // File to be uploaded
} __attribute((packed)) httpPutFileDataType;


int DM_ConfigureHttpClient(const char * acsUrl,
                           const char * acsUsername,
                           const char * acsPassword,
                           const char * sslDevicePublicCertificatFile,
                           const char * sslDeviceRsaKeyFile,
                           const char * sslAcsCertificationAuthorityFile);


/*
* @brief Function used to free HTTP Client Data Strucutures
*
* @param void
*
* @return 0 on success (-1 otherwise)
*
*/
int DM_StopHttpClient(void);


/*
* @brief Function used to send an HTTP Message
*
* @param IN: ptr on the message to send
*
* @return 0 on success (-1 otherwise)
*
*/
int DM_SendHttpMessage(IN const char * msgToSendStr);


/*
* @brief Function used to get a file using HTTP GET.
*
* @param IN: char * fileURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: int    filSize
*        IN: char * targetFileNameStr 
*
* @return The FW Upgrade Error Code
*
*/
int DM_HttpGetFile(IN httpGetFileDataType * httpGetFileDataPtr);

/*
* @brief Function used to upload a file using HTTP PUT.
*
* @param IN: char * destURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: char * localFileNameStr 
*
* @return The FW Upload Error Code
*
*/
int DM_HttpPutFile(httpPutFileDataType * httpPutFileDataPtr);


/*
* @brief Function used to upload a file using FTP PUT.
*
* @param IN: char * destURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: char * localFileNameStr 
*
* @return The FW Upload Error Code
*
*/
int DM_FtpPutFile(httpPutFileDataType * httpPutFileDataPtr);

/*
* @brief Function used to close the HTTP Client Session
*
* @param closeMode (NORMAL_CLOSE or IMMEDIATE_CLOSE)
*
* @return 0 on success (-1 otherwise)
*
*/
int DM_CloseHttpSession(bool closeMode);


#endif /* _GENERIC_HTTP_CLIENT_INTERFACE_H */
 
 
