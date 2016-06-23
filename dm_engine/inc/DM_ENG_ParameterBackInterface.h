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
 * File        : DM_ENG_ParameterBackInterface.h
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
 * @file DM_ENG_ParameterBackInterface.h
 *
 * @brief Interface provided to the Device Adapter to notify the DM Engine that some events occur
 *
 **/

#ifndef _DM_ENG_PARAMETERS_BACK_INTERFACE_H_
#define _DM_ENG_PARAMETERS_BACK_INTERFACE_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_Global.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_ArgStruct.h"
#include "DM_ENG_SampleDataStruct.h"

/**
 * Initializes the DM Engine.
 * 
 * @param dataPath Path to find the necessary data in working of the engine
 * @param factoryReset Indicates if the data must be resetted
 *
 * @return Returns 0 if OK or a fault code, 1 : missing data or file, 2 : I/O error, ..., 5 : ACS URL undefined, ...
 */
int DM_ENG_ParameterManager_init(const char* dataPath, DM_ENG_InitLevel level);

/**
 * Releases the resources used by the DM Engine before stopping
 */ 
void DM_ENG_ParameterManager_release();

/**
 * Waits the stopping of the DM Engine (when the device also stops)
 */
void DM_ENG_ParameterManager_join();

/**
 * Notify the DM Engine that value of one or several parameters has changed.
 * 
 * @param name Name of the parameter which changed. If the name ends with ".", this is a notification by group.
 * In this case, value changes are checked for all parameters in this group.
 */
void DM_ENG_ParameterManager_dataValueChanged(const char* name);

/**
 * Notify the DM Engine that the value of one parameters has changed and provide the new value.
 * 
 * @param name Name of the parameter which changed
 * @param value New value
 */
void DM_ENG_ParameterManager_dataNewValue(const char* name, char* value);

/**
 * Notify the DM Engine that parameter values are updated.
 *
 * @param parameterList List of name-value pairs that are updated
 */
void DM_ENG_ParameterManager_parameterValuesUpdated(DM_ENG_ParameterValueStruct* parameterList[]);

/**
 * Notify the DM Engine that a download is requested
 *
 * @param fileType The file type
 * @param args Name-value pairs
 */
void DM_ENG_ParameterManager_requestDownload(const char* fileType, DM_ENG_ArgStruct* args[]);

/**
 * Notify the DM Engine that a transfer operation completes.
 *
 * @param transferId An integer that uniquely identifies the transfer request in the queue of transfers
 * @param code Code résultat du téléchargement : 0 si OK, code d'erreur 90XX sinon
 * @param startTime Date de démarrage du téléchargement
 * @param completeTime Date de fin du téléchargement
 */
void DM_ENG_ParameterManager_transferComplete(unsigned int transferId, unsigned int code, time_t startTime, time_t completeTime);

/**
 * Notify the DM Engine that an autonomous transfer is in progress
 *
 * @param announceURL The URL on which the CPE listened to the announcements
 * @param transferURL URL specifying the source or destination location
 * @param isDownload Direction of the transfer : true = download, false = upload
 * @param fileType File type as specified in TR-069
 * @param fileSize Size of the file to be transfered in bytes
 * @param targetFileName File name to be used on the target file system
 *
 * @return A numerical transferId that must be used to notify the transfer completion 
 */
unsigned int DM_ENG_ParameterManager_autonomousTransferInProgress(char* announceURL, char* transferURL, bool isDownload, char* fileType, unsigned int fileSize, char* targetFileName);

/**
 * Notify the DM Engine that an autonomous transfer is in progress
 *
 * @param announceURL The URL on which the CPE listened to the announcements
 * @param transferURL URL specifying the source or destination location
 * @param isDownload Direction of the transfer : true = download, false = upload
 * @param fileType File type as specified in TR-069
 * @param fileSize Size of the file to be transfered in bytes
 * @param targetFileName File name to be used on the target file system
 * @param code 0 if the transfer was successful, otherwize à fault code
 * @param startTime Date and time the transfer was started in UTC 
 * @param completeTime Date and time the transfer was completed and applied in UTC
 */
void DM_ENG_ParameterManager_autonomousTransferComplete(char* announceURL, char* transferURL, bool isDownload, char* fileType, unsigned int fileSize, char* targetFileName, unsigned int code, time_t startTime, time_t completeTime);

/**
 * Notify the DM Engine that a vendor specific event occur.
 * 
 * @param OUI Organizationally Unique Identifier
 * @param event Name of an event
 */
void DM_ENG_ParameterManager_vendorSpecificEvent(const char* OUI, const char* event);

/**
 *
 */
void DM_ENG_ParameterManager_sampleData(DM_ENG_SampleDataStruct* sampleData);

#endif
