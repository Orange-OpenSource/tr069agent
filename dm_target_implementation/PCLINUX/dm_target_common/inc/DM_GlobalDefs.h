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
 * File        : DM_GlobalDefs.h
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
 * @file DM_GlobalDefs.h
 *
 * @brief main header to tune target specific parameters parameter 
 *
 **/
 


#ifndef _DM_GLOBALDEFS_H_
#define _DM_GLOBALDEFS_H_

#include "CMN_Type_Def.h"

#ifndef _InternetGatewayDevice_ /* This flag is set in the main Makefile when DEVICE_TYPE is specified (IGD or D) */
/* Prefix type is Device*/
#define DM_PREFIX "Device."
#else
/* Prefix type is InternetGatewayDevice*/
#define DM_PREFIX "InternetGatewayDevice."
#endif

// Suppress the definition of DM_COM_CLOSE_ON_HTTP_200_ALLOWED to prohibit the session closure with HTTP 200
#define DM_COM_CLOSE_ON_HTTP_200_ALLOWED

#define DM_TR106_MAXCONNECTIONREQUEST       DM_PREFIX "ManagementServer.X_ORANGE-COM_MaxConnectionRequest"
#define DM_TR106_MAXCONNECTIONFREQ          DM_PREFIX "ManagementServer.X_ORANGE-COM_FreqConnectionRequest"
// Key to retrieve the WAN Ip address from the datamodel.
#define   DM_TR106_WANIP_ADDRESS            DM_PREFIX "Services.X_ORANGE-COM_Internet.WANIPConnection"  
#define   DM_TR106_WANIP_EXTERNALIPADDRESS  DM_PREFIX "WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.ExternalIPAddress"  

extern char* DATA_PATH;


/* --------- Supported feature declaration ----------------- */
#define _CONNECTION_REQUEST_ALLOWED              true // An http server must be started or not for connection request
#define _GETRPCMETHODS_RPC_SUPPORTED             true
#define _SETPARAMETERVALUES_RPC_SUPPORTED        true
#define _GETPARAMETERVALUES_RPC_SUPPORTED        true
#define _SETPARAMETERATTRIBUTES_RPC_SUPPORTED    true
#define _GETPARAMETERATTRIBUTES_RPC_SUPPORTED    true
#define _GETPARAMETERNAMES_RPC_SUPPORTED         true
#define _ADDOBJECT_RPC_SUPPORTED                 true
#define _DELETEOBJECT_RPC_SUPPORTED              true
#define _REBOOT_RPC_SUPPORTED                    true
#define _DOWNLOAD_RPC_SUPPORTED                  true
#define _FACTORYRESET_RPC_SUPPORTED              true
#define _SCHEDULEINFORM_RPC_SUPPORTED            true
#define _UPLOAD_RPC_SUPPORTED                    true
#define _GETQUEUEDTRANSFERS_RPC_SUPPORTED        false  // Not implemented
#define _GETALLQUEUEDTRANSFERS_RPC_SUPPORTED     true
#define _SETVOUCHERS_RPC_SUPPORTED               false  // Not implemented
#define _GETOPTIONS_RPC_SUPPORTED                false  // Not implemented

#define _FORCE_CWMP_1_0_USAGE                    true   // Karma supports only cwmp-1-0

/* ------ Define this macro to prevent the DM Agent to implement the persistence of the scheduled inform ------- */
// #define NO_PERSISTENT_SCHEDULED_INFORM 1

// Size of the CPE URL
#define _CPE_URL_SIZE  16
// CPE PORT VALUE - Port assigned by IANA for CWMP : 7547
#define _CPE_PORT      50805

#endif /* _DM_GLOBALDEFS_H_ */
