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
 * File        : dm_com_rpc_acs.h
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
 * @file dm_RPC_ACS.h
 *
 * @brief Definition of the Remote Procedure Call of the ACS server
 *
 */

#ifndef _DM_RPC_ACS_H_
#define _DM_RPC_ACS_H_

#include "CMN_Type_Def.h"

// DM_COM's header
#include "dm_com.h"

// DM_ENGINE's header
#include "DM_ENG_RPCInterface.h"

//#ifndef FaultStruct
//typedef struct _FaultStruct_
//{
//	unsignedInt  FaultCode;
//	char	  FaultString[SIZE_FAULTSTRUCT_STRING];
//} __attribute((packed)) FaultStruct;
//#endif

/* ------------------- PROTOTYPES DEFINITION (for the DM_ENGINE) ----------------------- */

/**
 * @brief Request for the Remote Procedure Call available on the ACS server
 *
 * @param pMethods		List of the method that can be called by a CPE.
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_GetRPCMethodsCallback(char  **pMethods[]);

/**
 * @brief Send an informa(tion) message to the ACS server
 *
 * @param DeviceId		A structure that uniquely identifies the CPE.
 *
 * @param Event			An array of structures. Its indicate the event that causes the transaction session to
 *                              be established.
 * 
 * @param CurrentTime		The current date and time known to the CPE. This MUST be represented in the local time
 *				zone of the CPE, and MUST include the local time offset from UTC (will appropriate
 *				adjustment for daylight saving time).
 *
 * @param RetryCounts		Number of prior times an attempt was made to retry this session. This MUST be zero if
 *				and only if the previous session if any, completed succeffully. Eg : it will be reset
 *				to zero only when a session completes sucefully.
 *
 * @param ParameterList		Array of name-values pairs.
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_InformCallback(
  DM_ENG_DeviceIdStruct		    * id,
	DM_ENG_EventStruct		      * events[],
  DM_ENG_ParameterValueStruct	* parameterList[],
	time_t			                  currentTime,
	unsigned int		              retryCount);

/**
 * @brief Beside to "DM_ACS_Inform_Callback", this function will add the MaxEnveloppes parameter 
 * @brief that must be only known by the DM_COM module.
 *
 * @remarks The MaxEnveloppes parameter is set to 1 in the current TR069 version.
 *
 */
int
DM_ACS_Inform(DM_ENG_DeviceIdStruct	      * DeviceId,
              DM_ENG_EventStruct		      * Event[],
              unsignedInt		                MaxEnveloppes,
              dateTime          	          CurrentTime,
              unsignedInt		                RetryCounts,
              DM_ENG_ParameterValueStruct	* ParameterList[]);

/**
 * @brief Inform the ACS server to the end of a download or upload
 *
 * @param CommandKey		Set to the value of the commandkey argument passed to the CPE in the download or
 *				upload method call that initiated the transfert.
 *
 * @param FaultStruct		If the transfert was successfully, the faultcode is set to zero. Otherwise, a non-zero
 *				faultcode is specified along with a faultstring indicating the faillure reason.
 * 
 * @param StartTime		The date and time transfert was started in UTC. The CPE SOULD record  this information
 *				and report it in this argument, but if this information is not available, the value 
 *				of this argument MUST be set to the unknown time value.
 * 
 * @param CompleteTime		The date and time transfert completed in UTC. The CPE SOULD record  this information
 *				and report it in this argument, but if this information is not available, the value 
 *				of this argument MUST be set to the unknown time value.
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_TransfertCompleteCallback(DM_ENG_TransferCompleteStruct* tcs);

/**
 * @brief Beside to "DM_ACS_TransfertComplete_Callback", this function will follow the TR-069 definition
 */
int
DM_ACS_TransfertComplete(char		            * CommandKey,
                         FaultStruct	        FaultStruct,
			                   dateTime	            StartTime,
			                   dateTime	            CompleteTime);

/**
 * @brief Ask to download a file to the ACS server
 *
 * @param FileType	This is the file type being requested:
 *
 *				1 : Firmware upgrade image
 *				2 : Web content
 *				3 : Vendor configuration file
 *
 * @param FileTypeArg	Array of zero or more additional arguments where each argument is a structure of
 *                      name-value pairs. The use of the additional arguments depend of the file specified:
 *
 *				FileType	FiletypeArg Name
 *				   1			(none)
 *				   2			"Version"
 *				   3			(none)
 *	  
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_RequestDownloadCallback(const char         * FileType,
                               DM_ENG_ArgStruct   * FileTypeArg[]);

/**
 * @brief Function which make and send a soap message to the acs when need to be kicked
 *
 * @param Command	Generic argument that MAY be used by the ACS for identification or other purpose
 *
 * @param Referer	The content of the 'Referer' HTTP header sent to the CPE when it was kicked
 *
 * @param Arg		Generic argument that MAY be used by the ACS for identification or other purpose
 *
 * @param Next  	The URL that the ACS SHOULD return in the method response under normal conditions 
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_KickedCallback(char	* Command,
                      char	* Referer,
                      char	* Arg,
                      char  * Next);
		      
  /**
 * @brief Notify the DM_COM module to configure the HTTP Client Connection Data
 *        This callback function is used when the application is started and when 
 *        the ACS URL, Username or password is changed.
 *
 * @param url - The URL of the ACS
 *
 * @param username - The username to used to perform authentication (could be NULL if no authentication is required)
 * 
 * @param password - The password to used to perform authentication (could be NULL if no authentication is required)
 *
 * @return void
 *
 */ 
void
DM_ACS_updateAscParameters(char * url,
                           char * username,
				                   char * password);

/**
 * @brief function which remake the xml buffer from a XML data structure
 *
 * @param pElement  Current element of the XML tree (
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * @Remarks for a complete tree, pElement = pRoot
 */
char *
DM_RemakeXMLBufferACS(GenericXmlNodePtr pElement);


#endif /* _DM_RPC_ACS_H_ */
