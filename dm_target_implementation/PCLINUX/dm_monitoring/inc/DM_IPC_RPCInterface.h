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
 * File        : DM_IPC_RPCInterface.h
 *
 * Created     : 12/07/2011
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
 * @file DM_IPC_RPCInterface.h
 *
 * @brief The RPC Interface.
 * 
 * This header file defines the RPC Interface which provides remotely similar functions to the TR-069 RPC methods
 *
 */

#ifndef _DM_IPC_RPC_INTERFACE_H_
#define _DM_IPC_RPC_INTERFACE_H_

#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_SetParameterValuesFault.h"
#include "CMN_Type_Def.h"

/**
 * Initiates the DBUS Connection
 *
 * @return 0 if OK, 9800 if the connection can not be initiated
 */
int DM_IPC_InitConnection();

/**
 * Closes the DBUS Connection
 */
void DM_IPC_CloseConnection();

/**
 * This function may be used by the subscriber to modify the value of one or more CPE Parameters.
 *
 * @param parameterList Array of name-value pairs as specified. For each name-value pair, the CPE is instructed
 * to set the Parameter specified by the name to the corresponding value.
 * 
 * @param parameterKey The value to set the ParameterKey parameter. This MAY be used by the server to identify
 * Parameter updates, or left empty.
 * 
 * @param pStatus Pointer to get the status defined as follows :
 * - DM_ENG_ParameterStatus_APPLIED (=0) : Parameter changes have been validated and applied.
 * - DM_ENG_ParameterStatus_READY_TO_APPLY (=1) : Parameter changes have been validated and committed, but not yet applied
 * (for example, if a reboot is required before the new values are applied).
 * - DM_ENG_ParameterStatus_UNDEFINED : Undefined status (for example, parameterList is empty).
 *
 *  Note : The result is DM_ENG_ParameterStatus_APPLIED if all the parameter changes are APPLIED.
 * The result is DM_ENG_ParameterStatus_READY_TO_APPLY if one or more parameter changes is READY_TO_APPLY.
 * 
 * @param pFaults Pointer to get the faults struct if any, as specified in TR-069
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069, or 9800 for DBUS Error
 */
int DM_IPC_SetParameterValues(DM_ENG_ParameterValueStruct* parameterList[], char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus, OUT DM_ENG_SetParameterValuesFault** pFaults[]);

/**
 * This function may be used by the subscriber to obtain the value of one or more CPE parameters.
 * 
 * @param parameterNames Array of strings, each representing the name of the requested parameter.
 * @param Pointer to the array of the ParameterValueStruct.
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069, or 9800 for DBUS Error
 */
int DM_IPC_GetParameterValues(char* parameterNames[], OUT DM_ENG_ParameterValueStruct** pResult[]);

/**
 * Notify the DM Engine that a vendor specific event occurs.
 * 
 * @param OUI Organizationally Unique Identifier
 * @param event Name of an event
 * 
 * @return 0 if OK, or 9800 for DBUS Error
 */
int DM_IPC_VendorSpecificEvent(const char* OUI, const char* event);

#endif
