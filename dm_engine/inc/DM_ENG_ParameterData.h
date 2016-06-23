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
 * File        : DM_ENG_ParameterData.h
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
 * @file DM_ENG_ParameterData.h
 *
 * @brief This header file defined the persistent interface.
 *
 **/


#ifndef _DM_ENG_PARAMETER_DATA_H_
#define _DM_ENG_PARAMETER_DATA_H_

#include "DM_ENG_Parameter.h"
#include "DM_ENG_EventStruct.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_DownloadRequest.h"

/**
 * Initializes the data access.
 * 
 * @param dataPath Path to locate the data in the file system
 * @param factoryReset Indicates if the data must be resetted
 * 
 * @return Returns 0 if OK or a fault code, 1 : missing data or file, 2 : I/O error, ...
 */
int DM_ENG_ParameterData_init(const char* dataPath, bool factoryReset);

/**
 * Releases the resources used by the DM Agent
 */ 
void DM_ENG_ParameterData_release();

/**
 * Returns the number of parameter items i.e. parameters, nodes, prototypes or instances (currently known).
 * 
 * @return The number of parameter items
 */
int DM_ENG_ParameterData_getNumberOfParameters();

/**
 * Initializes an iteration over the parameters and returns the first parameter.
 * 
 * @return A pointer on the current parameter found
 */ 
DM_ENG_Parameter* DM_ENG_ParameterData_getFirst();

/**
 * Returns the next parameter in the iteration, NULL if there is no more parameter
 * 
 * @return A pointer on the current parameter found, NULL if there is no parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getNext();

/**
 * Initializes an iteration over the parameter names and returns the first name.
 * 
 * @return A pointer on the current parameter name
 */ 
char* DM_ENG_ParameterData_getFirstName();

/**
 * Returns the next parameter name in the iteration, NULL if there is no more parameter
 * 
 * @return A pointer on the current name, NULL if there is no more parameter
 */
char* DM_ENG_ParameterData_getNextName();

/**
 * Resumes the loop over the parameters to the given name. The subsequent DM_ENG_ParameterData_getNextName() calls
 * will provide the parameters from this position. 
 * 
 * @param resumeName Name of current parameter
 */
void DM_ENG_ParameterData_resumeParameterLoopFrom(char* resumeName);

/**
 * Returns the current accessed parameter
 * 
 * @return A pointer on the current parameter, NULL if there is no parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getCurrent();

/**
 * Gets a parameter by its name
 * 
 * @return A pointer on the parameter found, NULL if there is no parameter with the specified name
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getParameter(const char* name);

/**
 * Adds a new parameter to the data
 * 
 * @param Parameter just created
 */
void DM_ENG_ParameterData_addParameter(DM_ENG_Parameter* param);

/**
 * Creates an new instance for the specified object. Are created so much parameters that the object contains.
 * 
 * @param param Object for which the intance must be created
 * @param instanceNumber Intance number
 */
void DM_ENG_ParameterData_createInstance(DM_ENG_Parameter* param, unsigned int instanceNumber);

/**
 * Deletes one or more parameters with the given path and the configKey.
 * N.B. Only parameters resulting of a previous extension (which have a configKey value) are authorized to be suppressed.
 * 
 * @param name Name of the parameter to delete or partial path above a set of parameters to delete.
 * @param configKey ConfigKey value of the parameter(s) to delete. If NULL the parameters are deleted whatever the configKey value.
 *
 * @return true if at least one parameter has been deleted
 */  
bool DM_ENG_ParameterData_deleteParameters(const char* name, char* configKey);

/**
 * Deletes the object instance with the specified name. All the nodes and parameters of this instance are deleted.
 * 
 * @param objectName Root name of the instance to delete.
 */  
void DM_ENG_ParameterData_deleteObject(char* objectName);


/**
 * Returns the time of the next periodic inform message to send. 
 * 
 * @return The next periodic inform time.
 */ 
time_t DM_ENG_ParameterData_getPeriodicInformTime();

/**
 * Sets the next periodic inform time.
 * 
 * @param t the value to store
 */
void DM_ENG_ParameterData_setPeriodicInformTime(time_t t);

/**
 * Returns the time of next scheduled inform message.
 * 
 * @return the next scheduled inform time
 */
time_t DM_ENG_ParameterData_getScheduledInformTime();

/**
 * Returns the command key of next scheduled inform message.
 * 
 * @return the next scheduled inform command key
 */
char* DM_ENG_ParameterData_getScheduledInformCommandKey();

/**
 * Suppress the first scheduled inform in the stack and goes to the following
 */
void DM_ENG_ParameterData_nextScheduledInformCommand();

/**
 * Adds a scheduled inform command.
 * 
 * @param time Time of the scheduled inform command
 * @param commandKey Command key of the scheduled inform command
 */
void DM_ENG_ParameterData_addScheduledInformCommand(time_t time, char* commandKey);

/**
 * Tests if the event is allready in the list of the events to send
 * 
 * @param evCode The event code
 * 
 * @return true if the event is in the list, else otherwise
 */
bool DM_ENG_ParameterData_isInEvents(const char* evCode);

/**
 * Adds an event to send.
 * 
 * @param ec The event code
 * @param ck The command key associated to the event (possibly NULL)
 */
void DM_ENG_ParameterData_addEvent(const char* ec, char* ck);

/**
 * Deletes all the event with a given code
 * 
 * @param evCode An event code
 */
void DM_ENG_ParameterData_deleteEvent(const char* evCode);

/**
 * Deletes all the stored events.
 */
void DM_ENG_ParameterData_deleteAllEvents();

/**
 * Returns the events list
 * 
 * @return The first element in the chained list, NULL if empty. 
 */
DM_ENG_EventStruct* DM_ENG_ParameterData_getEvents();

/**
 * Returns the first element of the transfer requests list.
 * 
 * @return Pointer to the first transfer request
 */
DM_ENG_TransferRequest* DM_ENG_ParameterData_getTransferRequests();

/**
 * Inserts a transfer request in the list
 * 
 * @param tr A transfer request
 */
void DM_ENG_ParameterData_insertTransferRequest(DM_ENG_TransferRequest* tr);

/**
 * Removes a transfer request in the list
 * 
 * @param transferId The transfer id of the transfer request to remove
 */
void DM_ENG_ParameterData_removeTransferRequest(unsigned int transferId);

/**
 * Udates the transfer request which the given transferId
 * 
 * @param transferId The transfer id of the transfer request to update
 * @param state The new state
 * @param code The fault code
 * @param start The time when the transfer started
 * @param comp The time when the transfer is complete
 * 
 * @return Pointer to the updated transfer request
 */
DM_ENG_TransferRequest* DM_ENG_ParameterData_updateTransferRequest(unsigned int transferId, int state, unsigned int code, time_t start, time_t comp);

/**
 * Returns the first element of the download request list.
 * 
 * @return Pointer to the first download request
 */
DM_ENG_DownloadRequest* DM_ENG_ParameterData_getDownloadRequests();

/**
 * Adds a download request in the list
 * 
 * @param dr A download request from device
 */
void DM_ENG_ParameterData_addDownloadRequest(DM_ENG_DownloadRequest* dr);

/**
 * Removes the firt download request in the list
 */
void DM_ENG_ParameterData_removeFirstDownloadRequest();

/**
 * Sets the reboot command key value (possibly, a list of comma separeted command keys)
 * 
 * @param commandKey Command key string
 */
void DM_ENG_ParameterData_setRebootCommandKey(char* commandKey);

/**
 * Adds a reboot command key
 * 
 * @param commandKey Command key string
 */
void DM_ENG_ParameterData_addRebootCommandKey(char* commandKey);

/**
 * Gets the reboot command key value (possibly, a list of comma separeted command keys)
 * 
 * @return The reboot command key value
 */
char* DM_ENG_ParameterData_getRebootCommandKey();

/**
 * Sets the retry count to zero
 */
void DM_ENG_ParameterData_resetRetryCount();

/**
 * Gets the retry count integer
 * 
 * @return Retry count
 */
int DM_ENG_ParameterData_getRetryCount();

/**
 * Increments the retry count
 * 
 * @return the new value of the retry count
 */
int DM_ENG_ParameterData_incRetryCount();

/**
 * The DM Engine invokes the function to ensure that the data is saved at this point 
 */
void DM_ENG_ParameterData_sync();

/**
 * Restore the data as they were previously saved by a sync call (and discard the changes since this point) 
 */
void DM_ENG_ParameterData_restore();

/**
 * Gets the lock on the parameter resource if available.
 *
 * @return True if lock is taken, false otherwise
 */
bool DM_ENG_ParameterData_tryLock();

/**
 * Releases the lock.
 */
void DM_ENG_ParameterData_unLock();

#endif /*_DM_ENG_PARAMETER_DATA_H_*/
