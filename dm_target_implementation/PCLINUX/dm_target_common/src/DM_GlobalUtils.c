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
 * File        : DM_GlobalUtils.c
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
 * @file DM_GlobalUtils.c
 *
 * @brief 
 *
 **/
 
#include "CMN_Trace.h"
#include "DM_GlobalDefs.h"

const bool DM_ENG_IS_GETRPCMETHODS_SUPPORTED = _GETRPCMETHODS_RPC_SUPPORTED;
const bool DM_ENG_IS_SETPARAMETERVALUES_SUPPORTED = _SETPARAMETERVALUES_RPC_SUPPORTED;
const bool DM_ENG_IS_GETPARAMETERVALUES_SUPPORTED = _GETPARAMETERVALUES_RPC_SUPPORTED;
const bool DM_ENG_IS_GETPARAMETERNAMES_SUPPORTED = _GETPARAMETERNAMES_RPC_SUPPORTED;
const bool DM_ENG_IS_SETPARAMETERATTRIBUTES_SUPPORTED = _SETPARAMETERATTRIBUTES_RPC_SUPPORTED;
const bool DM_ENG_IS_GETPARAMETERATTRIBUTES_SUPPORTED = _GETPARAMETERATTRIBUTES_RPC_SUPPORTED;
const bool DM_ENG_IS_ADDOBJECT_SUPPORTED = _ADDOBJECT_RPC_SUPPORTED;
const bool DM_ENG_IS_DELETEOBJECT_SUPPORTED = _DELETEOBJECT_RPC_SUPPORTED;
const bool DM_ENG_IS_REBOOT_SUPPORTED = _REBOOT_RPC_SUPPORTED;
const bool DM_ENG_IS_DOWNLOAD_SUPPORTED = _DOWNLOAD_RPC_SUPPORTED;
const bool DM_ENG_IS_UPLOAD_SUPPORTED = _UPLOAD_RPC_SUPPORTED;
const bool DM_ENG_IS_FACTORYRESET_SUPPORTED = _FACTORYRESET_RPC_SUPPORTED;
const bool DM_ENG_IS_GETQUEUEDTRANSFERS_SUPPORTED = _GETQUEUEDTRANSFERS_RPC_SUPPORTED;
const bool DM_ENG_IS_GETALLQUEUEDTRANSFERS_SUPPORTED = _GETALLQUEUEDTRANSFERS_RPC_SUPPORTED;
const bool DM_ENG_IS_SCHEDULEINFORM_SUPPORTED = _SCHEDULEINFORM_RPC_SUPPORTED;
const bool DM_ENG_IS_SETVOUCHERS_SUPPORTED = _SETVOUCHERS_RPC_SUPPORTED;
const bool DM_ENG_IS_GETOPTIONS_SUPPORTED = _GETOPTIONS_RPC_SUPPORTED;

const bool DM_ENG_IS_CONNECTION_REQUEST_ALLOWED = _CONNECTION_REQUEST_ALLOWED;

const bool DM_COM_IS_FORCED_CWMP_1_0_USAGE = _FORCE_CWMP_1_0_USAGE;

/////////////////////////////////////////////////////////////////
//   PARAMETER NAMES
/////////////////////////////////////////////////////////////////

const char* DM_ENG_PARAMETER_PREFIX = DM_PREFIX;

// Parameter names definitions
const char* DM_TR106_ACS_URL                    = DM_PREFIX "ManagementServer.URL";
const char* DM_TR106_ACS_USERNAME               = DM_PREFIX "ManagementServer.Username";
const char* DM_TR106_ACS_PASSWORD               = DM_PREFIX "ManagementServer.Password";
const char* DM_TR106_CONNECTIONREQUESTURL       = DM_PREFIX "ManagementServer.ConnectionRequestURL";
const char* DM_TR106_CONNECTIONREQUESTUSERNAME  = DM_PREFIX "ManagementServer.ConnectionRequestUsername";
const char* DM_TR106_CONNECTIONREQUESTPASSWORD  = DM_PREFIX "ManagementServer.ConnectionRequestPassword";
const char* DM_TR106_STUN_PASSWORD              = DM_PREFIX "ManagementServer.STUNPassword";

const char* DM_ENG_LAN_IP_ADDRESS_NAME          = DM_PREFIX "LAN.IPAddress";
const char* DM_ENG_PARAMETER_KEY_NAME           = DM_PREFIX "ManagementServer.ParameterKey";
const char* DM_ENG_DEVICE_STATUS_PARAMETER_NAME = DM_PREFIX "DeviceInfo.DeviceStatus";

const char* DM_ENG_PERIODIC_INFORM_ENABLE_NAME  = DM_PREFIX "ManagementServer.PeriodicInformEnable";
const char* DM_ENG_PERIODIC_INFORM_INTERVAL_NAME = DM_PREFIX "ManagementServer.PeriodicInformInterval";
const char* DM_ENG_PERIODIC_INFORM_TIME_NAME    = DM_PREFIX "ManagementServer.PeriodicInformTime";

const char* DM_ENG_DEVICE_MANUFACTURER_NAME     = DM_PREFIX "DeviceInfo.Manufacturer";
const char* DM_ENG_DEVICE_MANUFACTURER_OUI_NAME = DM_PREFIX "DeviceInfo.ManufacturerOUI";
const char* DM_ENG_DEVICE_PRODUCT_CLASS_NAME    = DM_PREFIX "DeviceInfo.ProductClass";
const char* DM_ENG_DEVICE_SERIAL_NUMBER_NAME    = DM_PREFIX "DeviceInfo.SerialNumber";

/////////////////////////////////////////////////////////////////
//   CONNECTION REQUEST URL
/////////////////////////////////////////////////////////////////

const int CPE_URL_SIZE = _CPE_URL_SIZE;
const int CPE_PORT     = _CPE_PORT;
