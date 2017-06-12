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
 * File        : $Workfile:   DM_ENG_Global.h  $
 *
 * Created     : 17/04/2008
 * Author      : 
 *
 * Summary description :
 *
 *---------------------------------------------------------------------------
 *
 */

#ifndef _DM_ENG_GLOBAL_H_
#define _DM_ENG_GLOBAL_H_

#include "CMN_Type_Def.h"

#define defaultCpePassword "orange"

// DataModel Prefix
extern const char* DM_ENG_PARAMETER_PREFIX;

// Names of parameters which are used by the DM Agent
extern const char* DM_TR106_ACS_URL;
extern const char* DM_TR106_ACS_USERNAME;
extern const char* DM_TR106_ACS_PASSWORD;
extern const char* DM_TR106_CONNECTIONREQUESTURL;
extern const char* DM_TR106_CONNECTIONREQUESTUSERNAME;
extern const char* DM_TR106_CONNECTIONREQUESTPASSWORD;
extern const char* DM_TR106_STUN_PASSWORD;

extern const char* DM_ENG_LAN_IP_ADDRESS_NAME;
extern const char* DM_ENG_PARAMETER_KEY_NAME;
extern const char* DM_ENG_DEVICE_STATUS_PARAMETER_NAME;

extern const char* DM_ENG_PERIODIC_INFORM_ENABLE_NAME;
extern const char* DM_ENG_PERIODIC_INFORM_INTERVAL_NAME;
extern const char* DM_ENG_PERIODIC_INFORM_TIME_NAME;

extern const char* DM_ENG_DEVICE_MANUFACTURER_NAME;
extern const char* DM_ENG_DEVICE_MANUFACTURER_OUI_NAME;
extern const char* DM_ENG_DEVICE_PRODUCT_CLASS_NAME;
extern const char* DM_ENG_DEVICE_SERIAL_NUMBER_NAME;

// Standard values for DeviceInfo.DeviceStatus (N.B. "Down" is implicit)
#define DM_ENG_DEVICE_STATUS_DOWN          "Down"
#define DM_ENG_DEVICE_STATUS_UP            "Up"
#define DM_ENG_DEVICE_STATUS_INITIALIZING  "Initializing"
#define DM_ENG_DEVICE_STATUS_ERROR         "Error"
#define DM_ENG_DEVICE_STATUS_DISABLED      "Disabled"

// Init Level
//
typedef enum _DM_ENG_InitLevel
{
   DM_ENG_InitLevel_FACTORY_RESET = 0,
   DM_ENG_InitLevel_REBOOT,
   DM_ENG_InitLevel_RESUME

} DM_ENG_InitLevel;

// Supported RPC commands
//
#define  SUPPORTED_RPC_GETRPCMETHODS           "GetRPCMethods"
#define  SUPPORTED_RPC_SETPARAMETERVALUES      "SetParameterValues"
#define  SUPPORTED_RPC_GETPARAMETERVALUES      "GetParameterValues"
#define  SUPPORTED_RPC_GETPARAMETERNAMES       "GetParameterNames"
#define  SUPPORTED_RPC_SETPARAMETERATTRIBUTES  "SetParameterAttributes"
#define  SUPPORTED_RPC_GETPARAMETERATTRIBUTES  "GetParameterAttributes"
#define  SUPPORTED_RPC_ADDOBJECT               "AddObject"
#define  SUPPORTED_RPC_DELETEOBJECT            "DeleteObject"
#define  SUPPORTED_RPC_REBOOT                  "Reboot"
#define  SUPPORTED_RPC_DOWNLOAD                "Download"
#define  SUPPORTED_RPC_UPLOAD                  "Upload"
#define  SUPPORTED_RPC_FACTORYRESET            "FactoryReset"
#define  SUPPORTED_RPC_GETQUEUEDTRANSFERS      "GetQueuedTransfers"
#define  SUPPORTED_RPC_GETALLQUEUEDTRANSFERS   "GetAllQueuedTransfers"
#define  SUPPORTED_RPC_SCHEDULEINFORM          "ScheduleInform"
#define  SUPPORTED_RPC_SETVOUCHERS             "SetVouchers"
#define  SUPPORTED_RPC_GETOPTIONS              "GetOptions"

#define  MAX_SUPPORTED_RPC_METHODS             17

extern const bool DM_ENG_IS_GETRPCMETHODS_SUPPORTED;
extern const bool DM_ENG_IS_SETPARAMETERVALUES_SUPPORTED;
extern const bool DM_ENG_IS_GETPARAMETERVALUES_SUPPORTED;
extern const bool DM_ENG_IS_GETPARAMETERNAMES_SUPPORTED;
extern const bool DM_ENG_IS_SETPARAMETERATTRIBUTES_SUPPORTED;
extern const bool DM_ENG_IS_GETPARAMETERATTRIBUTES_SUPPORTED;
extern const bool DM_ENG_IS_ADDOBJECT_SUPPORTED;
extern const bool DM_ENG_IS_DELETEOBJECT_SUPPORTED;
extern const bool DM_ENG_IS_REBOOT_SUPPORTED;
extern const bool DM_ENG_IS_DOWNLOAD_SUPPORTED;
extern const bool DM_ENG_IS_UPLOAD_SUPPORTED;
extern const bool DM_ENG_IS_FACTORYRESET_SUPPORTED;
extern const bool DM_ENG_IS_GETQUEUEDTRANSFERS_SUPPORTED;
extern const bool DM_ENG_IS_GETALLQUEUEDTRANSFERS_SUPPORTED;
extern const bool DM_ENG_IS_SCHEDULEINFORM_SUPPORTED;
extern const bool DM_ENG_IS_SETVOUCHERS_SUPPORTED;
extern const bool DM_ENG_IS_GETOPTIONS_SUPPORTED;

extern const bool DM_ENG_IS_CONNECTION_REQUEST_ALLOWED;

#endif // _DM_ENG_GLOBAL_H_
