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
 * File        : DM_ENG_Device.h
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
 * @file DM_ENG_Device.h
 *
 * @brief Low API from the DM Engine to the Device Adapter 
 *
 **/

#ifndef _DM_ENG_DEVICE_H_
#define _DM_ENG_DEVICE_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_SampleDataStruct.h"

/**
 * Performs the necessary initializations of the device adapter if any, when starting the DM Agent.
 */ 
void DM_ENG_Device_init();

/**
 * Performs the necessary operations when stopping the DM Agent.
 */ 
void DM_ENG_Device_release();

/**
 * Notify the Device Adapter of the beginning of a session with the ACS. It allows to perform the necessary initializations.
 */ 
void DM_ENG_Device_openSession();

/**
 * Notify the Device Adapter of the end of the session with the ACS.
 */ 
void DM_ENG_Device_closeSession();

/**
 * Gets the parameter value.
 * 
 * @param name Name of one system parameter
 * @param data Data string possibly used to perform the getValue
 * @param pVal Pointer to char* which will get the result
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_getValue(const char* name, const char* data, OUT char** pVal);

/**
 * Sets the values of the system parameters.
 * 
 * @param  parameterList List of DM_ENG_SystemParameterValueStruct structs which provide the couples <name, value> to set
 *
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_setValues(DM_ENG_SystemParameterValueStruct* parameterList[]);

/**
 * Gets all the names and values of parameters of a sub-tree defined by the given object.
 * 
 * @param objName Name of an object
 * @param data Data string possibly used to perform the getObject
 * @param pParamVal Pointer to a null terminated array of DM_ENG_ParameterValueStruct* which returns the couples <name, value> 
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_getObject(const char* objName, const char* data, OUT DM_ENG_ParameterValueStruct** pParamVal[]);

/**
 * Gets all the names of parameters of a sub-tree defined by the given object.
 * 
 * @param objName Name of an object
 * @param data Data string possibly used to perform the getNames
 * @param pParamVal Pointer to a null terminated array of char* which gives the names
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_getNames(const char* objName, const char* data, OUT char** pNames[]);

/**
 * Creates a new instance for the object and returns the instance number.
 * 
 * @param objName Name of an object prototype
 * @param pInstanceNumber Pointer to an interger which provides the new instance number
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_addObject(const char* objName, OUT unsigned int* pInstanceNumber);

/**
 * Deletes an instance of an object.
 * 
 * @param objName Name of an object prototype
 * @param instanceNumber Instance number of the object to delete
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_deleteObject(const char* objName, unsigned int instanceNumber);

/**
 * Commands the device to reboot.
 * 
 * @param factoryReset Indicates if or not the device must perform à factory reset.
 */
void DM_ENG_Device_reboot(bool factoryReset);

/**
 * Commands the device to perform diagnostics (ping test, traceroute, ...).
 * 
 * @param objName Name of the TR-069 object refering to the diagnostics ("Xxx...ZzzDiagnostics.")
 * @param paramList Parameter list of the arguments
 * @param pResult Pointer to the result if it is provided synchronously
 * 
 * @return 0 if the diagnostics is carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_performDiagnostics(const char* objName, DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[]);

/**
 * Commands the device to download a file.
 * 
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the source file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param fileSize Size of the file to be downloaded in bytes
 * @param targetFileName Name of the file to be used on the target file system
 * @param successURL URL the CPE may redirect the user's browser to if the download completes successfully
 * @param failureURL URL the CPE may redirect the user's browser to if the download does not complete successfully
 * @param transferId An integer that uniquely identifies the transfer request in the queue of transfers
 * 
 * @return 0 if the download is carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_download(char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName,
             char* successURL, char* failureURL, unsigned int transferId);

/**
 * Commands the device to upload a file.
 * 
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the destination file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param transferId An integer that uniquely identifies the transfer request in the queue of transfers
 * 
 * @return 0 if the upload id carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_upload(char* fileType, char* url, char* username, char* password, unsigned int transferId);

/**
 * Prepares the collection of statistics identified by the objName. As appropriate, the data will be collected in polling mode (periodic calls
 * to the DM_ENG_Device_getSampleData() function by the DM Engine) or or by events notified to the DM Engine (callback DM_ENG_ParameterManager_sampleData()). 
 * 
 * @param objName Name of the data model object dedicated to a stats group
 * @param timeRef Time reference used the stats sampling
 * @param sampleInterval Time interval between consecutive sampling
 * @param args Args string that can be used for the specific stats group
 * 
 * @return Error code : 0 if OK, 1 if the component is not ready to start collection, 2 or others for various types of errors
 * 
 * N.B. This function may be called with objName == NULL. In this case, the return value indicates if the device is ready to start statistics.  
 */
int DM_ENG_Device_startSampling(const char* objName, time_t timeRef, unsigned int sampleInterval, const char* args);

/**
 * Stop the collection of statistics. 
 * Polling mode : polling function should no longer called.
 * Event mode : no more calls to the callback function.
 */
int DM_ENG_Device_stopSampling(const char* objName);

/**
 * 
 * @param timeStampBefore Data for this time stamp are already passed. They should not be provided again.
 * If no new data are available, no new DM_ENG_SampleDataStruct is created. A non zero code (1) is returned.
 */
int DM_ENG_Device_getSampleData(const char* objName, time_t timeStampBefore, const char* data, OUT DM_ENG_SampleDataStruct** pSampleData);

#endif
