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
 * File        : dm_com_rpc_acs.c
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
 * @file dm_com_rpc_acs.c
 *
 * @brief Implementation of the Remote Procedure Call of the ACS server
 *
 */

#ifndef __DM_COM_RPC_ACS_H_
#define _DM_COM_RPC_ACS_H_

// System headers
#include <stdlib.h>              /* Standard system library          */
#include <stdio.h>               /* Standard I/O functions           */
#include <stdarg.h>              /* */
#include <string.h>              /* Usefull string functions            */
#include <ctype.h>               /* */
#include <unistd.h>              /* Standard unix functions, like getpid()    */
#include <signal.h>              /* Signal name macros, and the signal() prototype  */
#include <time.h>               /* Time managing               */
#include <sys/types.h>          /* Various type definitions, like pid_t       */

#ifndef WIN32
#include <getopt.h>              /* Managing parameters              */
#include <netinet/in.h>         /*                    */
#include <arpa/inet.h>          /*                    */
#else
#include <winsock.h>
#endif

#include <errno.h>               /* Usefull for system errors           */


// Enable the tracing support
#ifdef DEBUG
#define SHOW_TRACES 1
#define TRACE_PREFIX "[DM-COM] %s:%s(),%d: ", __FILE__, __FUNCTION__, __LINE__
#endif

#include "CMN_Type_Def.h"  
#include "DM_ENG_Global.h"
#include "CMN_Trace.h"

// DM_COM's header
#include "dm_com.h"              /* DM_COM module definition            */
#include "dm_com_rpc_acs.h"      /* Definition of the ACS RPC methods         */

// DM_ENGINE's header
#include "DM_ENG_RPCInterface.h"    /* DM_Engine module definition            */

extern dm_com_struct  g_DmComData;

extern DM_CMN_Mutex_t mutexAcsSession;

/**
 * @brief Request for the Remote Procedure Call available on the ACS server
 *
 * @param pMethods List of RPC method that can be called by a CPE provide by the DM_Engine
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_GetRPCMethodsCallback(char **pMethods[])
{
  // Method not supported
   int nRet = DM_ENG_CANCELLED;
   if(NULL == pMethods) {
     EXEC_ERROR( "Invalid Pointer" );
   }
      
   return( nRet );
} /* DM_ACS_GetRPCMethodsCallback */

/**
 * @brief Send an informa(tion) message to the ACS server
 *
 * @param DeviceId      A structure that uniquely identifies the CPE.
 *
 * @param Event         An array of structures. Its indicate the event that causes the transaction session to
 *                              be established.
 * 
 * @param CurrentTime      The current date and time known to the CPE. This MUST be represented in the local time
 *          zone of the CPE, and MUST include the local time offset from UTC (will appropriate
 *          adjustment for daylight saving time).
 *
 * @param RetryCounts      Number of prior times an attempt was made to retry this session. This MUST be zero if
 *          and only if the previous session if any, completed succeffully. Eg : it will be reset
 *          to zero only when a session completes successfully.
 *
 * @param ParameterList    Array of name-values pairs.
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_InformCallback(
  DM_ENG_DeviceIdStruct        * id,
   DM_ENG_EventStruct            * events[],
  DM_ENG_ParameterValueStruct * parameterList[],
   time_t                           currentTime,
   unsigned int                    retryCount)
{
   int nRet = DM_ENG_CANCELLED;
   
   // Check parameters
   if ( (id != NULL) && (events != NULL) && (parameterList != NULL) )
   {
      // Optimisation pour le cas déconnecté : si pas d'adresse IP locale, pas la peine de tenter la connection à l'ACS
      bool ready = true;
      int i;
      for (i = 0; parameterList[i] != NULL; i++)
      {
         if (strcmp(parameterList[i]->parameterName, DM_ENG_LAN_IP_ADDRESS_NAME) == 0)
         {
            ready = (parameterList[i]->value != NULL);
            break;
         }
      }
      if (ready)
      {
         DBG( "Send an 'Inform()' message to the ACS (from the callback) " );

         // Translation with the internal DM_ACS_Inform function
         nRet = DM_ACS_Inform( id,
                             events,
                             MAX_ENVELOPPE_TR069,  // Set to 1 (cf TR069 protocol)
                             currentTime,
                             retryCount,
                             parameterList );
      }
      if ( nRet == DM_ENG_SESSION_OPENING )
      {
         DBG( "Inform Message : OK" );

         // ---------------------------------------------------------------------------
         // Enable the session
         // ---------------------------------------------------------------------------
         DBG( "Return code to give to the DM_ENGINE = %d", nRet );

         // Take the mutex to use bSession
         DM_CMN_Thread_lockMutex(mutexAcsSession);

         DBG("Create the ACS Session");
         g_DmComData.bSession = true;

         // Set the Time of the ACS Session creation.
         time(&g_DmComData.bSessionOpeningTime);

         // Free the mutex
         DM_CMN_Thread_unlockMutex(mutexAcsSession);

      } else {
         EXEC_ERROR( "Inform Message : NOK (%d) ", nRet );
      }
   } else {
      EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }

   return( nRet );
} /* DM_ACS_InformCallback */

/**
 * @brief Beside to "DM_ACS_Inform_Callback", this function will add the MaxEnveloppes parameter 
 * @brief that must be only known by the DM_COM module.
 *
 * @remarks The MaxEnveloppes parameter is set to 1 in the current TR069 version
 *
 * Example of an SOAP Inform message :
 *
 * <soap-env:Envelope
 * soap-env:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
 * xmlns:soap-env="http://schemas.xmlsoap.org/soap/envelope/"
 * xmlns:soap-enc="http://schemas.xmlsoap.org/soap/encoding/"
 * xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 * xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *      xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soap-env:Header>
 * <cwmp:ID 
 * soap-env:mustUnderstand="1" 
 * xsi:type="xsd:string"
 * xmlns:cwmp="urn:dslforum-org:cwmp-1-0">8402</cwmp:ID>
 * </soap-env:Header>
 * <soap-env:Body>
 * <cwmp:Inform>
 *   <DeviceId>
 *     <Manufacturer>FT-RD</Manufacturer>
 *     <OUI>00147D</OUI>
 *     <ProductClass>OpenSTB v0.1</ProductClass>
 *     <SerialNumber>STB0528JTG8</SerialNumber>
 *   </DeviceId>  // ---------------------------------------------------------------------------
   // Lock the mutex in order to synchronise the SOAP request sent to the ACS
   // with the SOAP response received by the CPE 
   // ---------------------------------------------------------------------------
   WARN( "Lock file <SOAP Inform Request>/<SOAP Inform Response>" );
   DM_LockDuringWork( &g_DmComData.mutexCallBackSynchro );
 *  <Event
 * soap-enc:arrayType="cwmp:EventStruct[02]">
 *    <EventStruct>
 *      <EventCode>1 BOOT</EventCode>
 *      <CommandKey></CommandKey>
 *    </EventStruct>
 *    <EventStruct>
 *      <EventCode>2 PERIODIC</EventCode>
 *      <CommandKey></CommandKey>
 *    </EventStruct>
 *  </Event>
 *  <MaxEnveloppes>1</MaxEnveloppes>
 *  <CurrentTime>2007-07-16T14:24:42</CurrentTime>
 *  <RetryCount>2</RetryCount>
 *  <ParameterList soap-enc:arrayType="cwmp:ParameterValueStruct[06]">
 *    <ParameterValueStruct>
 *      <Name>Device.DeviceSummary</Name>
 *      <Value xsi:type="xsd:string">STB:1.0[](Baseline:1),ServiceA:1.0[1](Baseline:1)</Value>
 *    </ParameterValueStruct>
 *    <ParameterValueStruct>
 *      <Name>Device.DeviceInfo.HardwareVersion</Name>
 *      <Value xsi:type="xsd:string">OpenSTB-Hardware-0.1</Value>
 *    </ParameterValueStruct>
 *    <ParameterValueStruct>
 *      <Name>Device.DeviceInfo.SoftwareVersion</Name>
 *      <Value xsi:type="xsd:string">OpenSTB-Software-0.1</Value>
 *    </ParameterValueStruct>
 *    <ParameterValueStruct>
 *      <Name>Device.ManagementServer.ParameterKey</Name>
 *      <Value xsi:type="xsd:string"></Value>
 *    </ParameterValueStruct>
 *    <ParameterValueStruct>
 *      <Name>Device.ManagementServer.ConnectionRequestURL</Name>
 *      <Value xsi:type="xsd:string"></Value>
 *    </ParameterValueStruct>
 *    <ParameterValueStruct>
 *      <Name>Device.LAN.IPAddress</Name>
 *      <Value xsi:type="xsd:string">10.194.60.186</Value>
 *    </ParameterValueStruct>
 *  </ParameterList>
 * </cwmp:Inform>
 * </soap-env:Body>
 * </soap-env:Envelope>
 *
 */
int
DM_ACS_Inform(DM_ENG_DeviceIdStruct       * DeviceId,
              DM_ENG_EventStruct          * Event[],
              unsignedInt                     MaxEnveloppes,
              dateTime                     CurrentTime,
              unsignedInt                     RetryCounts,
              DM_ENG_ParameterValueStruct * ParameterList[])
{
   int nRet = DM_ENG_CANCELLED;
   DM_SoapXml                SoapMsg;
   
   memset((void *)&SoapMsg, 0x00, sizeof(SoapMsg));
   
   // Check parameters
   if ( (Event != NULL) && (DeviceId != NULL) && (ParameterList != NULL) ) {
      int               nI = 0;
      char            pUniqueHeaderID[HEADER_ID_SIZE];
      GenericXmlNodePtr pRefInformTag          = NULL;
      char            pTmpBuffer[TMPBUFFER_SIZE];
      char           pTmpValueBuffer[TMPBUFFER_SIZE];
      int               nNbEventStruct            = 0;
      int               nNbParameterValueStruct   = 0;
      char         * commandKeyStr             = NULL;
      char         * httpMsgString             = NULL;
      char* sTime = NULL;

      INFO( "Send an 'Inform()' message to the ACS" );

      
      // ---------------------------------------------------------------------------
      // Initialize the SOAP data structure then create the XML/SOAP response
      // ---------------------------------------------------------------------------
      DM_InitSoapMsgReceived( &SoapMsg );
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, true);

      // ---------------------------------------------------------------------------
      // Update the SOAP HEADER & BODY
      // ---------------------------------------------------------------------------
      // --> Add an INFORM tag inside the body XML node
      // ---------------------------------------------------------------------------
      if ( DM_GenerateUniqueHeaderID( pUniqueHeaderID ) == DM_OK ) {
         //DBG( "-> HID = '%s' ", pUniqueHeaderID );
         DM_AddSoapHeaderID( pUniqueHeaderID, &SoapMsg );
      }
      
      if ( SoapMsg.pBody != NULL ){
         pRefInformTag = xmlAddNode(SoapMsg.pParser,  // XML Doc
                                 SoapMsg.pBody,    // XML Parent Node
                                 INFORM,           // Node Name
                                 NULL,             // Attribute Name
                                     NULL,             // Attribute Value
                                        NULL);            // Node Value
      
         
         if ( pRefInformTag == NULL ) {
           EXEC_ERROR( "Problem to add the <INFORM> XML tag!!" );
         }
      }
      
      
      // ---------------------------------------------------------------------
      // Add a DeviceId tag and some sub-tags
      // ---------------------------------------------------------------------
      GenericXmlNodePtr pRefDeviceIdTag = xmlAddNode(SoapMsg.pParser,  // XML Doc
                                                   pRefInformTag,    // XML Parent Node
                                                   INFORM_DEVICEID,  // Node Name
                                                   NULL,             // Attribute Name
                                                       NULL,             // Attribute Value
                                                          NULL);            // Node Value
            
      // - Manufacturer :
      char * manufacturerStr = NO_CONTENT;
      if ( DeviceId->manufacturer != NULL ){
         // DM_AddElementWithValue( pRefDeviceIdTag, INFORM_MANUFACTURER, (char*)DeviceId->manufacturer );
         manufacturerStr = DeviceId->manufacturer;       
      } else {
         // DM_AddElementWithValue( pRefDeviceIdTag, INFORM_MANUFACTURER, NO_CONTENT );
        EXEC_ERROR( "Missing manufacturer name!!" );
      }

    xmlAddNode(SoapMsg.pParser,      // doc
               pRefDeviceIdTag,      // parent node
               INFORM_MANUFACTURER,  // Node Name 
                NULL,                 // Attribut Name
                 NULL,                 // Attribut Value
                  manufacturerStr);     // Node Value
      
      // - Organisational Unique Identifier :
      char * ouiStr = NO_CONTENT;
      if ( DeviceId->OUI != NULL ){

         ouiStr = DeviceId->OUI;
      } else {

      EXEC_ERROR( "Missing OUI (Organizational Unique Identifier)!!" );
      }

    xmlAddNode(SoapMsg.pParser,  // doc
               pRefDeviceIdTag,  // parent node
               INFORM_OUI,       // Node Name 
                NULL,             // Attribut Name
                 NULL,             // Attribut Value
                  ouiStr);          // Node Value
              
      
      // - Product Class :
      char * productClassStr = NO_CONTENT;
      if ( DeviceId->productClass  != NULL ){
      productClassStr = DeviceId->productClass;
      } else {
        EXEC_ERROR( "Missing product class!!" );
      }

    xmlAddNode(SoapMsg.pParser,     // doc
               pRefDeviceIdTag,     // parent node
               INFORM_PRODUCTCLASS, // Node Name 
                NULL,                // Attribut Name
                 NULL,                // Attribut Value
                  productClassStr);    // Node Value
      
      // - Serial Number :
      char * serialNumberStr = NO_CONTENT;
      if ( DeviceId->serialNumber != NULL ){
      serialNumberStr = DeviceId->serialNumber;
      } else {
        EXEC_ERROR( "Missing serial number!!" );
      }

    xmlAddNode(SoapMsg.pParser,     // doc
               pRefDeviceIdTag,     // parent node
               INFORM_SERIALNUMBER, // Node Name 
                NULL,                // Attribut Name
                 NULL,                // Attribut Value
                  serialNumberStr);    // Node Value
             
      // ---------------------------------------------------------------------
      // Add an event tag and some sub-tags
      // ---------------------------------------------------------------------
   
      // ---------------------------------------------------------------------
      // Count how many Eventstruct there is in the list
      // ---------------------------------------------------------------------
      nNbEventStruct = DM_ENG_tablen( (void**)Event );
      if ( nNbEventStruct<10 ){
         snprintf( pTmpBuffer, TMPBUFFER_SIZE,  INFORM_EVENT_ATTR_VAL1, nNbEventStruct );
      } else {
         snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_EVENT_ATTR_VAL2, nNbEventStruct );
      }

      GenericXmlNodePtr pRefEventListTag = xmlAddNode(SoapMsg.pParser,    // XML Doc        
                                                      pRefInformTag,      // XML Parent Node  
                                                            INFORM_EVENT,       // Node Name
                                                                DM_COM_ArrayTypeAttrName,  // Attribute Name
                                                                   pTmpBuffer,         // Attribute Value
                                                                     NULL);              // Node Value
                          
                          
      
      // Node * pRefEventStruct = NULL;
      GenericXmlNodePtr pRefEventStruct = NULL;
      for ( nI=0 ; nI<nNbEventStruct ; nI++ )
      {

      pRefEventStruct = xmlAddNode(SoapMsg.pParser,         // XML Doc       
                                   pRefEventListTag,      // XML Parent Node   
                                   INFORM_EVENTSTRUCT,    // Node Name
                                   NULL,                  // Attribute Name
                                   NULL,                  // Attribute Value
                                   NULL);                 // Node Value

      xmlAddNode(SoapMsg.pParser,         // XML Doc       
                 pRefEventStruct,       // XML Parent Node   
                 INFORM_EVENT_CODE,     // Node Name
                 NULL,                  // Attribute Name
                 NULL,                  // Attribute Value
                 Event[nI]->eventCode); // Node Value
         
         // This data may be empty for some events
         commandKeyStr = NO_CONTENT;
         if ( Event[nI]->commandKey != NULL ){
            commandKeyStr = Event[nI]->commandKey;
         }

      xmlAddNode(SoapMsg.pParser,         // XML Doc       
                 pRefEventStruct,       // XML Parent Node   
                 INFORM_COMMANDKEY,     // Node Name
                 NULL,                  // Attribute Name
                 NULL,                  // Attribute Value
                 commandKeyStr); // Node Value  
      }
      
      // ---------------------------------------------------------------------
      // Add a MaxEnveloppes tag with a value set to 1 according to the TR-069 document (2007)
      // ---------------------------------------------------------------------
      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", MaxEnveloppes);
    xmlAddNode(SoapMsg.pParser,       // XML Doc        
               pRefInformTag,        // XML Parent Node   
               INFORM_MAXENVELOPES, // Node Name
               NULL,                 // Attribute Name
               NULL,                 // Attribute Value
               pTmpBuffer);          // Node Value 

      // ---------------------------------------------------------------------
      // Add a CurrentTime tag
      // ---------------------------------------------------------------------
      sTime = DM_ENG_dateTimeToString(CurrentTime);

      xmlAddNode(SoapMsg.pParser,       // XML Doc      
                  pRefInformTag,        // XML Parent Node   
                  INFORM_CURRENTTIME,   // Node Name
                  NULL,                 // Attribute Name
                  NULL,                 // Attribute Value
                  sTime);               // Node Value 
      free(sTime);

      // ---------------------------------------------------------------------
      // Add a RetryCount tag
      // ---------------------------------------------------------------------
      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", RetryCounts);

    xmlAddNode(SoapMsg.pParser,       // XML Doc        
               pRefInformTag,        // XML Parent Node   
               INFORM_RETRYCOUNT,   // Node Name
               NULL,                 // Attribute Name
               NULL,                 // Attribute Value
               pTmpBuffer);          // Node Value 
      // ---------------------------------------------------------------------
      // Add a parameterList tag and sub-tag ParameterValueStruct
      // ---------------------------------------------------------------------------
      // ---------------------------------------------------------------------------
      // Count how many parametervaluestruct there is in the list
      // ---------------------------------------------------------------------------
      nNbParameterValueStruct = DM_ENG_tablen( (void**)ParameterList );
      if ( nNbParameterValueStruct<10 ){
         snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTR_VAL1, nNbParameterValueStruct );
      } else {
         snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTR_VAL2, nNbParameterValueStruct );
      }

                          
      GenericXmlNodePtr pRefParamListTag = xmlAddNode(SoapMsg.pParser,    // XML Doc        
                                                      pRefInformTag,      // XML Parent Node  
                                                            INFORM_PARAMETERLIST,       // Node Name
                                                                DM_COM_ArrayTypeAttrName,  // Attribute Name
                                                                   pTmpBuffer,         // Attribute Value
                                                                     NULL);              // Node Value
                              

      GenericXmlNodePtr pRefParamStruct = NULL;
      GenericXmlNodePtr pRefParamValue  = NULL;
      // char      * lanIpAddressStr = NULL;
      for ( nI=0 ; nI<nNbParameterValueStruct ; nI++ ){

      pRefParamStruct = xmlAddNode(SoapMsg.pParser,               // doc
                                   pRefParamListTag,              // parent node
                                   INFORM_PARAMETERVALUESTRUCT,   // new node tag
                                   NULL,                          // Attribute Name
                                        NULL,                          // Attribute Value
                                            NULL);                         // Node value

      xmlAddNode(SoapMsg.pParser,                   // doc
                 pRefParamStruct,                   // parent node
                 INFORM_NAME,                       // new node tag
                 NULL,                              // Attribute Name
                 NULL,                              // Attribute Value
                 ParameterList[nI]->parameterName); // Node value

         memset((void*) pTmpValueBuffer, 0x00, TMPBUFFER_SIZE);
         if (ParameterList[nI]->value != NULL)  {
           strncpy(pTmpValueBuffer, ParameterList[nI]->value, TMPBUFFER_SIZE - 1);
           DBG("ParameterList[%d]->value: %s", nI, ParameterList[nI]->value);
         } else {
           DBG("ParameterList[%d]->value is NULL", nI);
         }

         // pRefParamValue = DM_AddElementWithValue( pRefParamStruct, INFORM_VALUE, lanIpAddressStr );
         
         switch ( ParameterList[nI]->type )
         {
            case DM_ENG_ParameterType_INT:     snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_INT );         break;
            case DM_ENG_ParameterType_UINT:    snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_UNSIGNEDINT ); break;
            case DM_ENG_ParameterType_LONG:    snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_LONG );        break;
            case DM_ENG_ParameterType_BOOLEAN: snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_BOOLEAN );     break;
            case DM_ENG_ParameterType_DATE:    snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_DATETIME );    break;
            case DM_ENG_ParameterType_STRING:  snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_STRING );      break;
            case DM_ENG_ParameterType_STATISTICS:
            case DM_ENG_ParameterType_ANY:     snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_ANY );         break;
            case DM_ENG_ParameterType_UNDEFINED: WARN( "Undefined type value!!" );           break;
            default:
               {
                 EXEC_ERROR( "Unknown type value for a parameter (%d) ",
                         ParameterList[nI]->type );
               }
               break;
         }

      pRefParamValue = xmlAddNode(SoapMsg.pParser,    // doc
                                  pRefParamStruct,    // parent node
                                  INFORM_VALUE,       // new node tag
                                  INFORM_VALUE_ATTR,  // Attribute Name
                                  pTmpBuffer,         // Attribute Value
                                  pTmpValueBuffer);   // Node value

      } // end for

      // ---------------------------------------------------------------------------
      // Remake the XML buffer then send it to ACS server
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pRoot != NULL )
      {
         httpMsgString = DM_RemakeXMLBufferACS( SoapMsg.pRoot );
         if ( httpMsgString != NULL ) {      
            // Send the HTTP message
            if ( DM_SendHttpMessage( httpMsgString ) == DM_OK ) {
               DBG( "Inform - Sending http message : OK" );
               nRet = DM_ENG_SESSION_OPENING;
            } else {
               DM_RemoveHeaderIDFromTab( pUniqueHeaderID );
              EXEC_ERROR( "Inform - Sending http message : NOK" );
            }
            
            // Free the HTTP Message
            free(httpMsgString);
         }
      }
   } else {
     EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }
   
  if(NULL != SoapMsg.pParser) {
     // Free xml document
     xmlDocumentFree(SoapMsg.pParser);
     SoapMsg.pParser = NULL;

   }
   
   return( nRet );
} /* DM_ACS_Inform */

/**
 * @brief Inform the ACS server to the end of a download or upload
 *
 * @param CommandKey    Set to the value of the commandkey argument passed to the CPE in the download or
 *          upload method call that initiated the transfert.
 *
 * @param FaultStruct      If the transfert was successfully, the faultcode is set to zero. Otherwise, a non-zero
 *          faultcode is specified along with a faultstring indicating the faillure reason.
 * 
 * @param StartTime     The date and time transfert was started in UTC. The CPE SOULD record  this information
 *          and report it in this argument, but if this information is not available, the value 
 *          of this argument MUST be set to the unknown time value.
 * 
 * @param CompleteTime     The date and time transfert completed in UTC. The CPE SOULD record  this information
 *          and report it in this argument, but if this information is not available, the value 
 *          of this argument MUST be set to the unknown time value.
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_TransfertCompleteCallback(DM_ENG_TransferCompleteStruct *tcs)
{
   int nRet         = DM_ENG_CANCELLED;
   FaultStruct        FaultStruct;
   
   // Check parameter
   if ( tcs != NULL ) {
      DBG( "Send an 'TransfertComplete()' message to the ACS (from the callback) " );
      DBG( "[%d] : '%s' ", tcs->faultCode, tcs->faultString );
      
      // ---------------------------------------------------------------------------
      // Set the FaultStruct data structure before
      // ---------------------------------------------------------------------------
      FaultStruct.FaultCode = tcs->faultCode;
      if ( tcs->faultString != NULL )
      {
         strcpy ( FaultStruct.FaultString, tcs->faultString );
      } else {
         strcpy ( FaultStruct.FaultString, NO_CONTENT );
      }
      
      // ---------------------------------------------------------------------------
      // Translation with the internal DM_ACS_Transfertcomplete function
      // ---------------------------------------------------------------------------
      nRet = DM_ACS_TransfertComplete(tcs->commandKey, FaultStruct, tcs->startTime, tcs->completeTime);
      if ( nRet == DM_ENG_SESSION_OPENING )
      {
         DBG( "Transfert Complete : OK" );

         
      } else {
         EXEC_ERROR( "Transfert Complete : NOK (%d) ", nRet );
      }
      
   } else {
      EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }
   
   
   return( nRet );
} /* DM_ACS_TransfertCompleteCallback */

/**
 * @brief Beside to "DM_ACS_TransfertComplete_Callback", this function will follow the TR-069 definition
 *
 * Example of an SOAP TransfertComplete message :
 *
 * <soap-env:Envelope 
 * soap-env:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
 * xmlns:soap-env="http://schemas.xmlsoap.org/soap/envelope/"
 * xmlns:soap-enc="http://schemas.xmlsoap.org/soap/encoding/"
 * xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 * xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 * xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soap-env:Header>
 * <cwmp:NoMoreRequests 
 *    xsi:type="xsd:string"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0">0</cwmp:NoMoreRequests>
 * <cwmp:ID soap-env:mustUnderstand="1"
 *    xsi:type="xsd:string"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0">OpenSTB-FTRD-9117</cwmp:ID>
 * </soap-env:Header>
 * <soap-env:Body>
 * <cwmp:TransfertComplete>
 *    <CommandKey>241</CommandKey>
 *    <FaultStruct>
 *          <FaultCode>0</FaultCode>
 *       <FaultString></FaultString>
 *    </FaultStruct>
 *    <StartTime>2001-08-08T05:35:14</StartTime>
 *    <CompleteTime>2001-08-08T05:42:59</CompleteTime>
 * </cwmp:TransfertComplete>
 * </soap-env:Body>
 * </soap-env:Envelope>
 */
int
DM_ACS_TransfertComplete(
  char          * CommandKey,
   FaultStruct   FaultStruct,
  dateTime      StartTime,
   dateTime     CompleteTime)
{
   int   nRet = DM_ENG_CANCELLED;
  DM_SoapXml                   SoapMsg;
  char                      * httpMsgString = NULL;
   
   memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));  
   
   // Check parameter
   if ( CommandKey != NULL )  {
      char         pUniqueHeaderID[HEADER_ID_SIZE];
      GenericXmlNodePtr pRefTranfertCompleteTag = NULL;
      char         pTmpBuffer[TMPBUFFER_SIZE];
    char* sTime = NULL;

      
      INFO( "Send a 'TranfertComplete()' message to the ACS (from the callback) " );
      
      // ---------------------------------------------------------------------------
      // Initialize the SOAP data structure then create the XML/SOAP response
      // ---------------------------------------------------------------------------
      DM_InitSoapMsgReceived( &SoapMsg );
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

      // ---------------------------------------------------------------------------      
      // Update the SOAP HEADER
      // ---------------------------------------------------------------------------
      if ( DM_GenerateUniqueHeaderID( pUniqueHeaderID ) == DM_OK ) {
         DM_AddSoapHeaderID( pUniqueHeaderID, &SoapMsg );
      }
      
      // ---------------------------------------------------------------------------
      // Save the current SOAPID
      // ---------------------------------------------------------------------------
      //strcpy( g_DmComData.DmSynchroCallback, pUniqueHeaderID );
      
      // ---------------------------------------------------------------------------
      // Update the SOAP BODY
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pBody != NULL ) {
         // ---------------------------------------------------------------------
         // Add a Transfertcomplete Tag
         // ---------------------------------------------------------------------

      pRefTranfertCompleteTag = xmlAddNode(SoapMsg.pParser,    // doc
                                           SoapMsg.pBody,      // parent node
                                           TRANSFERTCOMPLETE,  // new node tag
                                           NULL,               // Attribute Name
                                           NULL,               // Attribute Value
                                           NULL);              // Node value
         
         // ---------------------------------------------------------------------
         // Add a command key
         // ---------------------------------------------------------------------
         if ( strlen( (char*)CommandKey ) <= SIZE_COMMANDKEY ) {

        xmlAddNode(SoapMsg.pParser,             // doc
                   pRefTranfertCompleteTag,     // parent node
                   TRANSFERTCOMPLETECOMMANDKEY, // new node tag
                   NULL,                        // Attribute Name
                   NULL,                        // Attribute Value
                   (char*)CommandKey);          // Node value
                 
         } else {
           EXEC_ERROR( "Command key bigger than %d!!", SIZE_COMMANDKEY);
         }
         
         // ---------------------------------------------------------------------
         // Add a faultstruct and its two elements
         // ---------------------------------------------------------------------
      GenericXmlNodePtr pRefFaultStruct = xmlAddNode(SoapMsg.pParser,              // DOC
                                                     pRefTranfertCompleteTag,      // Parent Node
                                                     TRANSFERTCOMPLETEFAULTSTRUCT, // Node Name
                                                     NULL,                         // Attribute Name
                                                     NULL,                         // Attribute Value 
                                                     NULL);                        // Node Value
                                  
         
         snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d",  (int)FaultStruct.FaultCode );

      xmlAddNode(SoapMsg.pParser,             // DOC
                 pRefFaultStruct,             // Parent Node
                 TRANSFERTCOMPLETEFAULTCODE,  // Node Name
                 NULL,                        // Attribute Name
                 NULL,                        // Attribute Value                        
                 pTmpBuffer);                 // Node Value
               

      xmlAddNode(SoapMsg.pParser,              // DOC
                 pRefFaultStruct,              // Parent Node
                 TRANSFERTCOMPLETEFAULTSTRING, // Node Name
                 NULL,                         // Attribute Name
                 NULL,                         // Attribute Value                        
                 FaultStruct.FaultString);     // Node Value
         // ---------------------------------------------------------------------
         // Add a start time
         // ---------------------------------------------------------------------
         sTime = DM_ENG_dateTimeToString(StartTime);
         DBG( "Start time = %s ", sTime );

         xmlAddNode(SoapMsg.pParser,             // DOC
                    pRefTranfertCompleteTag,     // Parent Node
                    TRANSFERTCOMPLETESTARTTIME,  // Node Name
                    NULL,                        // Attribute Name
                    NULL,                        // Attribute Value                        
                    sTime);                      // Node Value
         free(sTime);

         // ---------------------------------------------------------------------
         // Add a complete time (end time)
         // ---------------------------------------------------------------------
         sTime = DM_ENG_dateTimeToString(CompleteTime);
         DBG( "Complete/End time = %s ", sTime );

         xmlAddNode(SoapMsg.pParser,               // DOC
                    pRefTranfertCompleteTag,       // Parent Node
                    TRANSFERTCOMPLETECOMPLETETIME, // Node Name
                    NULL,                          // Attribute Name
                    NULL,                          // Attribute Value                        
                    sTime);                        // Node Value
         free(sTime);

      }
      
      // ---------------------------------------------------------------------------
      // Remake the XML buffer then send it to DM_SendHttpMessage
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pRoot != NULL )
      {
         httpMsgString = DM_RemakeXMLBufferACS( SoapMsg.pRoot );
         if ( httpMsgString != NULL ) {      
            // Send the message
            if ( DM_SendHttpMessage( httpMsgString ) == DM_OK ) {
               DBG( "TransfertComplete - Sending http message : OK" );
            } else {
               DM_RemoveHeaderIDFromTab( pUniqueHeaderID );
              EXEC_ERROR( "TransfertComplete - Sending http message : NOK" );
            }
         }
         free(httpMsgString);
      }
      
      nRet = DM_ENG_SESSION_OPENING;
   } else {
     EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }
   
   if(NULL != SoapMsg.pParser) {
     // Free xml document
     xmlDocumentFree(SoapMsg.pParser);
     SoapMsg.pParser = NULL;

   }
   
   return( nRet );
} /* DM_ACS_TransfertComplete */

/**
 * @brief Ask to download a file to the ACS server
 *
 * @param FileType   This is the file type being requested:
 *
 *          1 : Firmware upgrade image
 *          2 : Web content
 *          3 : Vendor configuration file
 *
 * @param FileTypeArg   Array of zero or more additional arguments where each argument is a structure of
 *                      name-value pairs. The use of the additional arguments depend of the file specified:
 *
 *          FileType FiletypeArg Name
 *             1        (none)
 *             2        "Version"
 *             3        (none)
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_RequestDownloadCallback(const char         * FileType,
                               DM_ENG_ArgStruct   * FileTypeArg[])
{

  DM_SoapXml            SoapMsg;
  char               * httpMsgString          = NULL;
  GenericXmlNodePtr    pRefRequestDownloadTag   = NULL;
  GenericXmlNodePtr    pRefFileTypeArgTag     = NULL;
  GenericXmlNodePtr    pRefArgStructTag       = NULL;
  unsigned int         argStructArraySize     = 0;
  unsigned int         n;
  char                 tmpStr[SIZE_FAULTSTRUCT_STRING];
  char                 pUniqueHeaderID[HEADER_ID_SIZE];
   int   nRet = DM_ENG_CANCELLED;

   
   memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
   
   // Check parameter
   if ( FileType != NULL ) {
   
      INFO( "Send a 'RequestDownload()' message to the ACS (from the callback) " );
      // ---------------------------------------------------------------------------
      // Initialize the SOAP data structure then create the XML/SOAP response
      // ---------------------------------------------------------------------------
      DM_InitSoapMsgReceived( &SoapMsg );
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

      // ---------------------------------------------------------------------------      
      // Update the SOAP HEADER
      // ---------------------------------------------------------------------------
      if ( DM_GenerateUniqueHeaderID( pUniqueHeaderID ) == DM_OK ) {
         DM_AddSoapHeaderID( pUniqueHeaderID, &SoapMsg );
      }
      
      // ---------------------------------------------------------------------------
      // Update the SOAP BODY
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pBody != NULL ) {
         // ---------------------------------------------------------------------
         // Add a REQUESTDOWNLOAD Tag
         // ---------------------------------------------------------------------

        pRefRequestDownloadTag = xmlAddNode(SoapMsg.pParser,    // doc
                                            SoapMsg.pBody,      // parent node
                                            REQUESTDOWNLOAD,    // new node tag
                                            NULL,               // Attribute Name
                                            NULL,               // Attribute Value
                                            NULL);              // Node value
        }
        
        // Add the FileType Node
      xmlAddNode(SoapMsg.pParser,          // doc
                 pRefRequestDownloadTag,   // parent node
                 REQUESTDOWNLOAD_FILETYPE, // new node tag
                 NULL,                     // Attribute Name
                 NULL,                     // Attribute Value
                 FileType);                // Node value
       
        // Compute the number of element in the array
      argStructArraySize = DM_ENG_tablen( (void**)FileTypeArg );

        DBG("Number of element in the Array = %d", argStructArraySize);
        
        // Build the string
        sprintf(tmpStr, REQUESTDOWNLOAD_CWMP_ARG_STRUCT, argStructArraySize);      
                          
        // Add The Node 
        pRefFileTypeArgTag = xmlAddNode(SoapMsg.pParser,                 // doc
                                      pRefRequestDownloadTag,          // parent node
                                      REQUESTDOWNLOAD_FILETYPEARG,     // new node tag
                                      DM_COM_ArrayTypeAttrName, // Attribute Name
                                      tmpStr,                          // Attribute Value
                                      NULL);                           // Node value
                  
       // Add all the ArgStruct
       for(n = 0; n < argStructArraySize; n++) {
         // Add thhe 
         pRefArgStructTag = xmlAddNode(SoapMsg.pParser,           // doc
                                     pRefFileTypeArgTag,        // parent node
                                     REQUESTDOWNLOAD_ARGSTRUCT, // new node tag
                                     NULL,                      // Attribute Name
                                     NULL,                      // Attribute Value
                                     NULL);                     // Node value
          // Add the Name
          xmlAddNode(SoapMsg.pParser,       // doc
                  pRefArgStructTag,      // parent node
                  PARAM_NAME,            // new node tag
                  NULL,                  // Attribute Name
                  NULL,                  // Attribute Value
                  FileTypeArg[n]->name);  // Node value
                 
          // Add the Value
          xmlAddNode(SoapMsg.pParser,       // doc
                  pRefArgStructTag,      // parent node
                  PARAM_VALUE,           // new node tag
                  NULL,                  // Attribute Name
                  NULL,                  // Attribute Value
                  FileTypeArg[n]->value); // Node value
       }
       
       
      
      // ---------------------------------------------------------------------------
      // Remake the XML buffer then send it to DM_SendHttpMessage
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pRoot != NULL )
      {
         httpMsgString = DM_RemakeXMLBufferACS( SoapMsg.pRoot );
         if ( httpMsgString != NULL ) {      
            // Send the message
            if ( DM_SendHttpMessage( httpMsgString ) == DM_OK ) {
               DBG( "TransfertComplete - Sending http message : OK" );
            } else {
               DM_RemoveHeaderIDFromTab( pUniqueHeaderID );
               EXEC_ERROR( "TransfertComplete - Sending http message : NOK" );
            }
         }
         free(httpMsgString);
      }
      
      nRet = DM_ENG_SESSION_OPENING;
   } else {
      EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }
   
   if(NULL != SoapMsg.pParser) {
     // Free xml document
     xmlDocumentFree(SoapMsg.pParser);
     SoapMsg.pParser = NULL;

   }
   
   return( nRet );

} /* DM_ACS_RequestDownloadCallback */

/**
 * @brief Function which make and send a soap message to the acs when need to be kicked
 *
 * @param Command Generic argument that MAY be used by the ACS for identification or other purpose
 *
 * @param Referer The content of the 'Referer' HTTP header sent to the CPE when it was kicked
 *
 * @param Arg     Generic argument that MAY be used by the ACS for identification or other purpose
 *
 * @param Next    The URL that the ACS SHOULD return in the method response under normal conditions 
 *
 * @return see the TR069's RPC return code
 *
 */
int
DM_ACS_KickedCallback(
  char   * Command,
  char   * Referer,
  char   * Arg, 
  char  * Next)
{


   int nRet = DM_ENG_CANCELLED;
  if((NULL == Command) || (NULL == Referer) || (NULL == Arg) || (NULL == Next)) {
     EXEC_ERROR( "Invalid Pointer" );
   }     
   return( nRet );
} /* DM_ACS_KickedCallback */

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
void DM_ACS_updateAscParameters(char * url,
                                char * username,
                                    char * password)
{
  INFO("Configure the HTTP Client Connection Data");
  INFO("ACS url      = %s", ((NULL != url)      ? url      : "None"));
  INFO("ACS username = %s", ((NULL != username) ? username : "None"));
  INFO("ACS password = %s", ((NULL != password) ? password : "None"));

  DM_COM_ConfigureHttpClient(url, username, password);
    
}

/**
 * @brief function which remake the xml buffer
 *
 * @param pElement  Current element of the XML tree (
 *
 * @Return A pointer on the string (or NULL on ERROR)
 *
 * @Remarks for a complete tree, pElement = pRoot
 */
char *
DM_RemakeXMLBufferACS(GenericXmlNodePtr pElement)
{
  char * httpMessageStr = NULL;
  
  if(NULL == pElement) {
    return (httpMessageStr);
  }

   httpMessageStr = xmlDocumentToStringBuffer(xmlNodeToDocument(pElement));
      
   return httpMessageStr;
} /* DM_RemakeXMLBufferACS */


#endif /* _DM_COM_RPC_ACS_H_ */
