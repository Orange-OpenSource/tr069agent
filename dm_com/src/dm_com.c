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
 * File        : DM_com.c
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
 * @file DM_com.c
 *
 * @brief 
 *
 **/
 

// System headers
#include <stdlib.h>	  /* Standard system library	    	*/
#include <stdio.h>	  /* Standard I/O functions	    	*/
#include <stdarg.h>	  /* */
#include <string.h>	  /* Usefull string functions    */
#include <ctype.h>	  /* */
#include <unistd.h>	  /* Standard unix functions, like getpid()	  	*/
#include <signal.h>	  /* Signal name macros, and the signal() prototype 	*/
#include <time.h>	  /* Time managing      	*/
#include <sys/types.h>	  /* Various type definitions, like pid_t    	*/
#include <getopt.h>	  /* Managing parameters	    */

#ifndef WIN32
#include <netinet/in.h>	  /* */
#include <arpa/inet.h>	  /* */
#endif

#include <errno.h>	  /* Usefull for system errors    */
#include <sys/time.h> /* gettimeofday */
#include "DM_ENG_Global.h"
#include "CMN_Trace.h"
#include "DM_ENG_Parameter.h"
#include "DM_CMN_Thread.h"

// Enable the tracing support only if the debug mode is enabled
#ifdef DEBUG
#define SHOW_TRACES 1
#define TRACE_PREFIX	"[DM-COM] %s:%s(),%d: ", __FILE__, __FUNCTION__, __LINE__
#endif

// DM_COM's header
#include "dm_com.h"	  /* DM_COM module definition    */

#include "dm_com_rpc_acs.h"  /* Definition of the ACS RPC methods	  */

// DM_ENGINE's header
#include "DM_ENG_RPCInterface.h"  /* DM_Engine module definition    */

#define _EMPTY ""

#define _CONTENT_LENGTH_0 "Content-Length: 0"

static char* _DEFAULT_SOAPENV_NS = DM_COM_DEFAULT_SOAPENV_NS;

char* DM_COM_SoapEnv_NS = NULL;
static char* _SoapEnvAttrName = NULL;
static char* _SoapSkeleton = NULL;
static char* _EnvelopeTagName = NULL;
static char* _BeginEnvelopeTag = NULL;
static char* _EndEnvelopeTag = NULL;
static char* _HeaderTagName = NULL;
static char* _BeginHeaderTag = NULL;
static char* _BodyTagName = NULL;
static char* _BeginBodyTag = NULL;
static char* _FaultTagName = NULL; 
char* DM_COM_ArrayTypeAttrName = NULL; 
static char* _MustUnderstandAttrName = NULL; 

dm_com_struct	 g_DmComData;  /* Global data structure for the DM_COM module  */

// Information about the thread launched for the http's server
static DM_CMN_ThreadId_t _HttpSvrThreadID = 0;

// Information about the thread launched to supervise the ACS Session
static DM_CMN_ThreadId_t _AcsSupervisionThreadID;

static char  *g_pBufferCpe = NULL;	/* Global pointer used to remake XML buffer  */

static char  *g_retryBuffer = NULL;/* Global pointer used to retry CPE Request */

#define ACSSESSIONSUPERVISOR (10)
#define ACSSESSIONTIMEOUT    (30)

// Unauthorized Session
#define AUTHORIZATION_REQUIRED "401 Authorization Required"

// Mutex to protect ACS Session Data
DM_CMN_Mutex_t mutexAcsSession = NULL;

// This flag is used to ignore the HTTP NO CONTENT Message 
// when the Http session with the ACS is opened.
bool ignoreHttpNoContentMsgFlag = true;

// Indicate if the cwmp session must
// be closed when the cpe will receive the next 200 OK Http Message
// (This is done in order to be compliant with all the ACS type)
bool cwmpSessionClosingFlag = false;

// USed to perform ACS Capability - The CPE must try to use CWMP-1-1 flag.
// If the ACS does not support this release, the CPE uses CWMP-1-0 flag
bool atLeastOneInformResponseReceivedWithThisAcsTerminated = false;
bool acsSupportsCwmp_1_1                                   = true;

static bool _httpServerStatus = false; // Flag to indicate if the HTTP Server is started.

static void _forceACSSessionToClose(void);
static void _closeACSSession(bool success);
static char * _getCwmpVersionSupported();

static bool _AcsSessionSupervisorAlive = false;


/**
 * @brief Start the DM_COM module
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_COM_START()
{
	DMRET             nRet = DM_ERR;

   DBG("CWMP - DM_COM_START - Start DM_COM Module");
    nRet = DM_COM_INIT();
   if ( nRet == DM_OK )
   {
      // Wait until the HTTP Server is started
      DBG("Wait HTTP Server is started - begin");
	  while(false == _isHttpServerStarted())
	  {
	  	 DM_CMN_Thread_msleep(500);
	  }
      DBG("Wait HTTP Server is started - end");
   }

   return( nRet );
} /* DM_COM_START */

/**
 * @brief Stop the DM_COM module
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_COM_STOP()
{

	DMRET nRetHttpServer = DM_ERR;   

	INFO("DM_COM_STOP - begin");
	
	// Stop Http Client DataStructure (close session if required and free allocated memory)
	DM_StopHttpClient();

	// ---------------------------------------------------------------------------------
	// Close the HTTP server
	// ---------------------------------------------------------------------------------
	// Say to the http server to exit
	g_DmComData.ServerHttp.nExitHttpSvr = 1;
	if ( g_DmComData.ServerHttp.sockfd_serveur != -1 ){
#ifndef WIN32
    nRetHttpServer = shutdown( g_DmComData.ServerHttp.sockfd_serveur, SHUT_RDWR );
#else
    nRetHttpServer = shutdown( g_DmComData.ServerHttp.sockfd_serveur, 0 );
#endif  
	  nRetHttpServer = close( g_DmComData.ServerHttp.sockfd_serveur );
    if ( nRetHttpServer != 0 ) {
	    EXEC_ERROR( "[%d] Closing the http server : NOK ", nRetHttpServer );
    } else {
	    //DBG( "[%d] Closing the http server : OK ", nRetHttpServer );
	    nRetHttpServer = DM_OK;
    }
	} else {
    WARN( "[%d] Closing the http server : OK (http server already closed) ", nRetHttpServer );
	}
	
	// ---------------------------------------------------------------------------------
	// Close the DM_COM Thread
	// ---------------------------------------------------------------------------------
	  DM_CMN_Thread_cancel ( _HttpSvrThreadID );
	
	// ------------------------------------------------------
	// Delete the callbacks's links with the DM_ENGINE
	// ------------------------------------------------------
	DM_ENG_DeactivateNotification( DM_ENG_EntityType_ACS ); // Doesn't return any return code !!!
	
	// Close the ACS Supervision thread
	_AcsSessionSupervisorAlive = false;
	DM_CMN_Thread_cancel ( _AcsSupervisionThreadID );
  
  // Free g_retryBuffer
  DM_ENG_FREE(g_retryBuffer);
	
   // ---------------------------------------------------------------------------------
   // Destry the mutex used in the DM_COM module
   // ---------------------------------------------------------------------------------
   DM_CMN_Thread_destroyMutex( mutexAcsSession );

	INFO("DM_COM_STOP - end");

	return( DM_OK );
} /* DM_COM_STOP */

/**
 * @brief Initialize the DM_COM module
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET DM_COM_INIT()
{
	DMRET                    nRet       = DM_ERR;
	
	INFO( "Starting DM_COM module as a thread..." );
	
	// ---------------------------------------------------------------------------------
	// Initialize the Dm_Com data with the usefull information
	// ---------------------------------------------------------------------------------
	g_DmComData.ServerHttp.nPortNumber    = CPE_PORT;
	g_DmComData.ServerHttp.sockfd_serveur = -1;
	g_DmComData.Acs.nMaxEnvelopes	        = MAXENVELOPPE;
  
	// ---------------------------------------------------------------------------------
	// Initialize the SOAP structure
	// ---------------------------------------------------------------------------------
	DM_ENG_FREE(g_pBufferCpe);

	// ---------------------------------------------------------------------------------
	// Initialize the session indicator
	// ---------------------------------------------------------------------------------
	g_DmComData.bSession            = false;  // No need to get the mutex during init.
	g_DmComData.bSessionOpeningTime = 0;      // No need to get the mutex during init.
	g_DmComData.bIRTCInProgess      = false;

	// ---------------------------------------------------------------------------------
	// Initialize the several mutex used int the DM_COM module
	// ---------------------------------------------------------------------------------
	DM_CMN_Thread_initMutex( &mutexAcsSession );
	
	// ---------------------------------------------------------------------------------
	// Set the callback's notification with the DM_ENGINE
	// ---------------------------------------------------------------------------------

	DM_ENG_ActivateNotification( DM_ENG_EntityType_ACS, DM_ACS_InformCallback, DM_ACS_TransfertCompleteCallback, DM_ACS_RequestDownloadCallback, DM_ACS_updateAscParameters);
	
	// ---------------------------------------------------------------------------------
	// Create the ACS Session Supervisor Thread.
	// ---------------------------------------------------------------------------------
	_AcsSupervisionThreadID = 0;
   _HttpSvrThreadID = 0;

	if ( DM_CMN_Thread_create(DM_COM_ACS_SESSION_SUPERVISOR, NULL, false, &_AcsSupervisionThreadID) != 0 ) {
      EXEC_ERROR( "Starting the DM_ACS_Session_Supervision thread : NOK" );
	} else {

   	// ---------------------------------------------------------------------------------
   	// Launching the DM_COM_HttpServer thread (http server)
   	// ---------------------------------------------------------------------------------
   	if (DM_ENG_IS_CONNECTION_REQUEST_ALLOWED)
      {
   	  INFO("Start the HTTP Server Thread");
   	  if ( DM_CMN_Thread_create(DM_HttpServer_Start, NULL, false, &_HttpSvrThreadID) != 0 ){
         EXEC_ERROR( "Starting the DM_COM_HttpServer thread : NOK" );
   	  } else {
         nRet = DM_OK;
   	  }

   	} else {
   	  INFO("Do NOT Start the HTTP Server Thread");
   	  // Do no block the dm_com --> Notify DM_COM that the HTTP Server initialisation is done.
       DM_HttpServerStarted();
      }
   }
   return nRet;
}

/**
 * @brief Configure the HTTP CLient 
 * Configure the URL ACS and SSL Options
 *
 * @param none
 *
 * @return return void
 *
 */
void
DM_COM_ConfigureHttpClient(char * acsUrl,
                           char * acsUsername,
			                     char * acsPassword){
					     
  // Reset to default values
  atLeastOneInformResponseReceivedWithThisAcsTerminated = false;
  acsSupportsCwmp_1_1                                   = true;

  // Init Http Client Data structure and default value
  DM_ConfigureHttpClient(acsUrl,
                         acsUsername,
                         acsPassword,
                         g_DmComData.szCertificatStr,
                         g_DmComData.szRsaKeyStr,
                         g_DmComData.szCaAcsStr);  
	
}

static void _initPrefixedTag()
{
   if (DM_COM_SoapEnv_NS == NULL) { DM_COM_SoapEnv_NS = _DEFAULT_SOAPENV_NS; }
   size_t lns = strlen(DM_COM_SoapEnv_NS);

   // "xmlns:soapenv"
   if (_SoapEnvAttrName != NULL) { free(_SoapEnvAttrName); }
   _SoapEnvAttrName = malloc(lns+7);
   strcpy(_SoapEnvAttrName, "xmlns:");
   strcat(_SoapEnvAttrName, DM_COM_SoapEnv_NS);

   // "soapenv:Envelope"
   if (_EnvelopeTagName != NULL) { free(_EnvelopeTagName); }
   _EnvelopeTagName = malloc(lns+strlen(DM_COM_ENV_TAG)+2);
   strcpy(_EnvelopeTagName, DM_COM_SoapEnv_NS);
   strcat(_EnvelopeTagName, ":");
   strcat(_EnvelopeTagName, DM_COM_ENV_TAG);

   // "<soapenv:Envelope"
   if (_BeginEnvelopeTag != NULL) { free(_BeginEnvelopeTag); }
   _BeginEnvelopeTag = malloc(strlen(_EnvelopeTagName)+2);
   strcpy(_BeginEnvelopeTag, "<");
   strcat(_BeginEnvelopeTag, _EnvelopeTagName);

   // "</soapenv:Envelope>"
   if (_EndEnvelopeTag != NULL) { free(_EndEnvelopeTag); }
   _EndEnvelopeTag = malloc(strlen(_EnvelopeTagName)+4);
   strcpy(_EndEnvelopeTag, "</");
   strcat(_EndEnvelopeTag, _EnvelopeTagName);
   strcat(_EndEnvelopeTag, ">");

   // "<soapenv:Envelope></soapenv:Envelope>"
   if (_SoapSkeleton != NULL) { free(_SoapSkeleton); }
   _SoapSkeleton = malloc(strlen(_BeginEnvelopeTag)+strlen(_EndEnvelopeTag)+2);
   strcpy(_SoapSkeleton, _BeginEnvelopeTag);
   strcat(_SoapSkeleton, ">");
   strcat(_SoapSkeleton, _EndEnvelopeTag);

   // "soapenv:Header"
   if (_HeaderTagName != NULL) { free(_HeaderTagName); }
   _HeaderTagName = malloc(lns+strlen(DM_COM_HEADER_TAG)+2);
   strcpy(_HeaderTagName, DM_COM_SoapEnv_NS);
   strcat(_HeaderTagName, ":");
   strcat(_HeaderTagName, DM_COM_HEADER_TAG);

   // "<soapenv:Header>"
   if (_BeginHeaderTag != NULL) { free(_BeginHeaderTag); }
   _BeginHeaderTag = malloc(strlen(_HeaderTagName)+3);
   strcpy(_BeginHeaderTag, "<");
   strcat(_BeginHeaderTag, _HeaderTagName);
   strcat(_BeginHeaderTag, ">");

   // "soapenv:Body"
   if (_BodyTagName != NULL) { free(_BodyTagName); }
   _BodyTagName = malloc(lns+strlen(DM_COM_BODY_TAG)+2);
   strcpy(_BodyTagName, DM_COM_SoapEnv_NS);
   strcat(_BodyTagName, ":");
   strcat(_BodyTagName, DM_COM_BODY_TAG);

   // "<soapenv:Body>"
   if (_BeginBodyTag != NULL) { free(_BeginBodyTag); }
   _BeginBodyTag = malloc(strlen(_BodyTagName)+3);
   strcpy(_BeginBodyTag, "<");
   strcat(_BeginBodyTag, _BodyTagName);
   strcat(_BeginBodyTag, ">");

   // "soapenv:Fault"
   if (_FaultTagName != NULL) { free(_FaultTagName); }
   _FaultTagName = malloc(lns+strlen(DM_COM_FAULT_TAG)+2);
   strcpy(_FaultTagName, DM_COM_SoapEnv_NS);
   strcat(_FaultTagName, ":");
   strcat(_FaultTagName, DM_COM_FAULT_TAG);

   // "soapenc:arrayType"
   if (DM_COM_ArrayTypeAttrName != NULL) { free(DM_COM_ArrayTypeAttrName); }
   DM_COM_ArrayTypeAttrName = malloc(strlen(DM_COM_DEFAULT_SOAPENC_NS)+strlen(DM_COM_ARRAY_TYPE_ATTR)+2);
   strcpy(DM_COM_ArrayTypeAttrName, DM_COM_DEFAULT_SOAPENC_NS);
   strcat(DM_COM_ArrayTypeAttrName, ":");
   strcat(DM_COM_ArrayTypeAttrName, DM_COM_ARRAY_TYPE_ATTR);

   // "soapenv:mustUnderstand"
   if (_MustUnderstandAttrName != NULL) { free(_MustUnderstandAttrName); }
   _MustUnderstandAttrName = malloc(lns+strlen(DM_COM_MUST_UNDERSTAND_ATTR)+2);
   strcpy(_MustUnderstandAttrName, DM_COM_SoapEnv_NS);
   strcat(_MustUnderstandAttrName, ":");
   strcat(_MustUnderstandAttrName, DM_COM_MUST_UNDERSTAND_ATTR);
}

static void _freePrefixedTag()
{
   if (DM_COM_SoapEnv_NS != _DEFAULT_SOAPENV_NS) { DM_ENG_FREE(DM_COM_SoapEnv_NS); } else { DM_COM_SoapEnv_NS = NULL; }
   DM_ENG_FREE(_SoapEnvAttrName);
   DM_ENG_FREE(_SoapSkeleton);
   DM_ENG_FREE(_EnvelopeTagName);
   DM_ENG_FREE(_BeginEnvelopeTag);
   DM_ENG_FREE(_EndEnvelopeTag);
   DM_ENG_FREE(_HeaderTagName);
   DM_ENG_FREE(_BeginHeaderTag);
   DM_ENG_FREE(_BodyTagName);
   DM_ENG_FREE(_BeginBodyTag);
   DM_ENG_FREE(_FaultTagName);
   DM_ENG_FREE(DM_COM_ArrayTypeAttrName);
   DM_ENG_FREE(_MustUnderstandAttrName);
}

/**
 * @brief Function called by the HTTP Client engine in order to get the http content
 *
 * @param httpDataMsgString  
 * @param msgSize  
 *
 * @return Size of the data received
 *
 */
size_t
DM_HttpCallbackClientData(char   * httpDataMsgString,
                          size_t   msgSize)
{
   static char           * pMessage       = NULL;
   bool	                  MessageComplete = false;
   DM_SoapXml	            SoapMsg;

   // Dipslay Debug Info
   char* tmpStr = DM_ENG_strndup((char*) httpDataMsgString, msgSize);
   DBG( "message =\n%s\nMsgSize = %d", (char*) tmpStr, msgSize );

	// Make sure in case of Digest Authentication, the session is authorized
	// The string AUTHORIZATION_REQUIRED must be present in a non soap message
   if (NULL != strstr(tmpStr, AUTHORIZATION_REQUIRED))
   {
     WARN("HTTP SESSION UNAUTHORIZED - DIGEST AUTJENTICATION FAILED");
     DM_ENG_FREE(tmpStr);  
     return 0;
   }

	// -----------------------------------------------------------------------
	// Check if an exception message is sent by the ACS server
	// -----------------------------------------------------------------------
	if ( strstr( (char*)httpDataMsgString, JSP_EXCEPTION_ACS ) != 0 )
	{
    EXEC_ERROR( "Msg = '%s' ", (char*)httpDataMsgString );
    EXEC_ERROR( "An internal error have been detected which cannot valid the current request " );
    EXEC_ERROR( "For more information, see the TOMCAT log (/var/log/Tomcat5/catalina.out)!!  " );
    DM_ENG_FREE(tmpStr);
    return (-1) ;
	}

   // A vérifier validité lexicale du namespace trouvé !!
   if (( DM_COM_SoapEnv_NS == NULL ) || ( DM_COM_SoapEnv_NS == _DEFAULT_SOAPENV_NS))
   {
      char* c1 = strchr( tmpStr, '<' );
      char* c2 = strchr( tmpStr, ':' );
      if ( (c1==NULL) || (c2==NULL) || (c1>c2) || (c2-c1 > 15) )
      {
         WARN("Invalid SOAP message !");
         DM_ENG_FREE(tmpStr);  
         return 0;
      }
      DM_COM_SoapEnv_NS = DM_ENG_strndup( c1+1, c2-c1-1 );
      _initPrefixedTag();
   }
   DM_ENG_FREE(tmpStr);

	// Make sure a new SOAP message is coming while an imcomplete one is stored
  if ((pMessage != NULL) && (strstr( (char*)httpDataMsgString, _BeginEnvelopeTag ) != NULL)){
    EXEC_ERROR( "Can not complete the previous SOAP Message. Start a new SOAP Message" );
    DM_ENG_FREE(pMessage);
  }

	// -----------------------------------------------------------------------
	// At the beginning the message is NULL, then because the content can be separated
	// in several parts, we will need to concatenate each one.
	// -----------------------------------------------------------------------
	if ( pMessage == NULL )	{
    // --------------------------------------------------------------------------------
    // Look for the begin SOAP tag
    // --------------------------------------------------------------------------------
    if ( strstr( (char*)httpDataMsgString, _BeginEnvelopeTag ) == NULL ) {
      EXEC_ERROR( "The message received seems not to be SOAP!! Msg: %s", httpDataMsgString );
      return( msgSize );
    } else {
      DBG( "-----> The message received seem to be SOAP." );

      pMessage = DM_ENG_strndup( (char*)httpDataMsgString, msgSize );

      if ( strstr( (char*)httpDataMsgString, _EndEnvelopeTag ) == NULL)	{
        INFO(" Wait for other SOAP messages to complete the actual one..." );
      } else {
        DBG( "-----> SOAP message is complete!!" );
        MessageComplete = true;
      }
    }
	} else {  // if ( pMessage == NULL )
	  INFO( "Completing an existing message." );
	  	  
    // --------------------------------------------------------------------------------
    // Save the new part of the soap message / use realloc+strcat instead ??
    // --------------------------------------------------------------------------------
    pMessage = (char*)realloc( pMessage, strlen(pMessage)+strlen((char*)httpDataMsgString)+1 );
    sprintf( pMessage, "%s%s", pMessage, (char*)httpDataMsgString );
    if ( strstr( (char*)httpDataMsgString, _EndEnvelopeTag ) == NULL) {
      INFO(" SOAP message not complete yet!!"                            );
      INFO(" Wait for other SOAP messages to complete the actual one..." );
    } else {
      INFO( "-----> SOAP message complete!!" );
      MessageComplete = true;
    }
	}

	// -------------------------------------------------------------------------
	// If the soap message is complete, parse it and run the methods one by one
	// -------------------------------------------------------------------------
	if ( MessageComplete )
   {
	  INFO("------------------------------------------------");
	  INFO("- SOAP MESSAGE RECEIVED: ACS --> CPE           -");
     //INFO( "%s", pMessage );
     INFO("------------------------------------------------");

    // --------------------------------------------------------------------------------
    // Convert the SOAP message into a XML tree
    // --------------------------------------------------------------------------------
    DM_InitSoapMsgReceived( &SoapMsg );
    if(DM_OK == DM_AnalyseSoapMessage( &SoapMsg, pMessage, TYPE_ACS, false )) {

      // --------------------------------------------------------------------------------
      // Parse the XML tree in order to launch the RPC methods
      // --------------------------------------------------------------------------------
      DM_ParseSoapEnveloppe(SoapMsg.pBody, SoapMsg.pSoapID, SoapMsg.nHoldRequest);
			
    } else {
      EXEC_ERROR("Invalid SOAP Message. Close the session.");
    }

    xmlDocumentFree(SoapMsg.pParser);
    
    DM_InitSoapMsgReceived( &SoapMsg );
    
    if(NULL != pMessage) {
      DM_ENG_FREE(pMessage);
    }
    MessageComplete = false;
	}
	
	return( msgSize );
}






/**
 * @brief Function called by the HTTP Client engine in order to get the http content
 *
 * @param http message  
 * @param http message size  
 * @param lastHttpResponseCode  (HTTP_OK 200 , HTTP_CREATED 201, ...)
 * @param stream	Pointer to the buffer which contain the HTTP header	
 *
 * @return Size of the data received
 *
 */
size_t
DM_HttpCallbackClientHeader(char   * httpHeaderMsgString,
                            size_t   msgSize,
			                      int      lastHttpResponseCode)
{
  char       * pData	  = NULL;
	static int   nRedirectedCounter = 0;

	DBG( "message =\n%s\nMsgSize = %d\nhttpResponseCode = %d", (char*)httpHeaderMsgString, msgSize, lastHttpResponseCode  );
	
	// -------------------------------------------------------------------------
	// Read the data
	// -------------------------------------------------------------------------
   size_t dataSize = msgSize;
   while ((dataSize > 0) && (httpHeaderMsgString[dataSize-1] < ' ')) { dataSize--; } // on ignore les derniers caractères CR, LF, ...
	pData  = (char*)malloc( dataSize + 1 );
	memcpy( pData, httpHeaderMsgString, dataSize );
	*(pData+dataSize) = '\0';

	DBG( "HTTP Header received = '%s'", pData );
	if ( !strncmp( pData, HTTP_VERSION, 5 ) ) {
 
    // -------------------------------------------------------------------------
    // Analyse the return code
    // -------------------------------------------------------------------------
    switch ( lastHttpResponseCode )  {
	    // -------------------------------------------------------------------------
	    // 100 : Continue HTTP message
	    // -------------------------------------------------------------------------
	    case HTTP_CONTINUE: {
        DBG ( "Receiving a HTTP_CONTINUE message from the ACS server " );
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // 200 : Correct http message
	    // -------------------------------------------------------------------------
	    case HTTP_OK: {
        DBG ( "Received (not empty) HTTP_OK message from the ACS server" );
        g_DmComData.Acs.EmptyMessage = false;
        ignoreHttpNoContentMsgFlag = false;
	
	      if(true == cwmpSessionClosingFlag) {
	        WARN("cwmpSessionClosingFlag is set to true. Close the session");
		      _closeACSSession(true);
	      }
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // 204 : Correct and empty HTTP message
	    // -------------------------------------------------------------------------
	    case HTTP_NO_CONTENT: {

	      if(ignoreHttpNoContentMsgFlag == true) {
	        // The first HTTP_NO_CONTENT is ignored.
		      // This message is due to the HTTP Session Creation and is not part of the TR069 Protocol.
	        INFO("Ignore the HTTP No Content Messsage");
		      ignoreHttpNoContentMsgFlag = false;
		      break;
	      }
	    
        DBG ( "Receiving (empty) HTTP_NO_CONTENT message from the ACS server" );
        g_DmComData.Acs.EmptyMessage = true;
    
        // --------------------------------------------------------------------------------
        // Test to check if we can close the session
        // If so, send an empty POST HTTP/1.1 message to the ACS server
        // -------------------------------------------------------------------------------
	
	      // Take the mutex to use bSession
        DM_CMN_Thread_lockMutex(mutexAcsSession);

        if ( g_DmComData.bSession ) {
	        if ( !g_DmComData.bIRTCInProgess ){
            DBG( "Ask to the DM_ENGINE if we can close the session..." );
            g_DmComData.bIRTCInProgess = DM_ENG_IsReadyToClose( DM_ENG_EntityType_ACS ); // On ne tient pas compte du pSoapMsg->nHoldRequest si réponse vide
            DBG( "Session = %d - IRTC = %d", g_DmComData.bSession, (int)g_DmComData.bIRTCInProgess );
            if ( g_DmComData.bIRTCInProgess ) {
	            DBG( "DM_ENGINE ready to close the session : OK" );

	            // ------------------------------------------------------------
	            // Send the HTTP message
	            // ------------------------------------------------------------
	            if ( DM_SendHttpMessage( EMPTY_HTTP_MESSAGE ) == DM_OK ) {
		            WARN("Set cwmpSessionClosingFlag to true");
		            cwmpSessionClosingFlag = true;
                DBG( "Empty HTTP Msg - Sending: OK" );
	            } else {
                EXEC_ERROR( "Empty HTTP Msg - Sending: NOK" );
	            }

            } // end if ( bIRTCInProgess )
	        } else {
            // Close the ACS Session
	          _closeACSSession(true);
            INFO( "Closing current session : OK" );
	        }
        } else {
	        EXEC_ERROR( "DM_ENGINE ready to close the session : NOK " );
        }
        // Free the mutex
		  	DM_CMN_Thread_unlockMutex(mutexAcsSession);
	
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // 301 : Found
	    // -------------------------------------------------------------------------
	    case HTTP_MOVED_PERMANENTLY: {
        if ( nRedirectedCounter < NB_REDIRECT ) {
	        nRedirectedCounter++;
	        DBG( "Redirecting URL..." );
	        // TODO : resend the previous message to the current URL
        } else {
	        nRedirectedCounter = 0;
	        EXEC_ERROR( "Don't allow more than %d redirections!!", NB_REDIRECT );
        }
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // 302 : Moved Temporarily
	    // -------------------------------------------------------------------------
	    case HTTP_MOVED_TEMPORARILY1: {
        if ( nRedirectedCounter < NB_REDIRECT ) {
	        nRedirectedCounter++;
	        DBG( "Redirecting URL..." );
	        // TODO : resend the previous message to the current URL
        } else {
	        nRedirectedCounter = 0;
	        EXEC_ERROR( "Don't allow more than %d redirections!!", NB_REDIRECT );
        }
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // 307 : Temporary redirect
	    // -------------------------------------------------------------------------
	    case HTTP_MOVED_TEMPORARILY2: {
        if ( nRedirectedCounter < NB_REDIRECT ) {
	        nRedirectedCounter++;
	        DBG( "Redirecting URL..." );
	        // TODO : resend the previous message ti the current URL
        } else {
	        nRedirectedCounter = 0;
	        EXEC_ERROR( "Don't allow more than %d redirections!!", NB_REDIRECT );
        }
	    }
	    break;
	  
	    // -------------------------------------------------------------------------
	    // Other cases
	    // -------------------------------------------------------------------------
	    default: {
        DBG ( " (%d)-> Received unknown HTTP message from the ACS server",
        lastHttpResponseCode );
        g_DmComData.Acs.EmptyMessage = false;
	    }
	    break;
    } // end switch

#ifdef DM_COM_CLOSE_ON_HTTP_200_ALLOWED
   } else if ((lastHttpResponseCode == HTTP_OK) && (strcmp(pData, _CONTENT_LENGTH_0) == 0))  {
      if (g_DmComData.bIRTCInProgess && !cwmpSessionClosingFlag)
      {
         if ( DM_SendHttpMessage( EMPTY_HTTP_MESSAGE ) == DM_OK ) {
            WARN("Set cwmpSessionClosingFlag to true");
            cwmpSessionClosingFlag = true;
            DBG( "Empty HTTP Msg - Sending: OK" );
         } else {
            EXEC_ERROR( "Empty HTTP Msg - Sending: NOK" );
         }
      }
#endif

	} else { // end begin of header (HTTP VERSION)
	  DBG( "dm_com -  DM_CallbackClientHeader - No need to analyse the HTTP Message Header." );
	}
	
	// Free the memory previously allocated
	DM_ENG_FREE(pData );
	
	return( msgSize );
}


/**
 * @brief Function which initialize the SOAP structure
 *
 * @param pSoapMsg soap structure to initialize
 *
 * @return return DM_OK is okay else DM_ERR
 */
DMRET
DM_InitSoapMsgReceived(IN DM_SoapXml *pSoapMsg)
{
	DMRET nRet = DM_ERR;
	DBG("DM_InitSoapMsgReceived");
	
	// Check parameter
	if ( pSoapMsg != NULL )	{
    pSoapMsg->pParser   	  = NULL;
    pSoapMsg->pRoot     	  = NULL;
    pSoapMsg->pHeader   	  = NULL;
    pSoapMsg->pBody     	  = NULL;
    pSoapMsg->nHoldRequest	= 0;
    // bzero( pSoapMsg->pSoapID, sizeof(pSoapMsg->pSoapID) );
    memset((void *) pSoapMsg->pSoapID, '\0', sizeof(pSoapMsg->pSoapID) );
    nRet = DM_OK;
	} else {
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nRet );
}

/**
 * @brief Function which analyse/prepare a SOAP before to be parse
 *
 * @param pSoapMsg  XML object
 * @param pMessage  XML buffer to analyse
 * @param nTypeSoap TYPE_CPE or TYPE_ACS
 * @param allEnveloppeAttributs (true --> attr encoding, enveloppe, XMLSchema, XMLSchema-instance and 
                                          urn:dslforum-org:cwmp-1-0  ==> Inform and GetParameterValuesResponse
					  										 false --> attr encoding, enveloppe and urn:dslforum-org:cwmp-1-0 ==> All other cases )
 *
 * @return return DM_OK is okay else ERROR_INVALID_PARAMETERS
 *
 * @remarks DM_AnalyseSoapMessage( &g_DmComData, pMessage )
 *
 * @remarks only one SOAP enveloppe must be support ; all others must be ignored
 */
DMRET
DM_AnalyseSoapMessage(
  IN DM_SoapXml	 * pSoapMsg,
  IN const char	 * pMessage,
  IN int           nTypeSoap,
  IN bool          allEnveloppeAttributs)
{
  DMRET           nRet            = DM_ERR;
  char          * nodeHeaderIdStr = NULL;

  if (DM_COM_SoapEnv_NS == NULL) { _initPrefixedTag(); }

  // Check the parameters
  if (pSoapMsg != NULL)
  {
    if (pMessage == NULL) { pMessage = _SoapSkeleton; }

    // Parses an XML text buffer converting it into an XML DOM representation.
    pSoapMsg->pParser = xmlStringBufferToXmlDocument((char *) pMessage);
    
    if (NULL == pSoapMsg->pParser) {
      EXEC_ERROR( "Reading the SOAP buffer : NOK " );
      return( nRet );
    }
    
    // Build the Skeleton (add nodes, attributs and values
    if(false == _buildSoapMessageSkeleton(pSoapMsg->pParser, allEnveloppeAttributs)) {
      EXEC_ERROR("Can not build the SOAP Message Skeleton");
      xmlDocumentFree(pSoapMsg->pParser);
      pSoapMsg->pParser = NULL;
      return( nRet );
    }
    
    // ----------------------------------------------------------------------------------
    // Convert the SOAP message into a XML way
    // ----------------------------------------------------------------------------------

    // ----------------------------------------------------------------------------------
    // Find the root of the XML
    // ----------------------------------------------------------------------------------
    pSoapMsg->pRoot = xmlGetFirstChildNodeOfDocument(pSoapMsg->pParser);

    if ( NULL == pSoapMsg->pRoot) {
      EXEC_ERROR( "Finding the XML root : NOK " );
      // Free xml document
      xmlDocumentFree(pSoapMsg->pParser);
      pSoapMsg->pParser = NULL;
      return( nRet );
    }

    // ----------------------------------------------------------------------------------
    // Look for tags
    // ----------------------------------------------------------------------------------
    // Find the SOAP header
    if( nTypeSoap == TYPE_ACS ) {
      // To fix compil warning
    }

    pSoapMsg->pHeader = xmlGetFirstNodeWithTagName(xmlNodeToDocument(pSoapMsg->pRoot), _HeaderTagName); 

    if ( pSoapMsg->pHeader == NULL) {
      EXEC_ERROR( "Looking for the SOAP header : NOK - Given type = %d", nTypeSoap );
      xmlDocumentFree(pSoapMsg->pParser);
      pSoapMsg->pParser = NULL;
      return( nRet );
    }

    // ----------------------------------------------------------------------------------
    // Find the cwmp:ID which define the ID number of the current SOAP message
    // Usefull for the fault and response SOAP messages
    // ----------------------------------------------------------------------------------
    // <soap:Header>
    //   <cwmp:ID soap:mustUnderstand="1">1234</cwmp:ID>
    //   <cwmp:HoldRequests soap:mustUnderstand="1">0</cwmp:HoldRequests>
    // </soap:Header>
    // ----------------------------------------------------------------------------------
    /* Find the first Node of the XML Document */
    GenericXmlNodePtr nodeHeaderId = xmlGetFirstNodeWithTagName(xmlNodeToDocument(pSoapMsg->pHeader), HEADER_ID);
    if(NULL == nodeHeaderId) {
      DBG( "ID SOAP message : NOT FOUND (not a blocking condition) !!" );
    }

    // Retrieve the node HeaderId value and store it into pSoapMsg->pSoapID
    xmlGetNodeParameters(nodeHeaderId, NULL, &nodeHeaderIdStr);   
    
    if ( NULL == nodeHeaderIdStr){
      DBG( "ID SOAP message : NOT FOUND (not a blocking condition) !!" );
    } else {
      strncpy( pSoapMsg->pSoapID, (char*)nodeHeaderIdStr, HEADER_ID_SIZE);
      DBG( "ID SOAP message : %s ", pSoapMsg->pSoapID );
    }

    // ----------------------------------------------------------------------------------
    // Find the cwmp:HoldRequests which say that the ACS want to hold the connexion
    // ----------------------------------------------------------------------------------
    // <soap:Header>
    //   <cwmp:ID soap:mustUnderstand="1">1234</cwmp:ID>
    //   <cwmp:HoldRequests soap:mustUnderstand="1">0</cwmp:HoldRequests>
    // </soap:Header>
    // ----------------------------------------------------------------------------------

    GenericXmlNodePtr nodeHeaderHoldRequest = xmlGetFirstNodeWithTagName(xmlNodeToDocument(pSoapMsg->pHeader), HEADER_HOLDREQUEST);
    char      * nodeHeaderHoldRequestStr = NULL;
 
    if(NULL == nodeHeaderHoldRequest) {
      DBG( "Can not retrieve the HEADER_HOLDREQUEST (not a blocking condition) !!" );
    } else {
      xmlGetNodeParameters(nodeHeaderHoldRequest, NULL, &nodeHeaderHoldRequestStr);
      if ( nodeHeaderHoldRequestStr == NULL ){
        DBG( "HOLD REQUEST SOAP message : NOT FOUND (not a blocking condition) !!" );
      } else {
        pSoapMsg->nHoldRequest = atoi( nodeHeaderHoldRequestStr );
        DBG( "HOLDREQUEST : %d,  ", pSoapMsg->nHoldRequest );
      }
    }

    // ----------------------------------------------------------------------------------
    // find the SOAP body
    // ----------------------------------------------------------------------------------
    pSoapMsg->pBody = xmlGetFirstNodeWithTagName(xmlNodeToDocument(pSoapMsg->pRoot), _BodyTagName);

    if ( pSoapMsg->pBody == NULL) {
      EXEC_ERROR( "Looking for the SOAP body : NOK " );
      xmlDocumentFree(pSoapMsg->pParser);
      pSoapMsg->pParser = NULL;
      return( nRet );
    }

    // Everything is fine. Set nRet to DM_OK
     nRet = DM_OK;
	} else {
    // Error, Invalid Parameter
    nRet = (int ) ERROR_INVALID_PARAMETERS;
	}
	
	DBG("DM_AnalyseSoapMessage - End");

	return( nRet );
}

/**
 * @brief Function which parse a SOAP enveloppe and look for each body tag
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_ParseSoapEnveloppe(GenericXmlNodePtr bodyNodePtr, char* soapIdStr, unsigned int holdRequests)
{
  return DM_ParseSoapBodyMessage( bodyNodePtr, soapIdStr, holdRequests);
}

/**
 * @brief Function which parse a SOAP message and extract the RPC commands
 *
 * Example :
 * <soapenv:Body>
 *   <cwmp:GetParameterNames>
 *     <ParameterPath>Object.</ParameterPath>
 *     <NextLevel>0</NextLevel>
 *   </cwmp:GetParameterNames>
 * </soapenv:Body>
 *
 * @param pBody current reference to the body tag
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_ParseSoapBodyMessage(IN GenericXmlNodePtr pBody, IN char* soapIdStr, IN unsigned int holdRequests)
{
  DMRET                   nRet             = DM_ERR;
  char                  * rpcCmmandTypeStr = NULL;

  DBG("DM_ParseSoapBodyMessage - Begin");

  // Retrieve the list of child nodes (child nodes of soapenv:Body node)
  // Only one RPC command must exist per body
  GenericXmlNodeListPtr rpcNodeList = xmlGetChildNodesList(pBody);
  // Check only one one RPC exist 
  if(NULL == rpcNodeList) {
    EXEC_ERROR( "RPC Command List is Empty. No RPC Request in the Soap Message" );
    // Send a fault SOAP message to the ACS server (NOK)
    DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
    return nRet;
  }
  
  // Check there is only one RPC Command in the soap body
  if(MAX_NUMBER_OF_RPC_COMMAND_PER_BODY != xmlGetNumberOfChildNodes(pBody) ) {
    // More than 1 RPC Command in the body
    EXEC_ERROR( "The soapenv:Body contains %d RPC Cmd (max is %d)", (int )xmlGetNodesListLength(rpcNodeList),
                                                               MAX_NUMBER_OF_RPC_COMMAND_PER_BODY);
    // Send a fault SOAP message to the ACS server (NOK)
    DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
    // Free the rpcNodeList
    xmlFreeNodesList( rpcNodeList );
    return nRet;
  }

  // Get the RPC Node (first entry in the list)
  GenericXmlNodePtr rpcNode = xmlGetNodeFromNodesList(rpcNodeList, 0);
  GenericXmlDocumentPtr rpcDocNode = xmlNodeToDocument(rpcNode);

  // Free the rpcNodeList
  xmlFreeNodesList( rpcNodeList );

  // Get the name of the RPC Command
  xmlGetNodeParameters(rpcNode, &rpcCmmandTypeStr, NULL);

  INFO("RPC Command = %s", rpcCmmandTypeStr);
  
  INFO("SOAP Message: RPC: %s (soapId: %s)", rpcCmmandTypeStr , soapIdStr);
  
  // A message is received from the ACS. Update the ACS Session Timer.
  _updateAcsSessionTimer();


  if(0 == strcmp( rpcCmmandTypeStr, _FaultTagName )) {
    // -----------------------------------------------------------------
    // FAULT MESSAGE - BEGIN
    // -----------------------------------------------------------------
    // <soapenv:Envelope
    //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" 
    //	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
    //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    // <soapenv:Header>
    // <cwmp:ID
    //	soapenv:mustUnderstand="1"
    //	xsi:type="xsd:string" 
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1186754215138</cwmp:ID>
    // </soapenv:Header>
    // <soapenv:Body>
    // <soapenv:Fault>
    // <faultcode xsi:type="xsd:string">Server</faultcode>
    // <faultstring xsi:type="xsd:string">CWMP Fault</faultstring>
    // <detail>
    // 	<cwmp:Fault xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
    //   <FaultCode xsi:type="xsd:int">8002</FaultCode>
    //  <FaultString xsi:type="xsd:string">Problem during execution of operation inform : null</FaultString>
    //	</cwmp:Fault>
    // </detail>
    // </soapenv:Fault>
    // </soapenv:Body>
    // </soapenv:Envelope>
    // -----------------------------------------------------------------
    unsigned int            i;
    GenericXmlNodePtr       faultNode         = NULL;
    bool                    retryCpeRequest   = false; // Indicate if the Previous CPE Request must be retry.
    GenericXmlNodeListPtr   faultCodeNodeList = xmlGetNodesListWithTagName(rpcNode, FAULTCODE2);
    char                  * faultCodeStr      = NULL;
    int                     theFaultCode      = 0;

    
    if(NULL != faultCodeNodeList) {
      // Loop through all the FaultCode node to check if RetryRequest is present.
      for(i=0; i < xmlGetNodesListLength(faultCodeNodeList); i++){
	      faultNode = xmlGetNodeFromNodesList(faultCodeNodeList, i);
	      xmlGetNodeParameters(faultNode, NULL, &faultCodeStr);
        // Convert the string into int.
        if(DM_ENG_stringToInt(faultCodeStr, &theFaultCode)) {
          if(ACS_ERROR_RETRY_REQUEST == theFaultCode) {
	          // This is a Retry request
	          INFO("SOAP MSG ERROR 8005 (RETRY REQUEST DETECTED)");
	          retryCpeRequest = true;
	        } else {
	          // This is not a Retry Request
		        retryCpeRequest = false;
		        // Break: At least one error not equal to retry request
		        break;
	        }
        } else {
          EXEC_ERROR("Can not convert FaultCode string.");
	        retryCpeRequest = false;
          break;
        }
      }
      xmlFreeNodesList(faultCodeNodeList);
    } // end if(NULL != faultCodeNodeList)
    
    if(!retryCpeRequest) {
      // Look for a SOAP Header_ID in the global array (and remove it from HeaderID array.
      nRet = DM_RemoveHeaderIDFromTab( soapIdStr );
      if ( nRet != DM_OK ) {
        EXEC_ERROR( "The Header_ID %s of the soap message haven't been found in the",  soapIdStr);
        EXEC_ERROR( "global array. That's not a response of a previous message!!" );
      }
      
      // Force the ACS Session to close.
      _forceACSSessionToClose();
      
    } else {
      DBG("Perform Retry Request");
      // Perform retry request.
      _retryRequest();
    }

  } else if(0 == strcmp( rpcCmmandTypeStr, RPC_GETRPCMETHODS )){
    // -----------------------------------------------------------------
    // GET RPC METHOD - BEGIN
    // -----------------------------------------------------------------
    // <soapenv:Envelope
    //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
    //	xmlns:xsd="http://www.w3.org/2001/XMLSchema" 
    //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    // <soapenv:Header>
    // <cwmp:ID soapenv:mustUnderstand="1"
    //	xsi:type="xsd:string"
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1187869873996</cwmp:ID>
    //</soapenv:Header>
    // <soapenv:Body>
    // <cwmp:GetRPCMethods
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0"/>
    // </soapenv:Body>
    // </soapenv:Envelope>
    // -----------------------------------------------------------------

    if (!DM_ENG_IS_GETRPCMETHODS_SUPPORTED) {
    DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
    // Send a fault SOAP message to the ACS server (NOK)
    DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
    } else { 
      // The GET RPC Method is supported
      DBG( " RPC_GETRPCMETHODS - Begin");
      nRet = DM_SUB_GetRPCMethods( soapIdStr );
      if ( nRet == DM_ERR ) {
        EXEC_ERROR( "Result of the 'GetRPCMethods' method : NOK (%d) ", nRet );
      } else {
        DBG( "Result of the 'GetRPCMethods' : OK " );
      }
      DBG( " RPC_GETRPCMETHODS - End");
    } // end if(true != getRpcMethodSupportedOnDevice())
    
  } else if(0 == strcmp( rpcCmmandTypeStr, RPC_SETPARAMETERVALUES )) {
    // -----------------------------------------------------------------
    // NAME      = SETPARAMETERVALUES - BEGIN
    // MANDATORY = YES
    // -----------------------------------------------------------------
    // <soapenv:Envelope
    //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
    //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    // <soapenv:Header>
    // <cwmp:ID soapenv:mustUnderstand="1"
    //	xsi:type="xsd:string"
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1189610097240</cwmp:ID>
    // </soapenv:Header>
    // <soapenv:Body>
    // <cwmp:SetParameterValues
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
    //	<ParameterList>
    //  <ParameterValueStruct>
    //	  <Name xsi:type="xsd:string">Device.UserInterface.CurrentLanguage</Name>
    //	  <Value xsi:type="xsd:string">fr</Value>
    //  </ParameterValueStruct>
    //	</ParameterList>
    //	<ParameterKey/>
    // </cwmp:SetParameterValues>
    // </soapenv:Body>
    // </soapenv:Envelope>
    // -----------------------------------------------------------------
    
    if (!DM_ENG_IS_SETPARAMETERVALUES_SUPPORTED) {
      DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
      // Send a fault SOAP message to the ACS server (NOK)
      DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
    } else { 
      // The feature is supported
      GenericXmlNodeListPtr paramValStructNodeList = NULL;                                                             
      GenericXmlNodePtr paramValueNode             = NULL;                                                             
      GenericXmlNodePtr paramListNode = NULL; 
      GenericXmlNodePtr paramKeyNode  = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERKEY);   
      char          * parameterKeyValStr = NULL;
      unsigned int    nbParamValStructNodes;
      unsigned int    n;
      char          * parameterNameStr  = NULL;
      char          * parameterValueStr = NULL;
      char          * pValTypeStr       = NULL;
      int             valueTypeFound    = 0;
    
      paramListNode = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERLIST);  
      if(NULL == paramListNode) { // Make ACS and Karma Compatibility
        paramListNode = xmlGetFirstNodeWithTagName(rpcDocNode, CWMP_PARAM_PARAMETERLIST);  
      }

      // Check nodes pointers
      if((NULL == paramListNode) || (NULL == paramKeyNode)) {
         EXEC_ERROR( "SETPARAMETERVALUES - Can not get parameters node" );
	       // Send a fault SOAP message to the ACS server (NOK)
	     DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
         return nRet;
      }

      // Retrieve the parameter key's string
      xmlGetNodeParameters(paramKeyNode, NULL, &parameterKeyValStr);
      if(NULL == parameterKeyValStr) {
        DBG( "No content inside the ParameterKey tag (or empty value)" );
        parameterKeyValStr = _EMPTY;
      }

      paramValStructNodeList = xmlGetNodesListWithTagName(paramListNode, PARAMETERVALUESTRUCT);

      nbParamValStructNodes = xmlGetNodesListLength(paramValStructNodeList);

      if(0 == nbParamValStructNodes) {
         EXEC_ERROR( "PARAMETERVALUESTRUCT - No parameter to Set" );
         // Send a fault SOAP message to the ACS server (NOK)
         DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
         // Free the paramValStructNodeList
         xmlFreeNodesList(paramValStructNodeList);
         return nRet;
      }

      DM_ENG_ParameterValueStruct** pParameterList = DM_ENG_newTabParameterValueStruct(nbParamValStructNodes);

      for(n = 0; n < nbParamValStructNodes; n++) {
        GenericXmlDocumentPtr pvsNode = xmlNodeToDocument(xmlGetNodeFromNodesList(paramValStructNodeList, n));
      
        // Read the Name and value of the parameter
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(pvsNode, PARAM_NAME), NULL, &parameterNameStr);  
      
        // Read the value/content of the parameter
        paramValueNode = xmlGetFirstNodeWithTagName(pvsNode, PARAM_VALUE);
        xmlGetNodeParameters(paramValueNode, NULL, &parameterValueStr);
        if(NULL == parameterValueStr) {
          // Set empty value
          parameterValueStr = _EMPTY;
        }
        pValTypeStr    = xmlGetAttributValue(paramValueNode, ATTR_TYPE);

        // Check strings
        if((NULL == parameterNameStr) || (NULL == parameterValueStr) || (NULL == pValTypeStr)){
          EXEC_ERROR( "Some element(s) haven't been found in the soap message!!" );
	        xmlFreeNodesList( paramValStructNodeList );
           DM_ENG_deleteTabParameterValueStruct(pParameterList);
          // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
	        DM_ENG_FREE(pValTypeStr);
		
          return nRet;
        }

        // Convert the Value Type
        if(DM_OK != DM_FindTypeParameter(pValTypeStr, &valueTypeFound )){
          EXEC_ERROR( "Invalid Value Type" );
	        xmlFreeNodesList( paramValStructNodeList );
           DM_ENG_deleteTabParameterValueStruct(pParameterList);
          // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          DM_ENG_FREE(pValTypeStr);
          return nRet;
        }
        DM_ENG_FREE(pValTypeStr);
	
        // ------------------------------------------------------------
        // Add the information to the ParameterList
        // ------------------------------------------------------------
        DBG( "Param.%d : Name = '%s', Value = '%s' ", n, parameterNameStr, parameterValueStr);
        pParameterList[n] = DM_ENG_newParameterValueStruct(parameterNameStr,
                                                           (DM_ENG_ParameterType) valueTypeFound,
                                                           parameterValueStr );

      } // end for each parameterValueStructNode
       
      // Note: No need to set the last entry to NULL (it's already done above)

      // ------------------------------------------------------------
      // Launch the RPC method then analyse the result of the SetParameterValue RPC method
      // ------------------------------------------------------------
      nRet = DM_SUB_SetParameterValues( soapIdStr,
                                        pParameterList,
                                        parameterKeyValStr );
      DBG( "DM_SUB_SetParameterValues Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));

      // ------------------------------------------------------------
      // Free the memory previously allocated
      // ------------------------------------------------------------ 
      DM_ENG_deleteTabParameterValueStruct(pParameterList);

      // Free the list
      xmlFreeNodesList( paramValStructNodeList );
    }
    
    // -----------------------------------------------------------------
    // RPC_SETPARAMETERVALUES - END
    // -----------------------------------------------------------------

    } else if(0 == strcmp( rpcCmmandTypeStr, RPC_GETPARAMETERVALUES )){
    // -----------------------------------------------------------------
    // NAME      = GETPARAMETERVALUES
    // MANDATORY = YES
    // -----------------------------------------------------------------
    // <soapenv:Envelope
    //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
    //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    // <soapenv:Header>
    // <cwmp:ID
    //	soapenv:mustUnderstand="1"
    //	xsi:type="xsd:string"
    //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1189688625500</cwmp:ID>
    // </soapenv:Header>
    // <soapenv:Body>
    // <cwmp:GetParameterValues xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
    // <ParameterNames>
    // 	<string xsi:type="xsd:string">Device.UserInterface.CurrentLanguage</string>
    // </ParameterNames>
    // </cwmp:GetParameterValues>
    // </soapenv:Body>
    // </soapenv:Envelope>
    // -----------------------------------------------------------------
    
    if (!DM_ENG_IS_GETPARAMETERVALUES_SUPPORTED) {
      DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
      // Send a fault SOAP message to the ACS server (NOK)
      DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
    } else { 
      // The feature is supported
     
      DBG( " The %s RPC method is implemented.", rpcCmmandTypeStr);
      
      GenericXmlNodePtr      paramNamesNode      = NULL; 
      GenericXmlNodePtr      paramNamesChildNode = NULL;
      GenericXmlNodeListPtr  paramNamesList      = NULL;
    
      paramNamesNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERNAMES); 
      if(NULL == paramNamesNode) {
        // Try to find cwmp:ParameterNames (for ACS and Karma compatibility)
        DBG("NO %s try to find %s", PARAM_PARAMETERNAMES, CWMP_PARAM_PARAMETERNAMES);
        paramNamesNode      = xmlGetFirstNodeWithTagName(rpcDocNode, CWMP_PARAM_PARAMETERNAMES); 
      }
    
      unsigned int    nbParameter = 0;
      unsigned int    n = 0;
      char          * parameterNameStr    = NULL;

      if(NULL == paramNamesNode) {
        EXEC_ERROR( "No ParameterNames Node for GETPARAMETERVALUES RPC Command" );
	    	// Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
        return nRet;
      }

      // Retrieve the list of ParameterName to retrieve
      paramNamesList = xmlGetChildNodesList(paramNamesNode);

      // Check the list is not empty
      if(NULL == paramNamesList) {
        EXEC_ERROR( "No Parameter to retrieve for GETPARAMETERVALUES RPC Command" );
  	    // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
        return nRet;
      }

      // Get the number of element to retrieve
      nbParameter = xmlGetNodesListLength(paramNamesList);
    
      char** pChildParamName = (char**)calloc(nbParameter+1, sizeof(char*)); // calloc set memory to 0x00

      // Loop through all the parameters
      for(n = 0; n < nbParameter; n++) {
        paramNamesChildNode = xmlGetNodeFromNodesList(paramNamesList, n);
        // Read the Name of the parameter
        parameterNameStr = NULL;
        xmlGetNodeParameters(paramNamesChildNode, NULL, &parameterNameStr);
      
        if(NULL == parameterNameStr) {
	        // Set "" root
	        WARN("No parameter specified for GetParameterValues. Set root parameter");
	        parameterNameStr = _EMPTY;	
	     }

	     // Store the name 
        pChildParamName[n] = strdup( parameterNameStr );
        DBG( "Param.%d/%d = '%s' ", n, nbParameter, pChildParamName[n] );
      }
      // No need to set the last entry to NULL (it's already done above)

      // --------------------------------------------------------------
      // Launch the GetParameterValues RPC Method and analyse its result 
      // --------------------------------------------------------------
      DBG( " RPC method = GetParameterValues( <ParamName[%d]> ) ", nbParameter );
      nRet = DM_SUB_GetParameterValues( soapIdStr, (const char**)pChildParamName );

      DBG( "DM_SUB_GetParameterValues Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));

      // Free the list 
      xmlFreeNodesList( paramNamesList );

      // Free pParamName 
      DM_ENG_deleteTabString(pChildParamName);
    }

    // -----------------------------------------------------------------
    // RPC_GETPARAMETERVALUES - END
    // -----------------------------------------------------------------

    }  else if (0 == strcmp( rpcCmmandTypeStr, RPC_SETPARAMETERATTRIBUTES )){
      // -----------------------------------------------------------------
      // NAME      = RPC_SETPARAMETERATTRIBUTES
      // MANDATORY = YES
      // -----------------------------------------------------------------
 
     if (!DM_ENG_IS_SETPARAMETERATTRIBUTES_SUPPORTED) {
       DBG(" The %s RPC method is not implemented.", rpcCmmandTypeStr);
       // Send a fault SOAP message to the ACS server (NOK)
       DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
     } else {
        DBG(" The %s RPC method is implemented.", rpcCmmandTypeStr);
        // The feature is supported

        // Retrieve the ParameterList XML Node
	     GenericXmlNodePtr paramListNode = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERLIST); 
        if(NULL == paramListNode) { // Make ACS and Karma Compatibility
          paramListNode = xmlGetFirstNodeWithTagName(rpcDocNode, CWMP_PARAM_PARAMETERLIST);  
        }
	
	      if(NULL == paramListNode) {
	    	  // Send a fault SOAP message to the ACS server (NOK)
		      EXEC_ERROR("No Parameter List in the Message");
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
	        return nRet;
	      }
       
        // Retrieve the SetParameterAttributesStruct Nodes list
        GenericXmlNodeListPtr paramAttributeStructNodeList = xmlGetNodesListWithTagName(paramListNode, SETPARAMETERATTRIBUTESSTRUCT);
	
	      if(NULL == paramAttributeStructNodeList) {
	        // TEMP - Workaround for ACS Bug
	        WARN("No %s. Search %s for ACS Compatibility", SETPARAMETERATTRIBUTESSTRUCT, PARAMETERATTIBUTESSTRUCT);
		      paramAttributeStructNodeList = xmlGetNodesListWithTagName(paramListNode, PARAMETERATTIBUTESSTRUCT);
	      }
	
	      // Retrieve the node list size
        unsigned int nbParamAttributeStructNodes = xmlGetNodesListLength(paramAttributeStructNodeList);  

        if(0 == nbParamAttributeStructNodes) {
          WARN( "Set Parameter Attributes - No parameter to Set" );
          // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          // Free the paramValStructNodeList
          xmlFreeNodesList(paramAttributeStructNodeList);
          return nRet;
        }
        
	      // Allocate memory for the ParameterAttributesStruct Array
        DM_ENG_ParameterAttributesStruct ** pParameterList = DM_ENG_newTabParameterAttributesStruct(nbParamAttributeStructNodes);
	
        unsigned int n;
        for(n = 0; n < nbParamAttributeStructNodes ; n++)
        {
           GenericXmlDocumentPtr pasNode = xmlNodeToDocument(xmlGetNodeFromNodesList(paramAttributeStructNodeList, n));

           char* paramNameStr = NULL;
           char* paramNotificationChangeFlagStr = NULL;
           char* paramNotificationChangeValStr = NULL;
           char* paramAccessListChangeFlagStr = NULL;

           bool notificationChange = false;
           bool accessListChange = false;

           char** accessListArray = NULL;
           DM_ENG_NotificationMode notification;

		     // Read the Name, the Notifcation Change flag, the Notification and the AccessList
	        // Retrieve the Name Node
	        GenericXmlNodePtr paramNameNode = xmlGetFirstNodeWithTagName(pasNode, PARAM_NAME);

	        // Retrieve the Notification Change Flag Node
           GenericXmlNodePtr paramNotificationChangeFlagNode = xmlGetFirstNodeWithTagName(pasNode, PARAM_NOTIFICATION_CHANGE);	  

	        // Retrieve the Access List Change Flag Node
           GenericXmlNodePtr paramAccessListChangeFlagNode = xmlGetFirstNodeWithTagName(pasNode, PARAM_ACCESSLIST_CHANGE);

           if((NULL != paramNameNode) && (NULL != paramNotificationChangeFlagNode) && (NULL != paramAccessListChangeFlagNode))
           {
				   // Retrieve the node's values
				   xmlGetNodeParameters(paramNameNode,                   NULL, &paramNameStr);
				   xmlGetNodeParameters(paramNotificationChangeFlagNode, NULL, &paramNotificationChangeFlagStr);
               DM_ENG_stringToBool(paramNotificationChangeFlagStr, &notificationChange);

				   xmlGetNodeParameters(paramAccessListChangeFlagNode,   NULL, &paramAccessListChangeFlagStr);
               DM_ENG_stringToBool(paramAccessListChangeFlagStr, &accessListChange);
				   
				   if (NULL != paramNameStr)
               {
				      // Check paramNotificationChangeFlagStr Change required
				      if (notificationChange)
                  {
				        DBG("Notification flag set to true");
                // Retrieve the Notification Change Value
                GenericXmlNodePtr paramNotificationChangeValNode = xmlGetFirstNodeWithTagName(pasNode, PARAM_NOTIFICATION);		
		            xmlGetNodeParameters(paramNotificationChangeValNode, NULL, &paramNotificationChangeValStr);
               if(NULL != paramNotificationChangeValStr) {
                  int notif;
                 DM_ENG_stringToInt(paramNotificationChangeValStr, &notif);
                 notification = (DM_ENG_NotificationMode)notif;
	              } else {
		              WARN("No Value for Parameter Notification Change - Set it to undefined");
		              notification = DM_ENG_NotificationMode_UNDEFINED;
		            }
				      } else {
				        // No change required
					      DBG("Notification flag set to false");
					      notification = DM_ENG_NotificationMode_UNDEFINED;
				      }

				      // Check paramAccessListChangeFlagStr Change required
				      if (accessListChange)
                  {
				         DBG("Access List flag set to true");

			            // Retrieve the AccessList node
                     GenericXmlNodePtr paramAccessListNode =  xmlGetFirstNodeWithTagName(pasNode, PARAM_ACCESSLIST);

                     if(NULL == paramAccessListNode) {
                       EXEC_ERROR( "Missing AccessList in SetParameterAttributes RPC Command" );
                      // Send a fault SOAP message to the ACS server (NOK)
                       DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
                       return nRet;
                     }

                     unsigned int    nbAccessList = 0;
                     char          * accessListStr    = NULL;

                     // Retrieve the list of strings
                     GenericXmlNodePtr paramAccessListStringNodeList = xmlGetChildNodesList(paramAccessListNode);

                     // Check the list is not empty
                     if(NULL == paramAccessListStringNodeList) {
                       EXEC_ERROR( "Missing AccessList in SetParameterAttributes RPC Command" );
                      // Send a fault SOAP message to the ACS server (NOK)
                       DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
                       return nRet;
                     }

                     // Get the number of element to retrieve
                     nbAccessList = xmlGetNodesListLength(paramAccessListStringNodeList);
                     DBG("AccessList size = %d", nbAccessList);

                     // Allocate memory for the Acces List Array
                     accessListArray = (char**)calloc(nbAccessList+1, sizeof(char*)); // calloc set memory to 0x00

                     // Loop through all the parameters
                     unsigned int i = 0;
                     for(i = 0; i < nbAccessList; i++) {
                       GenericXmlNodePtr accessListChildNode = xmlGetNodeFromNodesList(paramAccessListStringNodeList, i);
                       // Read the Name of the parameter
                       accessListStr = NULL;
                       xmlGetNodeParameters(accessListChildNode, NULL, &accessListStr);
                       DBG("AccessList Value %d/%d = %s", i, nbAccessList, accessListStr);

                       // Store the name 
                       accessListArray[i] = (accessListStr == NULL ? _EMPTY : accessListStr);
                     }

                     // Free the list
                     xmlFreeNodesList( paramAccessListStringNodeList );
				      }

				      // Add the parameter structure into the parameter structure list
				      DBG("Param: %s, Notification: %d, AccesssList[0]: %s", paramNameStr, notification, (accessListArray == NULL ? "NULL" : accessListArray[0]));
				      pParameterList[n] = DM_ENG_newParameterAttributesStruct(paramNameStr, notification, accessListArray);
                  if (accessListArray != NULL) { free(accessListArray); }

				   } else {
				     // Error invalid parameter
				     EXEC_ERROR("Invalid ParameterAttribute");
				   }

				} else {
				  // Error - Invalid Parameter
				  EXEC_ERROR("Invalid ParameterAttribute");
				}     
	      } // end for

	      // Set the parameters Attributs
	      DM_SUB_SetParameterAttributes(soapIdStr, pParameterList);	      
	      
	      // Free the Tab
	      DM_ENG_deleteTabParameterAttributesStruct(pParameterList);
	      
	      // Free the Node list
	      xmlFreeNodesList(paramAttributeStructNodeList);
      }

    } else if (0 ==  strcmp( rpcCmmandTypeStr, RPC_GETPARAMETERATTRIBUTES )){
      // -----------------------------------------------------------------
      // NAME      = RPC_GETPARAMETERATTRIBUTES
      // MANDATORY = YES
      // -----------------------------------------------------------------
      DBG( " The RPC method '%s' have been found", rpcCmmandTypeStr );
      
      if (!DM_ENG_IS_GETPARAMETERATTRIBUTES_SUPPORTED) {
        DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else {
        DBG( " The %s RPC method is implemented.", rpcCmmandTypeStr);
        GenericXmlNodePtr      paramNamesNode      = NULL; 
        GenericXmlNodePtr      paramNamesChildNode = NULL;
        GenericXmlNodeListPtr  paramNamesList      = NULL; 
	
        paramNamesNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERNAMES); 
        if(NULL == paramNamesNode) {
          // Try to find cwmp:ParameterNames (for ACS and Karma compatibility)
          DBG("NO %s try to find %s", PARAM_PARAMETERNAMES, CWMP_PARAM_PARAMETERNAMES);
          paramNamesNode      = xmlGetFirstNodeWithTagName(rpcDocNode, CWMP_PARAM_PARAMETERNAMES); 
        }	
	
        if(NULL == paramNamesNode) {
          EXEC_ERROR( "No ParameterNames Node for GETPARAMETERATTRIBUTES RPC Command" );
	    	  // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }	

        unsigned int nbParameter = 0;
        unsigned int n = 0;
        char* parameterNameStr = NULL;		       

        // Retrieve the list of ParameterName to retrieve
        paramNamesList = xmlGetChildNodesList(paramNamesNode);

        // Check the list is not empty
        if(NULL == paramNamesList) {
          EXEC_ERROR( "No Parameter to retrieve for GETPARAMETERATTRIBUTES RPC Command" );
  	      // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }		
	
        // Get the number of element to retrieve
        nbParameter = xmlGetNodesListLength(paramNamesList);
	      DBG("**** RPC_GETPARAMETERATTRIBUTES - nbParameter = %d ****", nbParameter);
    
	      char** pChildParamNameArray = (char**)calloc(nbParameter+1, sizeof(char*)); // calloc set memory to 0x00

        // Loop through all the parameters
        for(n = 0; n < nbParameter; n++) {
          paramNamesChildNode = xmlGetNodeFromNodesList(paramNamesList, n);
  
          // Read the Name of the parameter
          parameterNameStr = NULL;
          xmlGetNodeParameters(paramNamesChildNode, NULL, &parameterNameStr);
      
          if(NULL == parameterNameStr) {
	          // Set "/" root
	          WARN("No parameter specified for GetParameterAttributes. Set root parameter");
	          parameterNameStr = _EMPTY;	
	       }
 	  
	       // Store the name 
          pChildParamNameArray[n] = strdup ( parameterNameStr );
          DBG( "Param.%d/%d = '%s' ", n, nbParameter, pChildParamNameArray[n] );
        } // end for

	      // No need to set the last entry to NULL (it's already done by the calloc)
		    	 
        // --------------------------------------------------------------
        // Launch the GetParameterAttributes RPC Method and analyse its result 
        // --------------------------------------------------------------
        nRet = DM_SUB_GetParameterAttributes( soapIdStr, (const char**)pChildParamNameArray );
    
        DBG( "DM_SUB_GetParameterAttributes Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));

        // Free the list
        xmlFreeNodesList( paramNamesList );
    
        // Free pChildParamNameArray
        DM_ENG_deleteTabString(pChildParamNameArray);
      }

    } else if (0 ==  strcmp( rpcCmmandTypeStr, RPC_GETPARAMETERNAMES )){
      // -----------------------------------------------------------------
      // NAME      = RPC_GETPARAMETERNAMES
      // MANDATORY = YES
      // -----------------------------------------------------------------
			DBG( " The RPC method '%s' have been found", rpcCmmandTypeStr );
      if (!DM_ENG_IS_GETPARAMETERNAMES_SUPPORTED) {
        DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else { 
        // The feature is supported
			  char 	            * pValParameterPath     = NULL;
			  char 	            * pValNextLevel         = NULL;			
        GenericXmlNodePtr   pValParameterPathNode = NULL; 
        GenericXmlNodePtr   pValNextLevelNode     = NULL;       
    
        pValParameterPathNode  = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERPATH); 
        xmlGetNodeParameters(pValParameterPathNode, NULL, &pValParameterPath);
      
        pValNextLevelNode = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_NEXTLEVEL); 
        xmlGetNodeParameters(pValNextLevelNode, NULL, &pValNextLevel);
	
        bool bNextLevel;
		  if (DM_ENG_stringToBool(pValNextLevel, &bNextLevel))
        {
           if (NULL == pValParameterPath) { pValParameterPath = (char*)DM_ENG_PARAMETER_PREFIX; }
	   
           DBG( " RPC method = DM_SUB_GetParameterNames( '%s' , '%s' ) ", pValParameterPath, pValNextLevel );
           nRet = DM_SUB_GetParameterNames( soapIdStr, pValParameterPath, bNextLevel);
	   
          if ( nRet != 0 ) {
            EXEC_ERROR( "Result of the 'DM_SUB_GetParameterNames' method : NOK (%d) ", nRet );
          } else {
            DBG( "Result of the 'DM_SUB_GetParameterNames' method : OK " );
          }
        } else {
	        DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
				  EXEC_ERROR( "One of the two parameters 'ObjectName' or 'ParameterKey' expected by the " );
				  EXEC_ERROR( "'DM_SUB_GetParameterNames' RPC method haven't been found in the soap message!!" );
			  }
	     } // end feature is supported
    } else if (0 ==  strcmp( rpcCmmandTypeStr, RPC_ADDOBJECT )){
      // -----------------------------------------------------------------
      // NAME      = RPC_ADDOBJECT
      // MANDATORY = YES
      // -----------------------------------------------------------------
			DBG( " The RPC method '%s' have been found", rpcCmmandTypeStr );
      if (!DM_ENG_IS_ADDOBJECT_SUPPORTED) {
        DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else { 
        // The feature is supported
			  char 	          * pValObjectName   = NULL;
			  char 	          * pValParameterKey = NULL;			
        GenericXmlNodePtr objNode          = NULL; 
        GenericXmlNodePtr paramKeyNode     = NULL;       
    
        objNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_OBJECTNAME); 
        xmlGetNodeParameters(objNode, NULL, &pValObjectName);
      
        paramKeyNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERKEY);
        if (paramKeyNode != NULL)
        {
           xmlGetNodeParameters(paramKeyNode, NULL, &pValParameterKey);
           if (NULL == pValParameterKey) { pValParameterKey = _EMPTY; }
        }

		  if ((pValObjectName != NULL) && (pValParameterKey != NULL)){
          DBG( " RPC method = AddObject( '%s' , '%s' ) ", (char*)pValObjectName, (char*)pValParameterKey );
          nRet = DM_SUB_AddObject( soapIdStr,
                                  (char*)pValObjectName,
                                  (char*)pValParameterKey );
          if ( nRet != 0 ) {
            EXEC_ERROR( "Result of the 'AddObject' method : NOK (%d) ", nRet );
          } else {
            DBG( "Result of the 'AddObject' method : OK " );
          }
        } else {
	        DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
				  EXEC_ERROR( "One of the two parameters 'ObjectName' or 'ParameterKey' expected by the " );
				  EXEC_ERROR( "'AddObject' RPC method haven't been found in the soap message!!" );
			  }	
			}
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_DELETEOBJECT )){
      // -----------------------------------------------------------------
      // NAME      = RPC_DELETEOBJECT
      // MANDATORY = YES
      // -----------------------------------------------------------------

			DBG( " The RPC method '%s' have been found", rpcCmmandTypeStr );
      if (!DM_ENG_IS_DELETEOBJECT_SUPPORTED) {
        DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else { 
        // The feature is supported
        char 	            * pValObjectName   = NULL;
			  char 	            * pValParameterKey = NULL;			
        GenericXmlNodePtr   objNode          = NULL; 
        GenericXmlNodePtr   paramKeyNode     = NULL;       
    
        objNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_OBJECTNAME); 
        xmlGetNodeParameters(objNode, NULL, &pValObjectName);
      
        paramKeyNode      = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_PARAMETERKEY); 
        if (paramKeyNode != NULL)
        {
           xmlGetNodeParameters(paramKeyNode, NULL, &pValParameterKey);
           if (NULL == pValParameterKey) { pValParameterKey = _EMPTY; }
        }

		  if ((pValObjectName != NULL) && (pValParameterKey != NULL)){
          DBG( " RPC method = DeleteObject( '%s' , '%s' ) ", (char*)pValObjectName, (char*)pValParameterKey );
          nRet = DM_SUB_DeleteObject( soapIdStr,
                                     (char*)pValObjectName,
                                     (char*)pValParameterKey );
          if ( nRet != 0 ) {
            EXEC_ERROR( "Result of the 'DeleteObject' method : NOK (%d) ", nRet );
          } else {
            DBG( "Result of the 'DeleteObject' method : OK " );
          }
        } else {
	        DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
				  EXEC_ERROR( "One of the two parameters 'ObjectName' or 'ParameterKey' expected by the " );
				  EXEC_ERROR( "'DeleteObject' RPC method haven't been found in the soap message!!" );
			  }
		  }   // end if feature is supported   
      
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_REBOOT )){
      // -----------------------------------------------------------------
      // NAME      = RPC_REBOOT
      // MANDATORY = YES
      // -----------------------------------------------------------------
      
			DBG( " The RPC method '%s' have been found", rpcCmmandTypeStr );
      if (!DM_ENG_IS_REBOOT_SUPPORTED) {
        DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else { 
        // The feature is supported

        GenericXmlNodePtr   pRefCommandKey  = xmlGetFirstNodeWithTagName(rpcDocNode, PARAM_COMMANDKEY);
        if (NULL == pRefCommandKey)
        {
           // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
           pRefCommandKey = xmlGetFirstNodeWithTagName(rpcDocNode, CWMP_PARAM_COMMANDKEY);
        }
			 
		  if ( pRefCommandKey != NULL ) {
          char * pValCommandKey = NULL;
          xmlGetNodeParameters(pRefCommandKey, NULL, &pValCommandKey);
           if(NULL == pValCommandKey) { pValCommandKey = _EMPTY; }
				
          // If commandKey is not set, set this value to the NULL ((void*)0) !!
          DBG( " RPC method = Reboot( cwmpID = %s, CmdKey = %s ) ", soapIdStr, pValCommandKey );
          nRet = DM_SUB_Reboot( soapIdStr, pValCommandKey );

			  } else {
			    DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
				  EXEC_ERROR( "The parameter 'CommandKey' expected for the 'Reboot' RPC method " );
				  EXEC_ERROR( "haven't been found in the soap message!!" );

			  }
			}  // end feature supported
      
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_DOWNLOAD )){
      // -----------------------------------------------------------------
      // NAME      = DOWNLOAD
      // MANDATORY = YES
      // -----------------------------------------------------------------
      // <soapenv:Envelope
      //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
      //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
      // <soapenv:Header>
      // <cwmp:ID soapenv:mustUnderstand="1"
      //	xsi:type="xsd:string"
      //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1193144023902</cwmp:ID>
      // </soapenv:Header>
      // <soapenv:Body>
      // <cwmp:Download xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
      // 	<CommandKey xsi:type="xsd:string">127</CommandKey>
      // 	<FileType xsi:type="xsd:string">1 Firmware Upgrade Image</FileType>
      // 	<URL xsi:type="xsd:string">ftp://192.168.1.5/mosaique.html</URL>
      // 	<Username xsi:type="xsd:string">cwmp</Username>
      // 	<Password xsi:type="xsd:string">cwmp</Password>
      // 	<FileSize xsi:type="xsd:long">0</FileSize>
      // 	<TargetFileName xsi:type="xsd:string">mosaique.html</TargetFileName>
      // 	<DelaySeconds xsi:type="xsd:int">0</DelaySeconds>
      // 	<SuccessURL xsi:type="xsd:string">mosaique.html</SuccessURL>
      // 	<FailureURL xsi:type="xsd:string"></FailureURL>
      // </cwmp:Download>
      // </soapenv:Body>
      // </soapenv:Envelope>
      // -----------------------------------------------------------------
      
    if (!DM_ENG_IS_DOWNLOAD_SUPPORTED) {
      DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
      // Send a fault SOAP message to the ACS server (NOK)
      DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
    } else { 
      // The feature is supported
      GenericXmlNodePtr downloadNode = xmlGetFirstNodeWithTagName(rpcDocNode, RPC_DOWNLOAD);
      char          * commandKeyValueStr = NULL;
      char          * fileTypeValueStr   = NULL;
      char          * urlValueStr        = NULL;
      char          * userNameValueStr   = NULL;
      char          * passwordValueStr   = NULL;
      char          * fileSizeValueStr   = NULL;
      char          * targetFileNameStr  = NULL;
      char          * delayValueStr      = NULL;
      char          * successUrlValueStr = NULL;
      char          * failureUrlValueStr = NULL;
      unsigned int    fileSize           = 0;
      unsigned int    delaySeconds       = 0;
      bool            validValue         = true;

        // -----------------------------------------------------------------
        // Field     : Commandkey
        // Mandatory : yes
        // -----------------------------------------------------------------

        GenericXmlNodePtr ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_COMMANDKEY);
        if (NULL == ckNode)
        {
           // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
           ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_COMMANDKEY);
        }

        if(NULL != ckNode)
        {
           xmlGetNodeParameters(ckNode, NULL, &commandKeyValueStr);  
           if(NULL == commandKeyValueStr) { commandKeyValueStr = _EMPTY; }
        }
        else
        {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No commandKey for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : FileType
        // Mandatory : yes
        // -----------------------------------------------------------------
        //	1 Firmware Upgrade Image
        //	2 Web Content
        //	3 Vendor Configuration File
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_FILETYPE), NULL, &fileTypeValueStr);  
        if(NULL == fileTypeValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_FILETYPE), NULL, &fileTypeValueStr);   
        }       
        if(NULL == fileTypeValueStr) {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No fileType for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : Url
        // Mandatory : yes
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_URL), NULL, &urlValueStr); 
        if(NULL == urlValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_URL), NULL, &urlValueStr);   
        }    
        if(NULL == urlValueStr) {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No URL for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : Username
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_USERNAME), NULL, &userNameValueStr);  
        if(NULL == userNameValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_USERNAME), NULL, &userNameValueStr);   
        } 
        if(NULL == userNameValueStr) {
          DBG( "No UserName for DOWNLOAD RPC Request.");
        }
       
        // -----------------------------------------------------------------
        // Field     : Password
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_PASSWORD), NULL, &passwordValueStr); 
        if(NULL == passwordValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_PASSWORD), NULL, &passwordValueStr);   
        }        
        if(NULL == passwordValueStr) {
          DBG( "No Password for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : FileSize
        // Mandatory : No (Default is 0 --> Ignore it)
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_FILESIZE), NULL, &fileSizeValueStr);
        if(NULL == fileSizeValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_FILESIZE), NULL, &fileSizeValueStr);   
        }           
        if(NULL == fileSizeValueStr) {
          // No error but debug (this parameter is not Mandatory)
          DBG( "No file size for DOWNLOAD RPC Request." );
	        fileSize = 0; // Default
        } else {
          validValue = DM_ENG_stringToUint(fileSizeValueStr, &fileSize);
        }

        // -----------------------------------------------------------------
        // Field     : TargetFileName
        // Mandatory : yes
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_TARGETFILENAME), NULL, &targetFileNameStr); 
        if(NULL == targetFileNameStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_TARGETFILENAME), NULL, &targetFileNameStr);   
        }  
        if(NULL == targetFileNameStr) {
          DBG( "No Target File Name for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : Delay
        // Mandatory : yes
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_DELAYSECONDS), NULL, &delayValueStr);  
        if(NULL == delayValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_DELAYSECONDS), NULL, &delayValueStr);   
        }  
        if(NULL == delayValueStr) {
          DBG( "No delay for DOWNLOAD RPC Request.");
          delaySeconds = 0;
        } else {
          validValue = validValue && DM_ENG_stringToUint(delayValueStr, &delaySeconds);
        }

        // -----------------------------------------------------------------
        // Field     : SuccessURL
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_SUCCESSURL), NULL, &successUrlValueStr);  
        if(NULL == successUrlValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
  	      xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_SUCCESSURL), NULL, &successUrlValueStr);   
        }  
        if(NULL == successUrlValueStr) {
          DBG( "No success URL for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : FailureURL
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_FAILUREURL), NULL, &failureUrlValueStr); 
        if(NULL == failureUrlValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_FAILUREURL), NULL, &failureUrlValueStr);   
        }        
        if(NULL == failureUrlValueStr) {
          DBG( "No failure URL for DOWNLOAD RPC Request.");
        }

        // Check parameter validity    
        if( (NULL != commandKeyValueStr) && 
            (NULL != fileTypeValueStr)   && 
            (NULL != urlValueStr)        &&
            validValue ) {
         
          nRet = DM_SUB_Download( soapIdStr,
                                  fileTypeValueStr,
                                  urlValueStr,
                                  userNameValueStr,
                                  passwordValueStr,
                                  fileSize,
                                  targetFileNameStr,
                                  delaySeconds,
                                  successUrlValueStr,
                                  failureUrlValueStr,
                                  commandKeyValueStr );
         
          DBG( "DM_SUB_Download Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));
         
        } else {
          // The request can not be performed
          EXEC_ERROR( "Invalid Parameter. The Download request can not be performed.");
	      // Send a fault SOAP message to the ACS server (NOK)
	      DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
        return nRet;
      }
      }
       
      // -----------------------------------------------------------------
      // DOWNLOAD - END
      // -----------------------------------------------------------------
       
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_FACTORYRESET )) {
      // -----------------------------------------------------------------
      // NAME      = RPC_FACTORYRESET
      // MANDATORY = NO
      // -----------------------------------------------------------------
     if (!DM_ENG_IS_FACTORYRESET_SUPPORTED) {
       DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
       // Send a fault SOAP message to the ACS server (NOK)
       DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
     } else { 
			 nRet = DM_SUB_FactoryReset( soapIdStr);
 			 if ( nRet != 0 ) {
         EXEC_ERROR( "Result of the 'FactoryReset' method : NOK (%d) ", nRet );
       } else {
         DBG( "Result of the 'FactoryReset' : OK " );
       }
     }

    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_SCHEDULEINFORM )) {
     // -----------------------------------------------------------------
     // NAME      = SCHEDULEINFORM
     // MANDATORY = YES
     // -----------------------------------------------------------------
     // <soapenv:Envelope
     //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
     //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
     //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
     // <soapenv:Header>
     // <cwmp:ID soapenv:mustUnderstand="1"
     //	xsi:type="xsd:string"
     //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1193144023902</cwmp:ID>
     // </soapenv:Header>
     // <soapenv:Body>
     // <cwmp:ScheduleInform>
     //   <DelaySeconds>Delay befor inform</DelaySeconds>
     //  <CommandKey>CommandKey</CommandKey>
     // </cwmp:ScheduleInform>
     // </soapenv:Body>
     // </soapenv:Envelope>
     // -----------------------------------------------------------------
     if (!DM_ENG_IS_SCHEDULEINFORM_SUPPORTED) {
       DBG( " The %s RPC method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
      DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
     } else { 
       // The feature is supported
       GenericXmlNodePtr scheduleInformNode = xmlGetFirstNodeWithTagName(rpcDocNode, RPC_SCHEDULEINFORM);  
       char          * delaySecondsValueStr = NULL;
       char          * commandKeyStr        = NULL;


       xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(scheduleInformNode), PARAM_DELAYSECONDS), NULL, &delaySecondsValueStr); 
       if(NULL ==  delaySecondsValueStr) {
         xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(scheduleInformNode), CWMP_PARAM_DELAYSECONDS), NULL, &delaySecondsValueStr); 
         if(NULL == delaySecondsValueStr) {
           EXEC_ERROR("NULL delaySecondsValueStr\n");
         } 
       }

        GenericXmlNodePtr ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(scheduleInformNode), PARAM_COMMANDKEY);
        if (NULL == ckNode)
        {
           ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(scheduleInformNode), CWMP_PARAM_COMMANDKEY);
        }

        if(NULL != ckNode)
        {
           xmlGetNodeParameters(ckNode, NULL, &commandKeyStr);  
           if(NULL == commandKeyStr) { commandKeyStr = _EMPTY; }
        }
        else
        {
           // Command Key Value could be NULL
           DBG("NULL commandKeyStr (No Command Key Str provided)\n");
        }

       if(NULL != delaySecondsValueStr) {
         nRet = DM_SUB_ScheduleInform( soapIdStr,
                                       delaySecondsValueStr,
                                       commandKeyStr );
         DBG( "DM_SUB_ScheduleInform Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));

       } else {
         EXEC_ERROR( "Invalid Parameter. The SCHEDULEINFORM request can not be performed.");
	      // Send a fault SOAP message to the ACS server (NOK)
	      DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
        return nRet;
       }
     }
       
     // -----------------------------------------------------------------
     // SCHEDULEINFORM - END
     // -----------------------------------------------------------------

    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_UPLOAD )) {
      // -----------------------------------------------------------------
      // NAME      = RPC_UPLOAD
      // MANDATORY = NO
      // -----------------------------------------------------------------
      if (!DM_ENG_IS_UPLOAD_SUPPORTED) {
        DBG( " The %s RPC Method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else {

        GenericXmlNodePtr    downloadNode       = xmlGetFirstNodeWithTagName(rpcDocNode, RPC_UPLOAD);
        char               * commandKeyValueStr = NULL;
        char               * fileTypeValueStr   = NULL;
        char               * urlValueStr        = NULL;
        char               * userNameValueStr   = NULL;
        char               * passwordValueStr   = NULL;
        char               * delayValueStr      = NULL;
        unsigned int         delaySeconds       = 0;
        bool                 validValue         = true;

        // -----------------------------------------------------------------
        // Field     : Commandkey
        // Mandatory : yes
        // -----------------------------------------------------------------

        GenericXmlNodePtr ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_COMMANDKEY);
        if (NULL == ckNode)
        {
           ckNode = xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_COMMANDKEY);
        }

        if(NULL == ckNode) {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No commandKey for DOWNLOAD RPC Request.");
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }

        xmlGetNodeParameters(ckNode, NULL, &commandKeyValueStr);  
        if(NULL == commandKeyValueStr) { commandKeyValueStr = _EMPTY; }

        // -----------------------------------------------------------------
        // Field     : FileType
        // Mandatory : yes
        // -----------------------------------------------------------------
        //	1 Vendor Configuration File
        //	2 Vendor Log File
        //	X OUI Vendor-specific identifier
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_FILETYPE), NULL, &fileTypeValueStr);  
        if(NULL == fileTypeValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_FILETYPE), NULL, &fileTypeValueStr);   
        }       
        if(NULL == fileTypeValueStr) {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No fileType for DOWNLOAD RPC Request.");
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }
      
        // -----------------------------------------------------------------
        // Field     : Url
        // Mandatory : yes
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_URL), NULL, &urlValueStr); 
        if(NULL == urlValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_URL), NULL, &urlValueStr); 
        }
        if(NULL == urlValueStr) {
          // This is a mandatory Parameter Value
          EXEC_ERROR( "No URL for DOWNLOAD RPC Request.");
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }

        // -----------------------------------------------------------------
        // Field     : Username
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_USERNAME), NULL, &userNameValueStr);
        if(NULL == userNameValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_USERNAME), NULL, &userNameValueStr);
        }
        if(NULL == userNameValueStr) {
          DBG( "No UserName for DOWNLOAD RPC Request.");
        }

        // -----------------------------------------------------------------
        // Field     : Password
        // Mandatory : No
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_PASSWORD), NULL, &passwordValueStr); 
        if(NULL == passwordValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_PASSWORD), NULL, &passwordValueStr);   
        } 
        if(NULL == passwordValueStr) {
          DBG( "No Password for DOWNLOAD RPC Request.");
        }
      
        // -----------------------------------------------------------------
        // Field     : Delay
        // Mandatory : yes
        // -----------------------------------------------------------------
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), PARAM_DELAYSECONDS), NULL, &delayValueStr);  
        if(NULL == delayValueStr) {
          // TRY with cwmp:CommandKey (ACS and KARMA Compatibility)
	        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(downloadNode), CWMP_PARAM_DELAYSECONDS), NULL, &delayValueStr);   
        }  
        if(NULL == delayValueStr) {
          DBG( "No delay for DOWNLOAD RPC Request.");
          delaySeconds = 0;
        } else {
          validValue = DM_ENG_stringToUint(delayValueStr, &delaySeconds);
        }

        // Check parameter validity    
        if( (NULL != commandKeyValueStr) && 
            (NULL != fileTypeValueStr)   && 
            (NULL != urlValueStr)        &&
            validValue ) {

          nRet = DM_SUB_Upload( soapIdStr,
                                fileTypeValueStr,
                                urlValueStr,
                                userNameValueStr,
                                passwordValueStr,
				                        delaySeconds,
                                commandKeyValueStr  );
         
          DBG( "DM_SUB_Upload Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));
         
        } else {
          // The request can not be performed
          EXEC_ERROR( "Invalid Parameter. The Upload request can not be performed.");
          // Send a fault SOAP message to the ACS server (NOK)
          DM_SoapFaultResponse( soapIdStr, DM_ENG_INVALID_ARGUMENTS );
          return nRet;
        }
      
	
      }


    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_GETQUEUEDTRANSFERS )) {
      // -----------------------------------------------------------------
      // NAME      = RPC_GETQUEUEDTRANSFERS
      // MANDATORY = NO
      // -----------------------------------------------------------------
      DBG( " The %s RPC Method is not implemented.", rpcCmmandTypeStr);
      // Send a fault SOAP message to the ACS server (NOK)
      DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      
    } else if ((0 == strcmp( rpcCmmandTypeStr, RPC_GETALLQUEUEDTRANSFERS ))) {
      // -----------------------------------------------------------------
      // NAME      = RPC_GETALLQUEUEDTRANSFERS
      // MANDATORY = NO
      // -----------------------------------------------------------------
      if (!DM_ENG_IS_GETALLQUEUEDTRANSFERS_SUPPORTED) {
        DBG( " The %s RPC Method is not implemented.", rpcCmmandTypeStr);
        // Send a fault SOAP message to the ACS server (NOK)
        DM_SoapFaultResponse( soapIdStr, DM_ENG_METHOD_NOT_SUPPORTED );
      } else {
        DBG( " The %s RPC Method is implemented.", rpcCmmandTypeStr);
	
	      // Retrieve the queued transfers and perform the response.
	      DM_SUB_GetAllQueuedTransferts(soapIdStr);
	      
      }
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_SETVOUCHERS )) {
      // -----------------------------------------------------------------
      // NAME      = RPC_SETVOUCHERS
      // MANDATORY = NO
      // -----------------------------------------------------------------
      DBG( " The %s RPC Method is not implemented.", rpcCmmandTypeStr);
    } else if (0 == strcmp( rpcCmmandTypeStr, RPC_GETOPTIONS )) {
      // -----------------------------------------------------------------
      // NAME      = RPC_GETOPTIONS
      // MANDATORY = NO
      // -----------------------------------------------------------------
      DBG( " The %s RPC Method is not implemented.", rpcCmmandTypeStr);
    } else if (0 == strcmp( rpcCmmandTypeStr, INFORMRESPONSE )) {
      // -----------------------------------------------------------------
      // NAME      = INFORMRESPONSE
      // MANDATORY = YES
      // -----------------------------------------------------------------
      // -----------------------------------------------------------------
      // RESPONSE MESSAGE FROM THE ACS SERVER
      // -----------------------------------------------------------------
      // NAME = INFORMRESPONSE
      // -----------------------------------------------------------------
      // <soapenv:Envelope
      //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
      //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
      // <soapenv:Header>
      // <cwmp:ID
      //	soapenv:mustUnderstand="1"
      //	xsi:type="xsd:string"
      //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">8402</cwmp:ID>
      // </soapenv:Header>
      // <soapenv:Body>
      // <cwmp:InformResponse
      //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
      // <MaxEnvelopes
      //	xsi:type="xsd:int">2</MaxEnvelopes>
      // </cwmp:InformResponse>
      // </soapenv:Body>
      // </soapenv:Envelope>
      // -----------------------------------------------------------------
      char          * maxEnveloppeValueStr = NULL;
      GenericXmlNodePtr informResponseNode = xmlGetFirstNodeWithTagName(rpcDocNode, INFORMRESPONSE);  
      
      // If no InformResponse is received and the CWMP Session Terminated due to
      // Timeout, the CPE must use the cwmp-1-0 flag instead of cwmp-1-1
      atLeastOneInformResponseReceivedWithThisAcsTerminated = true;

      if ( DM_OK == DM_RemoveHeaderIDFromTab( soapIdStr )){
         
        xmlGetNodeParameters(xmlGetFirstNodeWithTagName(xmlNodeToDocument(informResponseNode), INFORMRESPONSE_MAXENVELOPES), 
	                                                      NULL,
							                                          &maxEnveloppeValueStr);   

        if(NULL == maxEnveloppeValueStr) {
          EXEC_ERROR( "MaxEnveloppes value not specified.");
        } else {
          // Set the global data
          g_DmComData.Acs.nMaxEnvelopes = (int)atoi(maxEnveloppeValueStr);
        }

        DBG( "Ask the IsReadyToClose() function" );
        if(holdRequests || DM_ENG_IsReadyToClose( DM_ENG_EntityType_ACS )) {
           if (holdRequests == 0) { g_DmComData.bIRTCInProgess = true; }
	        DBG("Send an EMPTY_HTTP_MESSAGE");
          nRet = DM_SendHttpMessage( EMPTY_HTTP_MESSAGE );
          DBG("DM_SendHttpMessage Result: %s", (nRet==DM_OK ? "OK" : "ERROR"));
        }
      } else { // DM_OK != DM_RemoveHeaderIDFromTab( soapIdStr ))
        EXEC_ERROR( "SOAP RESPONSE - Inform received but could not  ");
        EXEC_ERROR( "associated with a previous HEADER_ID sent by the CPE to the ACS!!" );
      }

      // -----------------------------------------------------------------
      // INFORMRESPONSE - END
      // -----------------------------------------------------------------
       
    } else if (0 == strcmp( rpcCmmandTypeStr, TRANSFERTCOMPLETERESPONSE )) {
      // -----------------------------------------------------------------
      // NAME      = TRANSFERTCOMPLETERESPONSE
      // MANDATORY = NO
      // -----------------------------------------------------------------
      // <soapenv:Envelope
      //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
      //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
      // <soapenv:Header>
      // <cwmp:ID
      //	soapenv:mustUnderstand="1"
      //	xsi:type="xsd:string"
      //	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">same as TransferComplete</cwmp:ID>
      // </soapenv:Header>
      // <soapenv:Body>
      // <cwmp:TransferCompleteResponse xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
      // </cwmp:TransferCompleteResponse>
      // </soapenv:Body>
      // </soapenv:Envelope>
      // -----------------------------------------------------------------
       
      // Look for a SOAP Header_ID in the global array
      if ( DM_OK == DM_RemoveHeaderIDFromTab( soapIdStr )) {
        // --------------------------------------------------------------------------------
        // Test to check if we can close the session
        // If so, send an empty POST HTTP/1.1 message to the ACS server
        // -------------------------------------------------------------------------------
		    
		    // Take the mutex to use bSession
        DM_CMN_Thread_lockMutex(mutexAcsSession);
	
        if ( g_DmComData.bSession ) {
          // Ask the DM_ENGINE if the session can be closed
          if ( holdRequests || DM_ENG_IsReadyToClose( DM_ENG_EntityType_ACS ) ) {
            DBG( "DM_ENGINE ready to close the session : OK" );
           if (holdRequests == 0) { g_DmComData.bIRTCInProgess = true; }
            // Send the HTTP message
            if ( DM_OK == DM_SendHttpMessage( EMPTY_HTTP_MESSAGE ) ) {
              DBG( "Transfer Complete Response - Sending: OK" );
              nRet = DM_OK;
            } else {
              DBG( "Transfer Complete Response - Sending: NOK" );
            }
          }
        } // end if ( g_DmComData.bSession )
      } else {
        EXEC_ERROR( "SOAP RESPONSE - TransfertComplete received but can not "          );
        EXEC_ERROR( "be associatedwith a previous HEADER_ID sent by the CPE to the ACS!!"  );
      }
      
      // Free the mutex
			DM_CMN_Thread_unlockMutex(mutexAcsSession);
      
      // -----------------------------------------------------------------
      // TRANSFERTCOMPLETERESPONSE - END
      // -----------------------------------------------------------------
    }
    
 else if (0 == strcmp( rpcCmmandTypeStr, REQUESTDOWNLOADRESPONSE )) {
      // -----------------------------------------------------------------
      // NAME      = REQUESTDOWNLOADRESPONSE
      // MANDATORY = NO
      // -----------------------------------------------------------------
      // <soapenv:Envelope
      //	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
      //	xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      //	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
      // <soapenv:Header>
      // <cwmp:ID soapenv:mustUnderstand="1">same as RequestDownload</cwmp:ID>
      // </soapenv:Header>
      // <soapenv:Body>
      // <cwmp:RequestDownloadResponse></cwmp:TransferCompleteResponse>
      // </soapenv:Body>
      // </soapenv:Envelope>
      // -----------------------------------------------------------------
       
      // Look for a SOAP Header_ID in the global array
      if ( DM_OK == DM_RemoveHeaderIDFromTab( soapIdStr )) {
        // --------------------------------------------------------------------------------
        // Test to check if we can close the session
        // If so, send an empty POST HTTP/1.1 message to the ACS server
        // -------------------------------------------------------------------------------
		    
		    // Take the mutex to use bSession
        DM_CMN_Thread_lockMutex(mutexAcsSession);
	
        if ( g_DmComData.bSession ) {
          // Ask the DM_ENGINE if the session can be closed
          if ( holdRequests || DM_ENG_IsReadyToClose( DM_ENG_EntityType_ACS ) ) {
            DBG( "DM_ENGINE ready to close the session : OK" );
            if (holdRequests == 0) { g_DmComData.bIRTCInProgess = true; }
            // Send the HTTP message
            if ( DM_OK == DM_SendHttpMessage( EMPTY_HTTP_MESSAGE ) ) {
              DBG( "Request Download Response - Sending: OK" );
              nRet = DM_OK;
            } else {
              DBG( "Request Download Response - Sending: NOK" );
            }
          }
        } // end if ( g_DmComData.bSession )
      } else {
        EXEC_ERROR( "SOAP RESPONSE - RequestDownloadResponse received but can not "          );
        EXEC_ERROR( "be associatedwith a previous HEADER_ID sent by the CPE to the ACS!!"  );
      }
      
      // Free the mutex
			DM_CMN_Thread_unlockMutex(mutexAcsSession);
      
      // -----------------------------------------------------------------
      // REQUESTDOWNLOADRESPONSE - END
      // -----------------------------------------------------------------
    } else {
      // INVALID MESSAGE
      EXEC_ERROR( "An unknown RPC method '%s' have been found.", rpcCmmandTypeStr );
      // Force ACS Session to Close
      _forceACSSessionToClose();

  }
	
	return( nRet );
}

/**
 * @brief Call be the HTTP Server to notify when the server is started
 *
 * @param none
 *
 * @return return None
 *
 */
void DM_HttpServerStarted()
{
  DBG("HTTP Server Started");
  _httpServerStatus = true;

}

/**
 * @brief Find the type of a parameter
 *
 * @param pParameterType	Type of the parameter to find
 * @param nTypeFound  Number which define the type found
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_FindTypeParameter(IN  const char *pParameterType,
       OUT       int  *nTypeFound)
{
	DMRET	nRet	= DM_ERR;
	
	// Check Parameters
	if ( (pParameterType != NULL) && (nTypeFound != NULL) )
	{
  // Integer
  if ( strcmp( pParameterType, XSD_INT ) == 0 )              { *nTypeFound = DM_ENG_ParameterType_INT;       }

  // Unsigned integer
  else if ( strcmp( pParameterType, XSD_UNSIGNEDINT ) == 0 ) { *nTypeFound = DM_ENG_ParameterType_UINT;      }

  // Long
  else if ( strcmp( pParameterType, XSD_LONG ) == 0 )        { *nTypeFound = DM_ENG_ParameterType_LONG;      }

  // Boolean
  else if ( strcmp( pParameterType, XSD_BOOLEAN ) == 0 )     { *nTypeFound = DM_ENG_ParameterType_BOOLEAN;   }

  // Datetime
  else if ( strcmp( pParameterType, XSD_DATETIME ) == 0 )    { *nTypeFound = DM_ENG_ParameterType_DATE;      }

  // String
  else if ( strcmp( pParameterType, XSD_STRING ) == 0 )      { *nTypeFound = DM_ENG_ParameterType_STRING;    }

  // ANY kind (other kind of)
  else if ( strcmp( pParameterType, XSD_ANY ) == 0 )         { *nTypeFound = DM_ENG_ParameterType_ANY;       }

  // Unsigned integer
  else	         { *nTypeFound = DM_ENG_ParameterType_UNDEFINED; }
  nRet = DM_OK;
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}

	return( nRet );
} /* DM_FindTypeParameter */


/**
 * @brief function which remake the string buffer from a XML data structure
 *
 * @param pElement  Current element of the XML tree
 *
 * @Return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_RemakeXMLBufferCpe(GenericXmlNodePtr pElement)
{
  DMRET  nRet  = DM_ERR;

  if(NULL == pElement) {
    EXEC_ERROR( "Invalid Parameter" );
    return DM_ERR;
  }
  
  // If g_pBufferCpe is not NULL, free it.
  if(NULL != g_pBufferCpe) {
    DM_FreeTMPXMLBufferCpe();
  }
  
  g_pBufferCpe = xmlDocumentToStringBuffer(xmlNodeToDocument(pElement));
  
  if(NULL != g_pBufferCpe) {
    // XML Tree converted into String
    nRet = DM_OK;
  }
  
  return nRet;
	
} /* DM_RemakeXMLBufferCpe */

// ---------------------------------------------------------------------------------
// Sub functions of the Engine interface : DM_ENG_XXXX

/**
 * @brief Create and send an empty response Message
 *
 * @param pSoapId	SOAP Identifier
 * @param nFaultCode	tag of the response message	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SoapEmptyResponse(
  IN const char *pSoapId,
	IN const char *pResponseTag)
{
	DMRET  nMainRet       = DM_ERR;
	DM_SoapXml	SoapMsg;
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameter
	if ( (pResponseTag != NULL) && (pSoapId != NULL) )
	{
  // ---------------------------------------------------------------------------
  // Initialize the SOAP data structure
  // ---------------------------------------------------------------------------
  DM_FreeTMPXMLBufferCpe();
  DM_InitSoapMsgReceived( &SoapMsg );
  DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false ); 

  // ---------------------------------------------------------------------------
  // Update the SOAP HEADER
  // ---------------------------------------------------------------------------
  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
  // ---------------------------------------------------------------------------
  // Add the RESPONSE TAG tag
  // ---------------------------------------------------------------------------
  xmlAddNode(SoapMsg.pParser,             // XML Document
             SoapMsg.pBody,               // XML Parent Node
             (char*) pResponseTag,        // New Node TAG
             NULL,                        // Attr Name
             NULL,                        // Attr Value
             NULL);                       // Node Value
  
  // ---------------------------------------------------------------------------
  // Remake the XML buffer then send it to the ACS server
  // ---------------------------------------------------------------------------
  if ( SoapMsg.pRoot != NULL )
  {
	  DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
	  if ( g_pBufferCpe != NULL )
	  { 
	    // Send the HTTP message
    if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
	    DBG( "Empty Response - Sending http message : OK" );
    } else {
	    EXEC_ERROR( "Empty Response - Sending http message : NOK" );
    }
    
    // Free the buffer
    DM_FreeTMPXMLBufferCpe();
	  }
  }
  
  nMainRet = DM_OK;
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	if(NULL != SoapMsg.pParser) {
	  // Free xml document
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL;
	}

	return( nMainRet );
}

/**
 * @brief Create and send a SOAP response
 *
 * @param pSoapId	SOAP Identifier
 * @param nFaultCode	Return code of a RPC method	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SoapFaultResponse(
  IN const char *pSoapId,
	IN       int   nFaultCode)
{
	DMRET      nMainRet            = DM_ERR;
	char      pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml	SoapMsg;
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameter
	if ( pSoapId != NULL )
	{
  DBG( "Creating the SOAP Fault for the ACS server." );
  
  // ---------------------------------------------------------------------------
  // Initialize the SOAP data structure
  // ---------------------------------------------------------------------------
  DM_FreeTMPXMLBufferCpe();
  DM_InitSoapMsgReceived( &SoapMsg );
  DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

  
  // ---------------------------------------------------------------------------
  // Update the SOAP HEADER
  // ---------------------------------------------------------------------------
  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
  
  // ---------------------------------------------------------------------------
  // Update the DEFAULT's SOAP FAULT MESSAGE
  // ---------------------------------------------------------------------------
  // Add the 'Fault' tag
  // scew_element *pRefFaultTag     = scew_element_add( SoapMsg.pBody, FAULT_TAG_CPE );
  GenericXmlNodePtr faultCpeNode = xmlAddNode(SoapMsg.pParser, SoapMsg.pBody, _FaultTagName, NULL, NULL, NULL);
  
  // Add the 'faultcode' tag
  xmlAddNode(SoapMsg.pParser,        // doc
             faultCpeNode,           // parentNode
             FAULT_CODE,             // Node Name
             NULL,                   // Attribut Name
             NULL,                   // Attribut Value
             FAULT_CODE_CONTENT);    // Node Value  				    
  
  // Add the 'faultString' tag
	xmlAddNode(SoapMsg.pParser,        // doc
	           faultCpeNode,           // parentNode
	           FAULT_STRING,           // Node Name
	           NULL,                   // Attribut Name
	           NULL,                   // Attribut Value
	           FAULT_STRING_CONTENT);  // Node Value  			
					    
  
  // Add the 'detail' tag
  GenericXmlNodePtr faultDetailNode = xmlAddNode(SoapMsg.pParser, faultCpeNode, FAULT_DETAIL, NULL, NULL, NULL);  
	              
  // Add the 'Fault' tag
	GenericXmlNodePtr faultNode = xmlAddNode(SoapMsg.pParser,        // doc
	                                         faultDetailNode,        // parentNode
						                               FAULT,                  // Node Name
										                       NULL, //FAULT_ATTR,             // Attribut Name
													                 NULL, // _getCwmpVersionSupported(), // Attribut Value
															             NULL);                  // Node Value  			  
  
  // Add the 'FaultCode' tag and update its content
  snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", nFaultCode );

	xmlAddNode(SoapMsg.pParser,  // doc
	           faultNode,          // parentNode
	           FAULTCODE2,         // Node Name
	           NULL,               // Attribut Name
	           NULL,               // Attribut Value
	           (char*)pTmpBuffer); // Node Value  			  
  
	xmlAddNode(SoapMsg.pParser,       // doc
	           faultNode,             // parentNode
	           FAULTSTRING2,          // Node Name
	           NULL,                  // Attribut Name
	           NULL,                  // Attribut Value
              DM_ENG_getFaultString(nFaultCode)); // Node Value

  // ---------------------------------------------------------------------------
  // Remake the XML buffer then send it to the ACS server
  // ---------------------------------------------------------------------------
  if ( SoapMsg.pRoot != NULL ){
	  DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
    if ( g_pBufferCpe != NULL ) {
      // Send the HTTP message
      if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
	      DBG( "Soap Fault Response - Sending http message : OK" );
      } else {
	      EXEC_ERROR( "Soap Fault Response - Sending http message : NOK" );
      }
    
      // Free the buffer
      DM_FreeTMPXMLBufferCpe();
    
      nMainRet = DM_OK;
    } else {
      EXEC_ERROR( "Problem with the XML/SOAP buffer!!" );
	  }
  }
  
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	// Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser); 
	  SoapMsg.pParser = NULL;
	}

	return( nMainRet );
}

/**
 * @brief Function which initialize the temp. XML buffer
 *
 * @param none
 *
 * @Return DM_OK
 *
 */
DMRET
DM_FreeTMPXMLBufferCpe()
{
	// Free g_retryBuffer
	DM_ENG_FREE(g_pBufferCpe);
	
	return( DM_OK );
} /* DM_FreeTMPXMLBufferCpe */

/**
 * @brief Function which update the retry buffer whith 
 *        the last emitted request.
 *
 * @param A Pointer on the message to store (string) and its size
 *
 * @Return DM_OK
 *
 */
DMRET
DM_UpdateRetryBuffer(const char * soapMsgToStoreStr, int size) 
{
  DBG("DM_UpdateRetryBuffer - Update the buffer to perform RetryRequest if needed");
  
  if(NULL == soapMsgToStoreStr) {
    DBG("No message to store");
    return DM_OK;
  }

  // Free g_retryBuffer
  DM_ENG_FREE(g_retryBuffer);

  if(0 != size) {
    // Copy g_pBufferCpe into g_retryBuffer
    g_retryBuffer = strdup(soapMsgToStoreStr);
  }

  return (DM_OK);

}

/**
 * @brief Call the DM_ENG_GetRPCMethods
 *
 * @param pSoapId ID of the SOAP request
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * Example of GetRPCMethodsResponse message :
 *
 * <soapenv:Envelope
 * xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
 *	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
 *	xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 *	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *	xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soapenv:Header>
 *     <cwmp:ID
 *        soapenv:mustUnderstand="1"
 *	  xsi:type="xsd:string"
 *	  xmlns:cwmp="urn:dslforum-org:cwmp-1-0">2147483647</cwmp:ID>
 * </soapenv:Header>
 * <soapenv:Body>
 *   <cwmp:GetRPCMethodsResponse>
 *     <MethodList soapenc:arrayType="xsd:string[10]">
 *	 <string>GetRPCMethods</string>
 *	 <string>SetParameterValues</string>
 *	 <string>GetParameterValues</string>
 *	 <string>GetParameterNames</string>
 *	 <string>SetParameterAttributes</string>
 *	 <string>GetParameterAttributes</string>
 *	 <string>AddObject</string>
 *	 <string>DeleteObject</string>
 *	 <string>Reboot</string>
 *	 <string>Download</string>
 *	 </MethodList>
 *    </cwmp:GetRPCMethodsResponse>
 *  </soapenv:Body>
 * </soapenv:Envelope>
 *
 * @Remarks This method don't need to free the array given as a result.
 *
 */
DMRET
DM_SUB_GetRPCMethods(IN const char *pSoapId)
{
	int		      nRPCRet	        = DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET		      nMainRet         = DM_ERR;
	const char**   methodsList      = NULL;
	char		      pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml	  SoapMsg;
	
	// Check parameter
	if ( pSoapId != NULL )	{
	
    // Launching the RPC method from the DM_ENGINE
    DBG( " GetRPCMethods( EntityType_ACS, &methodsList ) " );
    nRPCRet = DM_ENG_GetRPCMethods( DM_ENG_EntityType_ACS,  &methodsList );

    // Check the RPC's return code
    if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (methodsList != NULL) ){
      int nI           = 0;
      int nNbRPCMEthod = 0;

      DBG( "GetRPCMethods : OK " );
      DBG( "Creating the SOAP Response for the ACS server." );
			
			// ---------------------------------------------------------------------------
			// Initialize the SOAP data structure then create the XML/SOAP response
			// --------------------------------------------------------------------------
			DM_FreeTMPXMLBufferCpe();
			DM_InitSoapMsgReceived( &SoapMsg );
			DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

		
			// ---------------------------------------------------------------------------
			// Update the SOAP HEADER & BODY
			// --> Add the GetRPCMethodsResponse tag
			// ---------------------------------------------------------------------------
			DM_AddSoapHeaderID( pSoapId, &SoapMsg );
      GenericXmlNodePtr pRefGetRPCMethodsResponse = xmlAddNode(SoapMsg.pParser,       // XML Document
                                                                   SoapMsg.pBody,         // XML Parent Node
                                                                   GETRPCMETHODSRESPONSE, // Node Name
                                                                   NULL,                  // Attribute Name
                                                                   NULL,                  // Attribute Value
                                                                   NULL);                 // Node Value
			
			// ---------------------------------------------------------------------------
			// Update the SOAP BODY
			// --> Count how many RPC methods have been given by the DM_Engine
			// ---------------------------------------------------------------------------
			nNbRPCMEthod = DM_ENG_tablen( (void**)methodsList );

			// ---------------------------------------------------------------------------
			// Add the MethodList with its attribute
	    GenericXmlNodePtr pRefMethodList = xmlAddNode(SoapMsg.pParser,            // XML Document
                                                    pRefGetRPCMethodsResponse,  // XML Parent Node
                                                    GETRPCMETHODSMETHODLIST,    // Node Name
								                                    NULL,                       // Attribute Name
														                        NULL,                       // Attribute Value
																		                NULL);                      // Node Value	  
			
			if ( nNbRPCMEthod < 10 ) {
				snprintf( pTmpBuffer, TMPBUFFER_SIZE, STRING_LIST_ATTR_VAL1, nNbRPCMEthod );
			} else {
				snprintf( pTmpBuffer, TMPBUFFER_SIZE, STRING_LIST_ATTR_VAL2, nNbRPCMEthod );
			}
			
			xmlAddNodeAttribut(pRefMethodList, DM_COM_ArrayTypeAttrName, pTmpBuffer);
			
			// ---------------------------------------------------------------------------
			// Add the RPC Methods
			for ( nI=0 ; nI<nNbRPCMEthod ; nI++ )	{
			  
        xmlAddNode(SoapMsg.pParser,          // XML Document
                   pRefMethodList,           // XML Parent Node
                   PARAM_STRING,             // New Node TAG
                   NULL,                     // Attribute Name
                   NULL,                     // Attribute Value
                   methodsList[nI] );        // New Node Value			  

			}
			
			// ---------------------------------------------------------------------------
			// Remake the XML buffer then send it to the ACS server
			// ---------------------------------------------------------------------------
			if ( SoapMsg.pRoot != NULL )
			{
				DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
				if ( g_pBufferCpe != NULL ) {
					// Send the message
					if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
						DBG( "Sending https/SOAP message (GetRPCMEthods response) : OK" );
					} else {
					  EXEC_ERROR( "Sending https/SOAP message (GetRPCMEthods response) : NOK" );
					}
					
					// Free the buffer
					DM_FreeTMPXMLBufferCpe();
				}
			}

			nMainRet = DM_OK;
		} else {
		  EXEC_ERROR( "GetRPCMethods : NOK (%d) ", nRPCRet );
			
			// Send a fault SOAP message to the ACS server (NOK)
			DM_SoapFaultResponse( pSoapId, nRPCRet );
		}
	} else {
	  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	// Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL; 
	}
	
	return( nMainRet );
} /* DM_SUB_GetRPCMethods */

/**
 * @brief Call the DM_ENG_GetParameterNames function
 *
 * @param pSoapId ID of the SOAP request
 * @param pPath
 * @param nNextLevel	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterNames(IN const char *pSoapId,
	   IN const char *pPath,
	   IN       bool  nNextLevel)
{


	int		  	  nRPCRet		= DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET		  	  nMainRet		= DM_ERR;
	DM_ENG_ParameterInfoStruct	**pResult		= NULL;
	char		  	  pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml	  	  SoapMsg;
		
	// Check parameters
	if ( (pSoapId !=NULL) && (pPath != NULL) ){
		// ---------------------------------------------------------------------------
		// Initialize the SOAP data structure
		// ---------------------------------------------------------------------------
		DM_InitSoapMsgReceived( &SoapMsg );
		
		// ---------------------------------------------------------------------------
		// Launching the RPC method from the DM_ENGINE
		// ---------------------------------------------------------------------------
		DBG( " GetParameterNames( EntityType_ACS, pPath, nNextLevel, pResult ) " );
		nRPCRet = DM_ENG_GetParameterNames( DM_ENG_EntityType_ACS,
					                             (char*)pPath,
					                              nNextLevel,
					                              &pResult );
			
		// ---------------------------------------------------------------------------
		// Check the RPC's return code
		// ---------------------------------------------------------------------------
		if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult != NULL) )	{
			int nI               = 0;
			int nNbParameterList = 0;
			
			DBG( "GetParameterName : OK " );
			DBG( "Creating the SOAP Response for the ACS server." );
			// ---------------------------------------------------------------------------
			// Create the XML/SOAP response
			// ---------------------------------------------------------------------------
         DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

			// ---------------------------------------------------------------------------
			// Update the SOAP HEADER
			// ---------------------------------------------------------------------------
			DM_AddSoapHeaderID( pSoapId, &SoapMsg );
			
			// ---------------------------------------------------------------------------
			// Add a GetParameterNameResponse tag
			// ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefGetParameterNameResponse = xmlAddNode(SoapMsg.pParser,       // XML Document
                                                                  SoapMsg.pBody,         // XML Parent Node
                                                                  GETPARAMETERNAMERESPONSE, // Node Name
                                                                  NULL,                  // Attribute Name
                                                                  NULL,                  // Attribute Value
                                                                  NULL);                 // Node Value			
			
	
			// ---------------------------------------------------------------------------
			// Add a ParameterList tag
			// ---------------------------------------------------------------------------
	    GenericXmlNodePtr pRefParameterList = xmlAddNode(SoapMsg.pParser,               // XML Document
                                                       pRefGetParameterNameResponse,  // XML Parent Node
                                                       PARAMETERINFOSTRUCT_LIST,      // Node Name
								                                       NULL,                          // Attribute Name
														                           NULL,                          // Attribute Value
																		                   NULL);                         // Node Value	  
																							
			// ---------------------------------------------------------------------------
			// Count how many parameter there is in the list, then add this information
			// as an attribute of the ParameterList tag
			// ---------------------------------------------------------------------------
			nNbParameterList = DM_ENG_tablen( (void**)pResult );

			DBG( "Number of parameters found = %d ", nNbParameterList );
			if ( nNbParameterList<10 ) {
				snprintf( pTmpBuffer, TMPBUFFER_SIZE, PARAMETERINFOSTRUCT_ATTR_VAL1, nNbParameterList );
			} else {
				snprintf( pTmpBuffer, TMPBUFFER_SIZE, PARAMETERINFOSTRUCT_ATTR_VAL2, nNbParameterList );
			}
      xmlAddNodeAttribut(pRefParameterList, DM_COM_ArrayTypeAttrName, pTmpBuffer);
			
			// ---------------------------------------------------------------------------
			// Add parameters in the ParameterList
			// ---------------------------------------------------------------------------
			for ( nI=0 ; nI<nNbParameterList ; nI++ )	{
				// Add a ParameterInfoStruct tag
        GenericXmlNodePtr pRefParamStruct = xmlAddNode(SoapMsg.pParser,       // XML Document
                                                       pRefParameterList,     // XML Parent Node
                                                       PARAMETERINFOSTRUCT,   // Node Name
                                                       NULL,                  // Attribute Name
                                                       NULL,                  // Attribute Value
                                                       NULL);                 // Node Value	
				
				
				
				// Add a Name tag with its value
        xmlAddNode(SoapMsg.pParser,             // XML Document
                   pRefParamStruct,             // XML Parent Node
                   PARAMETERINFOSTRUCT_NAME,    // Node Name
                   NULL,                        // Attribute Name
                   NULL,                        // Attribute Value
                   pResult[nI]->parameterName); // Node Value					
				
				
				
				
				
				// Add a writable tag with its value
				snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)pResult[nI]->writable );
        xmlAddNode(SoapMsg.pParser,             // XML Document
                   pRefParamStruct,             // XML Parent Node
                   PARAMETERINFOSTRUCT_WRITABLE,// Node Name
                   NULL,                        // Attribute Name
                   NULL,                        // Attribute Value
                   pTmpBuffer);                 // Node Value							
				
				
				
			}

			// ---------------------------------------------------------------------------
			// Remake the XML buffer then send it to the ACS server
			// ---------------------------------------------------------------------------
			if ( SoapMsg.pRoot != NULL ) {
				DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
				if ( g_pBufferCpe != NULL ) {
					if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
						DBG( "Sending http message : OK" );
					} else {
					  EXEC_ERROR( "Sending http message : NOK" );
					}
				
					// Free the buffer
					DM_FreeTMPXMLBufferCpe();
				}
			}
			
			// ---------------------------------------------------------------------------
			// Free the array given by the DM_Engine
			// ---------------------------------------------------------------------------
			DM_ENG_deleteTabParameterInfoStruct( pResult );
			
			nMainRet = DM_OK;
		} else {
		  EXEC_ERROR( "GetParameterName : NOK (%d) ", nRPCRet );
		
			// Send a fault SOAP message to the ACS server
			DM_SoapFaultResponse( pSoapId, nRPCRet );
		}
	} else {
	  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
// Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL; 
	}
	
	return( nMainRet );

} /* DM_SUB_GetParameterNames */

/**
 * @brief Call the SetParameterValues function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterList
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * Example of SetParameterValuesResponse message :
 *
 * <soapenv:Envelope
 * xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
 *	xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
 *	xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 *	xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *	xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soapenv:Header>
 * <cwmp:ID 
 *	soapenv:mustUnderstand="1"
 *	xsi:type="xsd:string"
 *	xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1189616705844</cwmp:ID>
 * </soapenv:Header>
 * <soapenv:Body>
 * <cwmp:SetParameterValuesResponse>
 * 	<Status>1</Status>
 * </cwmp:SetParameterValuesResponse>
 * </soapenv:Body>
 * </soapenv:Envelope>
 */
DMRET
DM_SUB_SetParameterValues(
  IN const char                  *pSoapId,
  IN       DM_ENG_ParameterValueStruct **pParameterList,
  IN const char                  *pParameterKey)
{
	DM_ENG_SetParameterValuesFault ** pFaults        = NULL;
	DMRET                      nMainRet	      = DM_ERR;
	int                        nRPCRet	      = DM_ENG_METHOD_NOT_SUPPORTED;
	char                       pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml                 SoapMsg;
	DM_ENG_ParameterStatus     Status;
	
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameters
   if ( (pSoapId != NULL) && (pParameterList != NULL) && (pParameterKey != NULL) )	{

   // ---------------------------------------------------------------------------
   // Initialize the SOAP data structure
   // ---------------------------------------------------------------------------
     DBG( "Creating the SOAP Response for the ACS server." );
     DM_InitSoapMsgReceived( &SoapMsg );
     DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );


  // ---------------------------------------------------------------------------
  // Launching the RPC method from the DM_ENGINE
  // ---------------------------------------------------------------------------
  DBG( " SetParameterValues( DM_ENG_EntityType_ACS, pParameterList, (char*)pParameterKey, &Status, &pFaults ) " );
  nRPCRet = DM_ENG_SetParameterValues( DM_ENG_EntityType_ACS,
                                pParameterList,
                                (char*)pParameterKey,
                                &Status,
                                &pFaults );

  
  // ---------------------------------------------------------------------------
  // Check the RPC's return code
  // ---------------------------------------------------------------------------
  if ( nRPCRet == RPC_CPE_RETURN_CODE_OK ) {
	  DBG( "DM_ENG_SetParameterValues : OK " );
  
	  // ---------------------------------------------------------------------------
	  // Update the SOAP HEADER
	  // ---------------------------------------------------------------------------
	  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
	  // ---------------------------------------------------------------------------
	  // Add a SetParameterValueResponse tag
	  // ---------------------------------------------------------------------------
    GenericXmlNodePtr setParamValuesRespNode = xmlAddNode(SoapMsg.pParser,             // XML Document
	                                                        SoapMsg.pBody,               // XML Parent Node
                                                          SETPARAMETERVALUESRESPONSE,  // New Node TAG
                                                          NULL,                        // Attr Name
                                                          NULL,                        // Attr Value
							                                            NULL);                       // Node Value
	  
	  // ---------------------------------------------------------------------------
	  // Add the status
	  // ---------------------------------------------------------------------------
	  DBG( " Status of the SetParameterValue = %d ", (int)Status);
	  snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)Status );
    xmlAddNode(SoapMsg.pParser,                  // XML Document
               setParamValuesRespNode,           // XML Parent Node
               SETPARAMETERVALUERESPONSESTATUS,  // New Node TAG
               NULL,                             // Attr Name
               NULL,                             // Attr Value
               pTmpBuffer);                      // Node Value
 
	        
	  // ---------------------------------------------------------------------------
	  // Remake the XML buffer then send it to the ACS server
	  // ---------------------------------------------------------------------------
	  if ( SoapMsg.pRoot != NULL ){
      DM_RemakeXMLBufferCpe( xmlDocumentToNode(SoapMsg.pRoot) );
      if ( g_pBufferCpe != NULL ) {
	      // Send the HTTP message
	      if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
          DBG( "SetParameterValues Response - Sending http message : OK" );
	      } else {
          EXEC_ERROR( "SetParameterValues Response - Sending http message : NOK" );
	      }
	            
	      // Free the buffer
	      DM_FreeTMPXMLBufferCpe();
      }
	  }

	  nMainRet = DM_OK;
	  
  } else {
	  EXEC_ERROR( "SetParameterValues : NOK (%d) ", nRPCRet );
	  DBG( "Creating the SOAP Fault for the ACS server." );
	  int nI                = 0;
	  int nNbParameterFault = 1;
	  
	  // ---------------------------------------------------------------------------
	  // Update the SOAP HEADER
	  // ---------------------------------------------------------------------------
	  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
	  // ---------------------------------------------------------------------------
	  // Update the DEFAULT's SOAP FAULT MESSAGE
	  // ---------------------------------------------------------------------------
	  // Add the 'Fault' tag
    GenericXmlNodePtr pRefFaultTag = xmlAddNode(SoapMsg.pParser, // XML Document
                                                SoapMsg.pBody,   // XML Parent Node
						                                    _FaultTagName,   // New Node TAG
										                            NULL,            // Attribute Name
													                      NULL,            // Attribute Value
															                  NULL);           // New Node Value
						    
	  
	  // Add the 'faultcode' tag
    xmlAddNode(SoapMsg.pParser,      // XML Document
               pRefFaultTag,         // XML Parent Node
		           FAULT_CODE,           // New Node TAG
		           NULL,                 // Attribute Name
		           NULL,                 // Attribute Value
		           FAULT_CODE_CONTENT);  // New Node Value
	  
	  // Add the 'faultString' tag
    xmlAddNode(SoapMsg.pParser,        // XML Document
               pRefFaultTag,           // XML Parent Node
		           FAULT_STRING,           // New Node TAG
		           NULL,                   // Attribute Name
		           NULL,                   // Attribute Value
		           FAULT_STRING_CONTENT);  // New Node Value
	  
	  
	  // Add the 'detail' tag
    GenericXmlNodePtr pRefDetail = xmlAddNode(SoapMsg.pParser, // XML Document
                                              pRefFaultTag,    // XML Parent Node
						                                  FAULT_DETAIL,    // New Node TAG
										                          NULL,            // Attribute Name
													                    NULL,            // Attribute Value
															                NULL);           // New Node Value
	  
	  // Add the 'Fault' tag
    GenericXmlNodePtr pRefFault = xmlAddNode(SoapMsg.pParser, // XML Document
                                             pRefDetail,      // XML Parent Node
						                                 FAULT,           // New Node TAG
										                         NULL,            // FAULT_ATTR,      // Attribute Name
													                   NULL,            // _getCwmpVersionSupported(), // Attribute Value
															               NULL);           // New Node Value
	  
	  // Add the 'FaultCode' tag and update its content

	  snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", nRPCRet );

    xmlAddNode(SoapMsg.pParser,     // XML Document
               pRefFault,           // XML Parent Node
						   FAULTCODE2,          // New Node TAG
						   NULL,                // Attribute Name
							NULL,                // Attribute Value
							(char*)pTmpBuffer);  // New Node Value

    xmlAddNode(SoapMsg.pParser,                  // XML Document
               pRefFault,                        // XML Parent Node
               FAULTSTRING2,                     // New Node TAG
               NULL,                             // Attribute Name
               NULL,                             // Attribute Value
	            DM_ENG_getFaultString(nRPCRet) ); // New Node Value
	  
	  // ---------------------------------------------------------------------------
	  // Count how many parameter there is in the parameter fault list
	  // ---------------------------------------------------------------------------
	  nNbParameterFault = DM_ENG_tablen( (void**)pFaults );
	  DBG( "Number of parameter found in the list: %d ", nNbParameterFault );
	  
	  // ---------------------------------------------------------------------------	
	  // Add each parameter to the list
	  // ---------------------------------------------------------------------------
	  // find the SetParameterValueFault tag
	  // ---------------------------------------------------------------------------
	  for ( nI=0 ; nI<nNbParameterFault ; nI++ ) {
      // Add SETPARAMETERVALUEFAULT Element
      GenericXmlNodePtr pRefParameterValueFault = xmlAddNode(SoapMsg.pParser,         // XML Document
                                                             pRefFault,               // XML Parent Node
						                                                 SETPARAMETERVALUEFAULT,  // New Node TAG
										                                         NULL,                    // Attribute Name
													                                   NULL,                    // Attribute Value
															                               NULL);                   // New Node Value
								 
      // Add a Parameter Name
      xmlAddNode(SoapMsg.pParser,                    // XML Document
                 pRefParameterValueFault,            // XML Parent Node
                 PARAM_PARAMETERNAME,                // New Node TAG
                 NULL,                               // Attribute Name
                 NULL,                               // Attribute Value
                 (char*)pFaults[nI]->parameterName); // New Node Value
				    
    
      // Add a FaultCode
      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)pFaults[nI]->faultCode );
      xmlAddNode(SoapMsg.pParser,          // XML Document
                 pRefParameterValueFault,  // XML Parent Node
                 FAULTCODE,                // New Node TAG
                 NULL,                     // Attribute Name
                 NULL,                     // Attribute Value
                 pTmpBuffer);              // New Node Value
				      
      // Add a FaultString
      xmlAddNode(SoapMsg.pParser,                  // XML Document
                 pRefParameterValueFault,          // XML Parent Node
                 FAULTSTRING,                      // New Node TAG
                 NULL,                             // Attribute Name
                 NULL,                             // Attribute Value
                 (char*)pFaults[nI]->faultString); // New Node Value
	  } // end for
	  // ---------------------------------------------------------------------------
	  // Remake the XML buffer
	  // ---------------------------------------------------------------------------
	  if ( SoapMsg.pRoot != NULL ) {
      DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
	  }
	  // ---------------------------------------------------------------------------
	  // Then send it to DM_SendHttpMessage
	  // ---------------------------------------------------------------------------
	  if ( g_pBufferCpe != NULL )  {
    // Send the HTTP message
    if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
	    DBG( "SetParameterValues - Sending http message : OK" );
    } else {
	    EXEC_ERROR( "SetParameterValues - Sending http message : NOK" );
    }
    
    // Free the buffer
    DM_FreeTMPXMLBufferCpe();
    
    nMainRet = DM_OK;
	  }
	  
	  // ---------------------------------------------------------------------------
	  // Free the array given by the DM_Engine
	  // ---------------------------------------------------------------------------
	  DM_ENG_deleteTabSetParameterValuesFault( pFaults );
  }
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	// Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL; 
	}
	
	return( nMainRet );
} /* DM_SUB_SetParameterValues */

/**
 * @brief Call the DM_ENG_GetParameterValues function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterName	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterValues(
  IN const char *pSoapId,
	IN const char **pParameterName)
{
	int 	                 nRPCRet	  = DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET	                 nMainRet	  = DM_ERR;
	DM_ENG_ParameterValueStruct	** pResult	  = NULL;
	int                      nI        = 0;
	int	                   nNbParameterValueStruct	= 1;
	char    	             pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml             SoapMsg;
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameters
	if ( (pSoapId != NULL) && (pParameterName != NULL) )
	{
  // Initialize the SOAP data structure
  DM_InitSoapMsgReceived( &SoapMsg );
  
  // Launching the RPC method from the DM_ENGINE
  DBG( " GetParameterValues( DM_ENG_EntityType_ACS, pParameterName ) " );
  nRPCRet = DM_ENG_GetParameterValues( DM_ENG_EntityType_ACS,
                                       (char**)pParameterName,
                                       &pResult );
  
  // Check the RPC's return code
  if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult != NULL) ){
	  DBG( "GetParameterValues : OK " );
	  DBG( "Creating the SOAP Response for the ACS server." );
	  // ---------------------------------------------------------------------------
	  // Create the XML/SOAP response
	  // ---------------------------------------------------------------------------
    DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, true );
	  
	  // ---------------------------------------------------------------------------
	  // Update the SOAP HEADER
	  // ---------------------------------------------------------------------------
	  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
	  // ---------------------------------------------------------------------------
	  // Add a SetParameterValueResponse tag
	  // ---------------------------------------------------------------------------
	  GenericXmlNodePtr pRefGetParameterValueResponse = xmlAddNode(SoapMsg.pParser,              // XML Document
                                                                 SoapMsg.pBody,                // XML Parent Node
                                                                 GETPARAMETERVALUESRESPONSE,   // Node Name
								                                                 NULL,                         // Attribute Name
														                                     NULL,                         // Attribute Value
																		                             NULL);                        // Node Value
																					     															     
																					     
																					     
	  // ---------------------------------------------------------------------
	  // Add a parameterList tag and sub-tag ParameterValueStruct
	  // ---------------------------------------------------------------------------
	  GenericXmlNodePtr pRefParamListTag = xmlAddNode(SoapMsg.pParser,              // XML Document
                                                    pRefGetParameterValueResponse,// XML Parent Node
                                                    PARAMETERINFOSTRUCT_LIST,     // Node Name
								                                    NULL,                         // Attribute Name
														                        NULL,                         // Attribute Value
																		                NULL);                        // Node Value	  
	  // ---------------------------------------------------------------------------
	  // Count how many Eventstruct there is in the list
	  // and update the attribute of the ParameterList tag
	  // ---------------------------------------------------------------------------
    nNbParameterValueStruct = DM_ENG_tablen( (void**)pResult );
 
 	  DBG( "Parameters found in the ParameterList : %d ", nNbParameterValueStruct );
 	  if ( nNbParameterValueStruct<10 ) {
     snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTR_VAL1, nNbParameterValueStruct );
 	  } else {
     snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTR_VAL2, nNbParameterValueStruct );
 	  }
 	  // --> is there some case where nNbParameterValueStruct may be bigger than 99 ?
 	  //     if so may we update the mask from XX to XXX
 	  DBG("ADD Node Attribut %s, %s", DM_COM_ArrayTypeAttrName, pTmpBuffer);
 	  xmlAddNodeAttribut(pRefParamListTag, DM_COM_ArrayTypeAttrName, pTmpBuffer);  
	  
	  // ---------------------------------------------------------------------------
	  // Add the ParameterValueStruct subtags into the ParameterList one
	  // ---------------------------------------------------------------------------
	  GenericXmlNodePtr pRefParamStruct       = NULL;
	  //GenericXmlNodePtr currentChildNameNode  = NULL;
	  GenericXmlNodePtr currentChildValueNode = NULL;
	  
	  for ( nI=0 ; nI<nNbParameterValueStruct ; nI++ )
	  {
    DBG( "Param.%d/%d : %s = '%s' (type = %d)", nI, nNbParameterValueStruct,
	            (char*)pResult[nI]->parameterName,
	            (char*)pResult[nI]->value,
	            pResult[nI]->type );

	    pRefParamStruct = xmlAddNode(SoapMsg.pParser, pRefParamListTag, INFORM_PARAMETERVALUESTRUCT, NULL, NULL, NULL);

      /*currentChildNameNode =*/ xmlAddNode(SoapMsg.pParser, pRefParamStruct, INFORM_NAME, NULL, NULL, (char*)pResult[nI]->parameterName);    
                    
	    // ---------------------------------------------------------------------------
	    // If the parameter is not affected to a value, the DM_ENGINE will replied NULL
	    // If so return an empty string to the ACS
	    // ---------------------------------------------------------------------------
	    if(pResult[nI]->value != NULL) {
		     currentChildValueNode = xmlAddNode(SoapMsg.pParser, pRefParamStruct, INFORM_VALUE, NULL, NULL, (char*)pResult[nI]->value);   
	    } else {
         currentChildValueNode = xmlAddNode(SoapMsg.pParser, pRefParamStruct, INFORM_VALUE, NULL, NULL, _EMPTY);   
	    }
	    
	    // ---------------------------------------------------------------------------
	    // Update the kind of type to the value content
	    // ---------------------------------------------------------------------------
	    switch ( pResult[nI]->type )
	    {
      case DM_ENG_ParameterType_INT:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_INT );         break;
      case DM_ENG_ParameterType_UINT:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_UNSIGNEDINT ); break;
      case DM_ENG_ParameterType_LONG:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_LONG );        break;
      case DM_ENG_ParameterType_BOOLEAN:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_BOOLEAN );     break;
      case DM_ENG_ParameterType_DATE:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_DATETIME );    break;
      case DM_ENG_ParameterType_STRING:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_STRING );      break;
      case DM_ENG_ParameterType_STATISTICS:
      case DM_ENG_ParameterType_ANY:
	      snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%s", XSD_ANY );         break;
      default:
      {
	      EXEC_ERROR( "Unknown type value for a parameter (%d) ",
	             (int)pResult[nI]->type );
      }
      break;
	    }
      xmlAddNodeAttribut(currentChildValueNode, INFORM_VALUE_ATTR, pTmpBuffer);
	  }
	  
	  // ---------------------------------------------------------------------------
	  // Remake the XML buffer then send it to the ACS server
	  // ---------------------------------------------------------------------------
	  if ( SoapMsg.pRoot != NULL ){
    DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
    if ( g_pBufferCpe != NULL )
    {
	    
	    // Send the HTTP message
	    if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
        DBG( "GetParameterValues - Sending http message : OK" );
	    } else {
        EXEC_ERROR( "GetParameterValues - Sending http message : NOK" );
	    }
	    
	    // Free the buffer
	    DM_FreeTMPXMLBufferCpe();
	    
	    nMainRet = DM_OK;
    }
	  }
	  
	  // ---------------------------------------------------------------------------
	  // Free the array given by the DM_Engine
	  // ---------------------------------------------------------------------------
	  DM_ENG_deleteTabParameterValueStruct( pResult );
	  
	  nMainRet = DM_OK;
  } else {
	  EXEC_ERROR( "GetParameterValues : NOK (%d) ", nRPCRet );

	  // Send a fault SOAP message to the ACS server (NOK)
	  DM_SoapFaultResponse( pSoapId, nRPCRet );
  }
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}

	// Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);   
	  SoapMsg.pParser = NULL;
	}

	return( nMainRet );
} /* DM_SUB_GetParameterValues */

/**
 * @brief Call the DM_ENG_SetParameterAttributes
 *
 * @param pSoapId	ID of the SOAP request
 * @param pResult	List 
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_SetParameterAttributes(
  IN const char                             * pSoapId,
	IN       DM_ENG_ParameterAttributesStruct * pParameterList[])
{
  DMRET returnCode = DM_OK;
  int   rc         = 0;
	
	DBG("DM_SUB_SetParameterAttributes - Begin");
	
  if((NULL == pSoapId) || (NULL == pParameterList)) {
  
    EXEC_ERROR( "Invalid pointer" );
    returnCode = DM_ERR;
    DM_SoapFaultResponse( pSoapId, DM_ENG_INVALID_ARGUMENTS );
  }	else {
    // Call the DM Engine to perform the request
    rc = DM_ENG_SetParameterAttributes(DM_ENG_EntityType_ACS, pParameterList);

    if(0 != rc) {
      // Error while seting parameters attributs.
      EXEC_ERROR("Error while seting parameters attributs.");
      returnCode = DM_ERR;
      DM_SoapFaultResponse( pSoapId, rc );
    } else {
      // Send a Soap response
		  DM_SoapEmptyResponse( pSoapId, SETPARAMETERATTRIBUTESRESPONSE );
    }
  }
  
  DBG("DM_SUB_SetParameterAttributes - End");
  
	return( returnCode );
} /* DM_SUB_SetParameterAttributes */

/**
 * @brief Call the DM_ENG_GetParameterAttributes function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterName	List of parameters
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterAttributes(IN const char  * pSoapId,
	                            IN const char ** pParameterName)
{
   DM_SoapXml                           SoapMsg;
   DMRET                                nMainRet  = DM_ERR;
   DM_ENG_ParameterAttributesStruct ** pResult   = NULL;
   int                                  nRPCRet  = DM_ENG_METHOD_NOT_SUPPORTED;
   int                                    nNbParameterValueStruct = 0;  
   int                                  nNbAccessListString      = 0;
   int                                  loopAccesList            = 0;
   int                                  nI = 0;
   char                                  pTmpBuffer[TMPBUFFER_SIZE];
   
         
   // Memset to 0x00 the SoapMsg
   memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
   
  if((NULL != pSoapId) || (NULL != pParameterName)) {
    
    // Initialize the SOAP data structure
    DM_InitSoapMsgReceived( &SoapMsg );    
    
    // Launching the RPC method from the DM_ENGINE
    DBG( " GetParameterValues( DM_ENG_EntityType_ACS, pParameterName ) " );
    nRPCRet = DM_ENG_GetParameterAttributes( DM_ENG_EntityType_ACS,
                                              (char**)pParameterName,
                                              &pResult );    

    // Check the RPC's return code
    if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult != NULL) ){  
      DBG( "GetParameterAttributes : OK " );
      
      DBG( "Creating the SOAP Response for the ACS server." );
      
       // ---------------------------------------------------------------------------
       // Create the XML/SOAP response
       // ---------------------------------------------------------------------------
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );	 
      
       // ---------------------------------------------------------------------------
       // Update the SOAP HEADER
       // ---------------------------------------------------------------------------
       DM_AddSoapHeaderID( pSoapId, &SoapMsg );      
      
       // ---------------------------------------------------------------------------
       // Add a SetParameterValueResponse tag
       // ---------------------------------------------------------------------------
       GenericXmlNodePtr pRefGetParameterAttributesResponse = xmlAddNode(SoapMsg.pParser,               // XML Document
                                                                        SoapMsg.pBody,                  // XML Parent Node
                                                                        GETPARAMETERATTRIBUTESRESPONSE, // Node Name
                                                                        NULL,                           // Attribute Name
                                                                        NULL,                           // Attribute Value
                                                                        NULL);                          // Node Value
                                                                                                                    
                                                                    
        // ---------------------------------------------------------------------
       // Add a parameterList tag and sub-tag ParameterValueStruct
       // ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefParamListTag = xmlAddNode(SoapMsg.pParser,                   // XML Document
                                                      pRefGetParameterAttributesResponse,// XML Parent Node
                                                      PARAMETERINFOSTRUCT_LIST,          // Node Name
                                                      NULL,                              // Attribute Name
                                                      NULL,                              // Attribute Value
                                                      NULL);                             // Node Value                     
       
      // ---------------------------------------------------------------------------
      // Count how many Eventstruct there is in the list
      // and update the attribute of the ParameterList tag
      // ---------------------------------------------------------------------------
      nNbParameterValueStruct = DM_ENG_tablen( (void**)pResult );
 
      DBG( "Parameters found in the ParameterList : %d ", nNbParameterValueStruct );
      if ( nNbParameterValueStruct<10 ) {
        snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTRIBUTE_VAL1, nNbParameterValueStruct );
      } else {
        snprintf( pTmpBuffer, TMPBUFFER_SIZE, INFORM_PARAMETERLIST_ATTRIBUTE_VAL2, nNbParameterValueStruct );
      }

      DBG("ADD Node Attribut %s, %s", DM_COM_ArrayTypeAttrName, pTmpBuffer);
      xmlAddNodeAttribut(pRefParamListTag, DM_COM_ArrayTypeAttrName, pTmpBuffer);        
      
      // ---------------------------------------------------------------------------
      // Add the ParameterValueStruct subtags into the ParameterList one
      // ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefParamStruct              = NULL;
      //GenericXmlNodePtr currentChildNameNode         = NULL;
      //GenericXmlNodePtr currentChildNotificationNode = NULL;
      GenericXmlNodePtr currentChildAccessListNode   = NULL;   
      char              tmp[5];
      
      for ( nI=0 ; nI<nNbParameterValueStruct ; nI++ )
      {
        DBG( "Param.%d/%d : %s, Notification: %d, AccessList[0]: %s", nI,
                                                                  nNbParameterValueStruct,
                                                                         (char*)pResult[nI]->parameterName,
                                                                               pResult[nI]->notification,
                                                                                   ((NULL == pResult[nI]->accessList) ? "NONE" : (pResult[nI]->accessList)[0]));

         pRefParamStruct = xmlAddNode(SoapMsg.pParser, pRefParamListTag, PARAMETERATTRIBUTESTRUCT, NULL, NULL, NULL);
        
         // Add the Name Node
        /*currentChildNameNode =*/ xmlAddNode(SoapMsg.pParser,
                                           pRefParamStruct,
                                                PARAMETERNAME,
                                                   NULL,
                                                      NULL,
                                                        (char*)pResult[nI]->parameterName);        

         // Add the Notification Node
        sprintf(tmp, "%d", pResult[nI]->notification);
        /*currentChildNotificationNode =*/ xmlAddNode(SoapMsg.pParser,
                                                   pRefParamStruct,
                                                         PARAM_NOTIFICATION,
                                                             NULL,
                                                                NULL,
                                                                  (char*)tmp); 

        // Add the AccesList Node(s)   
        currentChildAccessListNode = xmlAddNode(SoapMsg.pParser,
                                                 pRefParamStruct,
                                                       PARAM_ACCESSLIST,
                                                           NULL,
                                                              NULL,
                                                                NULL);

        // ---------------------------------------------------------------------------
        // Count how many AccessList there is in the list
        // ---------------------------------------------------------------------------
        nNbAccessListString = DM_ENG_tablen( (void**)pResult[nI]->accessList );

        if ( nNbAccessListString < 10 ) {
          snprintf( pTmpBuffer, TMPBUFFER_SIZE, STRING_LIST_ATTR_VAL1, nNbAccessListString );
        } else {
          snprintf( pTmpBuffer, TMPBUFFER_SIZE, STRING_LIST_ATTR_VAL2, nNbAccessListString );
        }         
   
        xmlAddNodeAttribut(currentChildAccessListNode, DM_COM_ArrayTypeAttrName, pTmpBuffer);
      
           for(loopAccesList = 0; loopAccesList < nNbAccessListString; loopAccesList++) {
            xmlAddNode(SoapMsg.pParser,
                     currentChildAccessListNode,
                     PARAM_STRING,
                     NULL,
                     NULL,
                     ((NULL == pResult[nI]->accessList[loopAccesList]) ? _EMPTY : pResult[nI]->accessList[loopAccesList]));
           } // end for AccesList
      } // end for ParameterValueStruct

      // ---------------------------------------------------------------------------
      // Remake the XML buffer then send it to the ACS server
      // ---------------------------------------------------------------------------
      if ( SoapMsg.pRoot != NULL ){
        DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
        if ( g_pBufferCpe != NULL ) {
       
          // Send the HTTP message
          if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
            DBG( "GetParameterAttributes - Sending http message : OK" );
          } else {
            EXEC_ERROR( "GetParameterAttributes - Sending http message : NOK" );
          }
       
        // Free the buffer
        DM_FreeTMPXMLBufferCpe();
        nMainRet = DM_OK;
      }
     }
     
     // ---------------------------------------------------------------------------
     // Free the array given by the DM_Engine
     // ---------------------------------------------------------------------------
     DM_ENG_deleteTabParameterAttributesStruct( pResult );
     
     nMainRet = DM_OK;      
      
     } else { //  end if Check the RPC's return code
       // Send an error response message
       DM_SoapFaultResponse( pSoapId, nRPCRet );
     }
       
  }   else {
    // Invalid Parameters
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
  }

   // Free xml document
   if(NULL != SoapMsg.pParser) {
     xmlDocumentFree(SoapMsg.pParser);   
     SoapMsg.pParser = NULL;
   }

   return( nMainRet );
} /* DM_SUB_GetParameterAttributes */

/**
 * @brief Call the DM_ENG_AddObject function
 *
 * @param pSoapId	 ID of the SOAP request
 * @param pObjectName	 Name og the object to create
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_AddObject(IN const char *pSoapId,
   IN const char *pObjectName,
   IN const char *pParameterKey)
{
	DMRET		               nMainRet		      = DM_ERR;
	int 		               nRPCRet		      = DM_ENG_METHOD_NOT_SUPPORTED;
	unsigned int	         nInstanceNumber	= 0;
	DM_ENG_ParameterStatus Status		        = DM_ENG_ParameterStatus_UNDEFINED;
	char		               pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml	           SoapMsg;
	
	// Check parameters
	if ( (pSoapId !=NULL) && (pObjectName != NULL) && (pParameterKey != NULL) )	{
		// ---------------------------------------------------------------------------
		// Initialize the SOAP data structure
		// ---------------------------------------------------------------------------
		DM_InitSoapMsgReceived( &SoapMsg );
		
		// ---------------------------------------------------------------------------
		// Launching the RPC method from the DM_ENGINE
		// ---------------------------------------------------------------------------
		DBG( " AddObject( EntityType_ACS, pObjectName, pParameterKey, &InstanceNumber, &Status ) " );
		nRPCRet = DM_ENG_AddObject( DM_ENG_EntityType_ACS,
                               (char*)pObjectName,
                               (char*)pParameterKey,
                               &nInstanceNumber,
                               &Status);
		
		// ---------------------------------------------------------------------------
		// Check the RPC's return code
		// ---------------------------------------------------------------------------
		if ( nRPCRet == RPC_CPE_RETURN_CODE_OK ) {
			DBG( "AddObject : OK " );
			DBG( "Creating the response for the ACS server." );
			
			// ---------------------------------------------------------------------------
			// Create the XML/SOAP response
			// ---------------------------------------------------------------------------
         DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

			// ---------------------------------------------------------------------------
			// Update the SOAP HEADER
			// ---------------------------------------------------------------------------
			DM_AddSoapHeaderID( pSoapId, &SoapMsg );
			
			// ---------------------------------------------------------------------------
			// Update the SOAP BODY - Add the AddObject tag
			// ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefaddObject = xmlAddNode(SoapMsg.pParser,     // XML Document
                                                   SoapMsg.pBody,       // XML Parent Node
                                                   ADDOBJECTRESPONSE,   // Node Name
							                                     NULL,                // Attribut Name
												                           NULL,                // Attribut Value
															                     NULL);               // Node Value	
			
			// ---------------------------------------------------------------------------
			// Add the Instance number
			// ---------------------------------------------------------------------------
			DBG( " Instance = %d ", nInstanceNumber );
			snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", nInstanceNumber );
      xmlAddNode(SoapMsg.pParser,            // XML Document
                 pRefaddObject,              // XML Parent Node
                 ADDOBJECT_INSTANCENUMBER,   // Node Name
			           NULL,                       // Attribut Name
			           NULL,                       // Attribut Value
			           pTmpBuffer);                // Node Value      
      
			
			// ---------------------------------------------------------------------------
			// Add the status
			// ---------------------------------------------------------------------------
			snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)Status );
      xmlAddNode(SoapMsg.pParser,    // XML Document
                 pRefaddObject,      // XML Parent Node
                 ADDOBJECT_STATUS,   // Node Name
			           NULL,               // Attribut Name
			           NULL,               // Attribut Value
			           pTmpBuffer);        // Node Value  		
				   	
			// ---------------------------------------------------------------------------
			// Remake the XML buffer then send it to the ACS server
			// ---------------------------------------------------------------------------
			if ( SoapMsg.pRoot != NULL ) {
				DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
				if ( g_pBufferCpe != NULL )	{
					// Send the message using libCurl
					if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
						DBG( "Sending http message : OK" );
					} else {
					  EXEC_ERROR( "Sending http message : NOK" );
					}
				}
			}
			
			nMainRet = DM_OK;
		} else {
		  EXEC_ERROR( "AddObject : NOK (%d) ", nRPCRet );
			
			// Send a fault SOAP message to the ACS server (NOK)
			DM_SoapFaultResponse( pSoapId, nRPCRet );
		}
	} else {
	  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nMainRet );


} /* DM_SUB_AddObject */

/**
 * @brief Call the DM_ENG_DeleteObject function
 *
 * @param pSoapId	 ID of the SOAP request
 * @param pObjectName	 Name of the object to delete
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_DeleteObject(IN const char *pSoapId,
      IN const char *pObjectName,
      IN const char *pParameterKey)
{
	DMRET			                nMainRet	      = DM_ERR;
	int 			                nRPCRet		      = DM_ENG_METHOD_NOT_SUPPORTED;
	char			                pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml		            SoapMsg;
	DM_ENG_ParameterStatus		Status;
	
	// Check parameters
	if ( (pSoapId != NULL) && (pObjectName != NULL) && (pParameterKey != NULL) ){
		// ---------------------------------------------------------------------------
		// Initialize the SOAP data structure
		// ---------------------------------------------------------------------------
		DM_InitSoapMsgReceived( &SoapMsg );
		
		// ---------------------------------------------------------------------------
		// Launching the RPC method from the DM_ENGINE
		// ---------------------------------------------------------------------------
		DBG( " DeleteObject( EntityType_ACS, pObjectName, pParameterKey, &Status ) " );
		nRPCRet = DM_ENG_DeleteObject( DM_ENG_EntityType_ACS,
					                        (char*)pObjectName,
					                        (char*)pParameterKey,
					                        &Status );
		
		// ---------------------------------------------------------------------------
		// Check the RPC's return code
		// ---------------------------------------------------------------------------
		if ( nRPCRet == RPC_CPE_RETURN_CODE_OK ){
			DBG( "DeleteObject : OK " );
			DBG( "Creating the response for the ACS server." );
			
			// ---------------------------------------------------------------------------
			// Create the XML/SOAP response
			// ---------------------------------------------------------------------------
         DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );

			// ---------------------------------------------------------------------------
			// Update the SOAP HEADER
			// ---------------------------------------------------------------------------
			DM_AddSoapHeaderID( pSoapId, &SoapMsg );
			
			// ---------------------------------------------------------------------------
			// Update the SOAP BODY
			// ---------------------------------------------------------------------------
			// Add the DeleteObject tag
			// ---------------------------------------------------------------------------
			// scew_element *pRefDeleteObject = scew_element_add( SoapMsg.pBody, DELETEOBJECTRESPONSE );
      GenericXmlNodePtr pRefDeleteObject = xmlAddNode(SoapMsg.pParser,        // XML Document
                                                      SoapMsg.pBody,          // XML Parent Node
                                                      DELETEOBJECTRESPONSE,   // Node Name
							                                        NULL,                   // Attribut Name
												                              NULL,                   // Attribut Value
																                      NULL);                  // Node Value		
																		      		
			// ---------------------------------------------------------------------------
			// Add the status
			// ---------------------------------------------------------------------------
			snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)Status );
			
      xmlAddNode(SoapMsg.pParser,       // XML Document
                 pRefDeleteObject,      // XML Parent Node
                 DELETEOBJECT_STATUS,   // Node Name
			           NULL,                  // Attribut Name
			           NULL,                  // Attribut Value
			           pTmpBuffer);           // Node Value  			
			
			// ---------------------------------------------------------------------------
			// Remake the XML buffer
			// ---------------------------------------------------------------------------
			if ( SoapMsg.pRoot != NULL ) {
				DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
			}
			
			// ---------------------------------------------------------------------------
			// Then send it to DM_SendHttpMessage
			// ---------------------------------------------------------------------------
			if ( g_pBufferCpe != NULL )	{
				if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
					DBG( "Sending http message : OK" );
				} else {
				  EXEC_ERROR( "Sending http message : NOK" );
				}

			}

			nMainRet = DM_OK;
		} else {
		  EXEC_ERROR( "DeleteObject : NOK (%d) ", nRPCRet );
			
			// Send a fault SOAP message to the ACS server (NOK)
			DM_SoapFaultResponse( pSoapId, nRPCRet );
		}
	} else {
	  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nMainRet );
	
} /* DM_SUB_DeleteObject */

/**
 * @brief Call the DM_ENG_Download function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pFileType  Kind of download according TR069
 * @param pUrl	  URL of the server where we need to get the file
 * @param pUsername  Username for accessing the download server
 * @param pPassword  Password for accessing the download server
 * @param pFileSize  Size of the file to download
 * @param pTargetFileName	Local filename (must be renamed after downloading using this name)
 * @param pDelay  Delay in second for starting the download
 * @param pSuccessURL  URL if the download success
 * @param pFailureURL  URL if the download failed
 * @param pCommandKey  Number which define the current download. 
 *                              It must be used within the download, inform and transfertcomplete
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * Example of DownloadResponse message:
 *
 * <soapenv:Envelope
 *    xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
 *    xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 *    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *    xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soapenv:Header>
 * <cwmp:ID
 *    soapenv:mustUnderstand="1"
 *    xsi:type="xsd:string"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1193677131607</cwmp:ID>
 * </soapenv:Header>
 * <soapenv:Body>
 * <cwmp:DownloadResponse>
 *   <Status>1</Status>
 *   <StartTime>0001-01-01T00:00:00Z</StartTime>
 *   <CompleteTime>0001-01-01T00:00:00Z</CompleteTime>
 * </cwmp:DownloadResponse>
 * </soapenv:Body>
 * </soapenv:Envelope>
 */
DMRET
DM_SUB_Download(IN const char         *pSoapId,
  IN const char         *pFileType,
  IN const char         *pUrl,
  IN const char         *pUsername,
  IN const char         *pPassword,
  IN       unsigned int  nFileSize,
  IN const char         *pTargetFileName,
  IN       unsigned int  nDelay,
  IN const char         *pSuccessURL,
  IN const char         *pFailureURL,
  IN const char         *pCommandKey)
{
	int 	               nRPCRet	      	= DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET	               nMainRet	 	      = DM_ERR;
	DM_ENG_TransferResultStruct * pResult	= NULL;
	char    	           pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml           SoapMsg;
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameters
	if ( (pSoapId     != NULL) &&
	     (pFileType   != NULL) &&
	     (pUrl        != NULL) && 
	     (pCommandKey != NULL) ){
  // ---------------------------------------------------------------------------
  // Initialize the SOAP data structure
  // ---------------------------------------------------------------------------
  DM_InitSoapMsgReceived( &SoapMsg );
  
  // ---------------------------------------------------------------------------
  // Launching the RPC method from the DM_ENGINE
  // ---------------------------------------------------------------------------
  DBG("------ DOWNLOAD REQUEST ------");
  DBG("FileType:           %s", (char*)pFileType);
  DBG("Url:                %s", (char*)pUrl);
  DBG("CommandKey:         %s", (char*)pCommandKey);
  DBG("nDelay:             %d",        nDelay);
  DBG("FileSize:           %d",        nFileSize);
  if(NULL != pUsername) {
    DBG("Username:           %s", (char*)pUsername);
  } else {
    DBG("Username:           None");
  }
  if(NULL != pPassword) {
    DBG("Password:           %s", (char*)pPassword);
  } else {
    DBG("Password:           None");
  }
  if(NULL != pTargetFileName) {
    DBG("TargetFileName:     %s", (char*)pTargetFileName);
  } else {
    DBG("TargetFileName:     None");
  }
  if(NULL != pSuccessURL) {
    DBG("SuccessURL:         %s", (char*)pSuccessURL);
  } else {
    DBG("SuccessURL:         None");
  }
  if(NULL != pFailureURL) {
    DBG("FailureURL:         %s", (char*)pFailureURL);
  } else {
    DBG("FailureURL:         None");
  }   
  			   
  nRPCRet = DM_ENG_Download( DM_ENG_EntityType_ACS,
                            (char*)pFileType,
                            (char*)pUrl,
                            (char*)pUsername,
                            (char*)pPassword,
                            (unsigned int)nFileSize,
                            (char*)pTargetFileName,
                            (unsigned int)nDelay,
                            (char*)pSuccessURL,
                            (char*)pFailureURL,
                            (char*)pCommandKey,
                            &pResult );
  
  // ---------------------------------------------------------------------------
  // Check the RPC's return code
  // ---------------------------------------------------------------------------
  if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult != NULL) ){
	  DBG( "Download : OK " );
	  DBG( "Creating the response for the ACS server." );
	  
	  // ---------------------------------------------------------------------------
	  // Create the XML/SOAP response
	  // ---------------------------------------------------------------------------
    DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );
	  
	  // ---------------------------------------------------------------------------
	  // Update the SOAP HEADER
	  // ---------------------------------------------------------------------------
	  DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
	  // ---------------------------------------------------------------------------
	  // Update the SOAP BODY and add the DownloadResponse tag
	  // ---------------------------------------------------------------------------
    GenericXmlNodePtr pRefDownloadResponse = xmlAddNode(SoapMsg.pParser,     // XML Document
                                                        SoapMsg.pBody,       // XML Parent Node
                                                        DOWNLOADRESPONSE,    // Node Name
							                                          NULL,                // Attribut Name
												                                NULL,                // Attribut Value
																                        NULL);               // Node Value
	  
	  // ---------------------------------------------------------------------------
	  // Add the status
	  // ---------------------------------------------------------------------------
	  DBG( " Status of the download = %d ", (int)pResult->status );
	  snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)pResult->status );

      xmlAddNode(SoapMsg.pParser,            // XML Document
                 pRefDownloadResponse,       // XML Parent Node
                 DOWNLOADRESPONSE_STATUS,    // Node Name
			           NULL,                       // Attribut Name
			           NULL,                       // Attribut Value
			           pTmpBuffer);                // Node Value
	  
	  // ---------------------------------------------------------------------------
	  // Add the start time if defined
	  // ---------------------------------------------------------------------------
	  if ( (int)pResult->status == 0 ) {
      char* sTime = DM_ENG_dateTimeToString(pResult->startTime);
      DBG( "Start time = %s (WELL DEFINED) ", pTmpBuffer );
      xmlAddNode(SoapMsg.pParser,            // XML Document
                 pRefDownloadResponse,       // XML Parent Node
                 DOWNLOADRESPONSE_STARTTIME, // Node Name
			           NULL,                    // Attribut Name
			           NULL,                    // Attribut Value
			           sTime);                  // Node Value
      free(sTime);

	  } else {
      DBG( "Start time (UNDEFINED) " );
      xmlAddNode(SoapMsg.pParser,            // XML Document
                 pRefDownloadResponse,       // XML Parent Node
                 DOWNLOADRESPONSE_STARTTIME, // Node Name
			           NULL,                       // Attribut Name
			           NULL,                       // Attribut Value
			           UNDEFINED_UTC_DATETIME);    // Node Value
	  }

	  // ---------------------------------------------------------------------------
	  // Add the Complete time if defined
	  // ---------------------------------------------------------------------------
	  if ( (int)pResult->status == 0 ) {
      char* sTime = DM_ENG_dateTimeToString(pResult->completeTime);
      DBG( "Complete time = %s (WELL DEFINED) ", pTmpBuffer );
      xmlAddNode(SoapMsg.pParser,               // XML Document
                 pRefDownloadResponse,          // XML Parent Node
                 DOWNLOADRESPONSE_COMPLETETIME, // Node Name
			           NULL,                       // Attribut Name
			           NULL,                       // Attribut Value
			           sTime);                     // Node Value  
      free(sTime);

	  } else {
      DBG( "Complete time (UNDEFINED) " );
      xmlAddNode(SoapMsg.pParser,               // XML Document
                 pRefDownloadResponse,          // XML Parent Node
                 DOWNLOADRESPONSE_COMPLETETIME, // Node Name
			           NULL,                          // Attribut Name
			           NULL,                          // Attribut Value
			           UNDEFINED_UTC_DATETIME);       // Node Value  
	  }
	  
	  // ---------------------------------------------------------------------------
	  // Remake the XML buffer then send it to DM_SendHttpMessage
	  // ---------------------------------------------------------------------------
	  if ( SoapMsg.pRoot != NULL )
	  {
    DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
    if ( g_pBufferCpe != NULL )
    {
      // Send the HTTP message
	    if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
      DBG( "Download - Sending http message : OK" );
	    } else {
      EXEC_ERROR( "Download - Sending http message : NOK" );
	    }

    }
	  }
	  
	  // ---------------------------------------------------------------------------
	  // Free the array given by the DM_Engine
	  // ---------------------------------------------------------------------------
	  DM_ENG_deleteTransferResultStruct( pResult );
	  
	  nMainRet = DM_OK;
  } else if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult == NULL) ){
      WARN("Download OK but pResult is NULL");
      DM_SoapFaultResponse( pSoapId, DM_ENG_INTERNAL_ERROR );
  } else {
	  EXEC_ERROR( "Download : NOK (%d) ", nRPCRet );
	  
	  // Send a fault SOAP message to the ACS server (NOK)
	  DM_SoapFaultResponse( pSoapId, nRPCRet );
  }
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
  // Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL;
	}
	
	return( nMainRet );
} /* DM_SUB_Download */

/**
 * @brief Call the DM_SUB_Upload function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pFileType  Kind of download according TR069
 * @param pUrl	  URL of the server where we need to get the file
 * @param pUsername  Username for accessing the download server
 * @param pPassword  Password for accessing the download server
 * @param pFileSize  Size of the file to download
 * @param pTargetFileName	Local filename (must be renamed after downloading using this name)
 * @param pDelay  Delay in second for starting the download
 * @param pSuccessURL  URL if the download success
 * @param pFailureURL  URL if the download failed
 * @param pCommandKey  Number which define the current download. 
 *                              It must be used within the download, inform and transfertcomplete
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * Example of IpResponse message:
 *
 * <soapenv:Envelope
 *    xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
 *    xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0"
 *    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
 *    xmlns:xsd="http://www.w3.org/2001/XMLSchema">
 * <soapenv:Header>
 * <cwmp:ID
 *    soapenv:mustUnderstand="1"
 *    xsi:type="xsd:string"
 *    xmlns:cwmp="urn:dslforum-org:cwmp-1-0">1193677131607</cwmp:ID>
 * </soapenv:Header>
 * <soapenv:Body>
 * <cwmp:UploadResponse>
 *   <Status>1</Status>
 *   <StartTime>0001-01-01T00:00:00Z</StartTime>
 *   <CompleteTime>0001-01-01T00:00:00Z</CompleteTime>
 * </cwmp:UploadResponse>
 * </soapenv:Body>
 * </soapenv:Envelope>
 */
DMRET
DM_SUB_Upload(IN const char   *pSoapId,
              IN const char   *pFileType,
              IN const char   *pUrl,
              IN const char   *pUsername,
              IN const char   *pPassword,
              IN unsigned int  nDelay,
              IN const char   *pCommandKey)
{
	int 	               nRPCRet	      	= DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET	               nMainRet	 	      = DM_ERR;
	DM_ENG_TransferResultStruct * pResult	= NULL;
	char    	           pTmpBuffer[TMPBUFFER_SIZE];
	DM_SoapXml           SoapMsg;
	
	memset((void *) &SoapMsg, 0x00, sizeof(SoapMsg));
	
	// Check parameters
	if ( (pSoapId     != NULL) &&
	     (pFileType   != NULL) &&
	     (pUrl        != NULL) && 
	     (pCommandKey != NULL) ){
    // ---------------------------------------------------------------------------
    // Initialize the SOAP data structure
    // ---------------------------------------------------------------------------
    DM_InitSoapMsgReceived( &SoapMsg );
  
    // ---------------------------------------------------------------------------
    // Launching the RPC method from the DM_ENGINE
    // ---------------------------------------------------------------------------
    DBG("------ UPLOAD REQUEST ------");
    DBG("FileType:           %s", (char*)pFileType);
    DBG("Url:                %s", (char*)pUrl);
    DBG("Username            %s", ((NULL == pUsername) ? _EMPTY : pUsername));
    DBG("Password            %s", ((NULL == pPassword) ? _EMPTY : pPassword));
    DBG("CommandKey:         %s", (char*)pCommandKey);
    DBG("nDelay:             %d",        nDelay);
  			   
    nRPCRet = DM_ENG_Upload( DM_ENG_EntityType_ACS,
                             (char*)pFileType,
                             (char*)pUrl,
                             (char*)pUsername,
                             (char*)pPassword,
                             (unsigned int)nDelay,
                             (char*)pCommandKey,
                             &pResult );
  
    // ---------------------------------------------------------------------------
    // Check the RPC's return code
    // ---------------------------------------------------------------------------
    if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult != NULL) ){
	    DBG( "Upload : OK " );
	    DBG( "Creating the response for the ACS server." );
	  
	    // ---------------------------------------------------------------------------
	    // Create the XML/SOAP response
	    // ---------------------------------------------------------------------------
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );
	  
	    // ---------------------------------------------------------------------------
	    // Update the SOAP HEADER
	    // ---------------------------------------------------------------------------
	    DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	  
	    // ---------------------------------------------------------------------------
	    // Update the SOAP BODY and add the UploadResponse tag
	    // ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefUploadResponse = xmlAddNode(SoapMsg.pParser, // XML Document
                                                        SoapMsg.pBody,   // XML Parent Node
                                                        UPLOADRESPONSE,  // Node Name
					  		                                        NULL,            // Attribut Name
						     					                              NULL,            // Attribut Value
								  								                      NULL);           // Node Value
	  
	    // ---------------------------------------------------------------------------
	    // Add the status
	    // ---------------------------------------------------------------------------
	    DBG( " Status of the upload = %d ", (int)pResult->status );
	    snprintf( pTmpBuffer, TMPBUFFER_SIZE, "%d", (int)pResult->status );

      xmlAddNode(SoapMsg.pParser,         // XML Document
                 pRefUploadResponse,      // XML Parent Node
                 UPLOADRESPONSE_STATUS,   // Node Name
			           NULL,                    // Attribut Name
			           NULL,                    // Attribut Value
			           pTmpBuffer);             // Node Value
	  
	    // ---------------------------------------------------------------------------
	    // Add the start time if defined
	    // ---------------------------------------------------------------------------
	    if ( (int)pResult->status == 0 ) {
        char* sTime = DM_ENG_dateTimeToString(pResult->startTime);
        DBG( "Start time = %s (WELL DEFINED) ", pTmpBuffer );
        xmlAddNode(SoapMsg.pParser,            // XML Document
                   pRefUploadResponse,         // XML Parent Node
                   UPLOADRESPONSE_STARTTIME,   // Node Name
			             NULL,                       // Attribut Name
			             NULL,                       // Attribut Value
			             sTime);                     // Node Value
        free(sTime);

	    } else {
        DBG( "Start time (UNDEFINED) " );
        xmlAddNode(SoapMsg.pParser,            // XML Document
                   pRefUploadResponse,         // XML Parent Node
                   UPLOADRESPONSE_STARTTIME,   // Node Name
			             NULL,                       // Attribut Name
			             NULL,                       // Attribut Value
			             UNDEFINED_UTC_DATETIME);    // Node Value
	    }

	    // ---------------------------------------------------------------------------
	    // Add the Complete time if defined
	    // ---------------------------------------------------------------------------
	    if ( (int)pResult->status == 0 ) {
        char* sTime = DM_ENG_dateTimeToString(pResult->completeTime);
        DBG( "Complete time = %s (WELL DEFINED) ", pTmpBuffer );
        xmlAddNode(SoapMsg.pParser,             // XML Document
                   pRefUploadResponse,          // XML Parent Node
                   UPLOADRESPONSE_COMPLETETIME, // Node Name
			             NULL,                        // Attribut Name
			             NULL,                        // Attribut Value
			             sTime);                      // Node Value  
        free(sTime);

	    } else {
        DBG( "Complete time (UNDEFINED) " );
        xmlAddNode(SoapMsg.pParser,             // XML Document
                   pRefUploadResponse,          // XML Parent Node
                   UPLOADRESPONSE_COMPLETETIME, // Node Name
			             NULL,                        // Attribut Name
			             NULL,                        // Attribut Value
			             UNDEFINED_UTC_DATETIME);     // Node Value  
	    }
	  
	    // ---------------------------------------------------------------------------
	    // Remake the XML buffer then send it to DM_SendHttpMessage
	    // ---------------------------------------------------------------------------
	    if ( SoapMsg.pRoot != NULL ) {
        DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
        if ( g_pBufferCpe != NULL ) {
          // Send the HTTP message
	        if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
            DBG( "Upload - Sending http message : OK" );
	        } else {
            EXEC_ERROR( "Upload - Sending http message : NOK" );
	        }
        }
	    }
	  
	    // ---------------------------------------------------------------------------
	    // Free the array given by the DM_Engine
	    // ---------------------------------------------------------------------------
	    DM_ENG_deleteTransferResultStruct( pResult );
	  
	    nMainRet = DM_OK;
    } else if ( (nRPCRet == RPC_CPE_RETURN_CODE_OK) && (pResult == NULL) ){
      WARN("Upload OK but pResult is NULL");
      DM_SoapFaultResponse( pSoapId, DM_ENG_INTERNAL_ERROR );
    } else {
	    EXEC_ERROR( "Upload : NOK (%d) ", nRPCRet );  
	    // Send a fault SOAP message to the ACS server (NOK)
	    DM_SoapFaultResponse( pSoapId, nRPCRet );
    }
	} else {
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
  // Free xml document
	if(NULL != SoapMsg.pParser) {
	  xmlDocumentFree(SoapMsg.pParser);
	  SoapMsg.pParser = NULL;
	}
	
	return( nMainRet );
} /* DM_SUB_Upload */


/**
 * @brief Call the DM_ENG_GetAllQueuedTransfers function
 *
 * @param NONE
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET 
DM_SUB_GetAllQueuedTransferts(IN const char *pSoapId)
{
  int rc;
  DM_ENG_AllQueuedTransferStruct  ** pResult = NULL;
  DM_SoapXml           SoapMsg;
	int nNbParameterValueStruct = 0;
	int nI;
	char * tmpStr = NULL;
	   
  DBG("DM_SUB_GetAllQueuedTransferts - Begin");
  
  rc = DM_ENG_GetAllQueuedTransfers(DM_ENG_EntityType_ACS, &pResult);
  
  DBG("DM_ENG_GetAllQueuedTransfers - rc = %d", rc);
  
  if(0 == rc) {
    // Perform the response
    
	    // ---------------------------------------------------------------------------
	    // Create the XML/SOAP response
	    // ---------------------------------------------------------------------------
	    DM_InitSoapMsgReceived(&SoapMsg);
      DM_AnalyseSoapMessage( &SoapMsg, NULL, TYPE_CPE, false );
	  
	    // ---------------------------------------------------------------------------
	    // Update the SOAP HEADER
	    // ---------------------------------------------------------------------------
	    DM_AddSoapHeaderID( pSoapId, &SoapMsg );
	    
	    // ---------------------------------------------------------------------------
	    // Update the SOAP BODY and add the GETALLQUEUEDTRANSFERTSRESPONSE tag
	    // ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefGetAllTransfertsResponse = xmlAddNode(SoapMsg.pParser,                 // XML Document
                                                                  SoapMsg.pBody,                   // XML Parent Node
                                                                  GETALLQUEUEDTRANSFERTSRESPONSE,  // Node Name
					  		                                                  NULL,                            // Attribut Name
						     					                                        NULL,                            // Attribut Value
								  								                                NULL);                           // Node Value		    
	    // ---------------------------------------------------------------------------
	    // Update the SOAP BODY and add the PARAM_TRANSFER_LIST tag
	    // ---------------------------------------------------------------------------
      GenericXmlNodePtr pRefTransferListResponse = xmlAddNode(SoapMsg.pParser,         // XML Document
                                                        pRefGetAllTransfertsResponse,  // XML Parent Node
                                                        PARAM_TRANSFER_LIST,           // Node Name
					  		                                        NULL,                          // Attribut Name
						     					                              NULL,                          // Attribut Value
								  								                      NULL);                         // Node Value
      // ---------------------------------------------------------------------------
      // Count how many Eventstruct there is in the list
      // and update the attribute of the ParameterList tag
      // ---------------------------------------------------------------------------																		      
      nNbParameterValueStruct = DM_ENG_tablen( (void**)pResult );

																		      																		      
      for ( nI=0 ; nI<nNbParameterValueStruct ; nI++ ) {
     
        // Add a response structure AllQueuedTransferStruct
        GenericXmlNodePtr pRefAllQueuedTransferStructResponse = xmlAddNode(SoapMsg.pParser,               // XML Document
                                                                         pRefTransferListResponse,      // XML Parent Node
                                                                         GETALLQUEUEDTRANSFERSTRUCT,    // Node Name
					  		                                                         NULL,                          // Attribut Name
						     					                                               NULL,                          // Attribut Value
								  								                                       NULL);                         // Node Value       
     
         // Add the CommandKey
         xmlAddNode(SoapMsg.pParser,                      // XML Document
                    pRefAllQueuedTransferStructResponse,  // XML Parent Node
                    GETALLQUEUEDTRANSFER_COMMANDKEY,      // Node Name
					  		    NULL,                                 // Attribut Name
						     		NULL,                                 // Attribut Value
								  	pResult[nI]->commandKey);      // Node Value
									
         // Add the state
	       tmpStr = DM_ENG_intToString(pResult[nI]->state);
         xmlAddNode(SoapMsg.pParser,                      // XML Document
                    pRefAllQueuedTransferStructResponse,  // XML Parent Node
                    GETALLQUEUEDTRANSFER_STATE,           // Node Name
					  		    NULL,                                 // Attribut Name
						     		NULL,                                 // Attribut Value
								  	tmpStr); // Node Value
				DM_ENG_FREE(tmpStr);
													       
         // Add the IsDownload
	       tmpStr = DM_ENG_intToString(pResult[nI]->isDownload);
         xmlAddNode(SoapMsg.pParser,                            // XML Document
                    pRefAllQueuedTransferStructResponse,        // XML Parent Node
                    GETALLQUEUEDTRANSFER_ISDOWNLOAD,            // Node Name
					  		    NULL,                                       // Attribut Name
						     		NULL,                                       // Attribut Value
								  	tmpStr);                                    // Node Value
				 DM_ENG_FREE(tmpStr);
					       
         // Add the FileType
         xmlAddNode(SoapMsg.pParser,                         // XML Document
                    pRefAllQueuedTransferStructResponse,     // XML Parent Node
                    GETALLQUEUEDTRANSFER_FILETYPE,           // Node Name
					  		    NULL,                                    // Attribut Name
						     		NULL,                                    // Attribut Value
								  	pResult[nI]->fileType);                  // Node Value
					          
         // Add the FileSize
	       tmpStr = DM_ENG_intToString(pResult[nI]->fileSize);
         xmlAddNode(SoapMsg.pParser,                            // XML Document
                    pRefAllQueuedTransferStructResponse,        // XML Parent Node
                    GETALLQUEUEDTRANSFER_FILESIZE,              // Node Name
					  		    NULL,                                       // Attribut Name
						     		NULL,                                       // Attribut Value
								  	DM_ENG_intToString(pResult[nI]->fileSize)); // Node Value
				 DM_ENG_FREE(tmpStr);
					           
         // Add the TargetFileName (Set an empty string)
         xmlAddNode(SoapMsg.pParser,                         // XML Document
                    pRefAllQueuedTransferStructResponse,     // XML Parent Node
                    GETALLQUEUEDTRANSFER_TARGETFILENAME,     // Node Name
					  		    NULL,                                    // Attribut Name
						     		NULL,                                    // Attribut Value
								  	_EMPTY);                                     // Node Value
		 }	// End for												      
					
																		      
	  // ---------------------------------------------------------------------------
	  // Remake the XML buffer then send it to DM_SendHttpMessage
	  // ---------------------------------------------------------------------------
	    if ( SoapMsg.pRoot != NULL ) {
        DM_RemakeXMLBufferCpe( SoapMsg.pRoot );
        if ( g_pBufferCpe != NULL ) {
          // Send the HTTP message
	        if ( DM_SendHttpMessage( g_pBufferCpe ) == DM_OK ) {
            DBG( "GetAllQueuedTransfer - Sending http message : OK" );
	        } else {
            EXEC_ERROR( "GetAllQueuedTransfer - Sending http message : NOK" );
	        }
        }
	    }																															      
																																												      
																																																									      													      
     DM_ENG_deleteTabAllQueuedTransferStruct(pResult);					      
																		      
																		      
        
  } else {
    // An error occurs
    DM_SoapFaultResponse( pSoapId, rc );
  }
  
  
  
  DBG("DM_SUB_GetAllQueuedTransferts - End");

  return rc;
}


/**
 * @brief Call the DM_ENG_FactoryReset function
 *
 * @param pSoapId ID of the SOAP request
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_FactoryReset(IN const char *pSoapId)
{

	DMRET nMainRet = DM_ERR;
	int   nRPCRet  = DM_ENG_METHOD_NOT_SUPPORTED;
	
	// Check parameter
	if ( pSoapId != NULL ) {
		// ---------------------------------------------------------------------------
		// Launching the RPC method from the DM_ENGINE
		// ---------------------------------------------------------------------------
		DBG( " FactoryReset( EntityType_ACS ) " );
		nRPCRet = DM_ENG_FactoryReset( DM_ENG_EntityType_ACS );
		
		// ---------------------------------------------------------------------------
		// Check the RPC's return code
		// ---------------------------------------------------------------------------
		if ( nRPCRet == RPC_CPE_RETURN_CODE_OK ) {
			DBG( "FactoryReset : OK " );
			
			// Send a SOAP response (OK)
			DM_SoapEmptyResponse( pSoapId, FACTORYRESET_RESPONSE );
		
			nMainRet = DM_OK;
		} else {
		  EXEC_ERROR( "FactoryReset : NOK (%d) ", nRPCRet );
		
			// Send a fault SOAP message to the ACS server (NOK)
			DM_SoapFaultResponse( pSoapId, nRPCRet );
		}
	} else {
	  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nMainRet );


} /* DM_SUB_FactoryReset */

/**
 * @brief Call the DM_ENG_ScheduleInform function
 *
 * @param pSoapId	ID of the SOAP request
 * @param nDelay	Delay in second
 * @param pCommandKey	Number which define the action
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_ScheduleInform(
  IN const char         * pSoapId,
	IN const char         * pDelay,
	IN const char         * pCommandKey)
{
	DMRET nMainRet = DM_ERR;
	int   nRPCRet  = DM_ENG_METHOD_NOT_SUPPORTED;
	
	// Check parameters
	if ( (pSoapId != NULL) && (pDelay != NULL) && (pCommandKey != NULL) )	{
  // ---------------------------------------------------------------------------
  // Launching the RPC method from the DM_ENGINE
  // ---------------------------------------------------------------------------
  DBG( " ScheduleInform( DM_ENG_EntityType_ACS, pDelay, pCommandKey ) " );

  unsigned int delay; 
  if (DM_ENG_stringToUint((char*)pDelay, &delay))
  {
    nRPCRet = DM_ENG_ScheduleInform( DM_ENG_EntityType_ACS, delay, (char*)pCommandKey );
    // nRPCRet = RPC_CPE_RETURN_CODE_OK;
  }
  else
  {
    nRPCRet = DM_ENG_INVALID_ARGUMENTS;
  }
  
  // ---------------------------------------------------------------------------
  // Check the RPC's return code
  // ---------------------------------------------------------------------------
  if ( nRPCRet == RPC_CPE_RETURN_CODE_OK )
  {
	  DBG( "ScheduleInform : OK " );

	  // Send a SOAP response (OK)
	  DM_SoapEmptyResponse( pSoapId, SCHEDULEINFORM_RESPONSE );

	  nMainRet = DM_OK;
  } else {
	  EXEC_ERROR( "ScheduleInform : NOK (%d) ", nRPCRet );

	  // Send a fault SOAP message to the ACS server (NOK)
	  DM_SoapFaultResponse( pSoapId, nRPCRet );
  }
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}

	return( nMainRet );
} /* DM_SUB_ScheduleInform */

/**
 * @brief Call the DM_ENG_Reboot function
 *
 * @param pSoapId	ID of the SOAP request
 * @param pCommandKey	Number which define the current action
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_Reboot(IN const char *pSoapId,
              IN const char *pCommandKey)
{
	int   nRPCRet  = DM_ENG_METHOD_NOT_SUPPORTED;
	DMRET nMainRet = DM_ERR;
	
	// Check parameters
	if ( (pSoapId != NULL) && (pCommandKey != NULL) )	{
    // ---------------------------------------------------------------------------
    // Launching the RPC method from the DM_ENGINE
    // ---------------------------------------------------------------------------
    DBG( " Reboot( DM_ENG_EntityType_ACS, pCommandKey ); " );
    nRPCRet = DM_ENG_Reboot( DM_ENG_EntityType_ACS, (char*)pCommandKey );
  
    // ---------------------------------------------------------------------------
    // Check the RPC's return code
    // ---------------------------------------------------------------------------
    if ( nRPCRet == RPC_CPE_RETURN_CODE_OK ) {
	    DBG( "Reboot : OK " );
	  
	    // Send a SOAP response (OK)
	    DM_SoapEmptyResponse( pSoapId, REBOOT_RESPONSE );
	  
	    nMainRet = DM_OK;
    } else {
	    EXEC_ERROR( "Reboot : NOK (%d) ", nRPCRet );
	  
	    // Send a fault SOAP message to the ACS server
	    DM_SoapFaultResponse( pSoapId, nRPCRet );
    }
	} else {
	  if(pSoapId != NULL) {
	    DM_SoapFaultResponse( pSoapId, DM_ENG_INVALID_ARGUMENTS );
	  }
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nMainRet );
} /* DM_SUB_Reboot */

/**
 * @brief function which generate a Header_ID and save it into the global array
 *
 * @param pID_generated	Pointer to the unique header ID
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_GenerateUniqueHeaderID(OUT char *pID_generated)
{
  static unsigned int cwmpUniqueId = 0;
  
  // Increment the Id
  cwmpUniqueId++;
    
  sprintf(pID_generated, "%d", cwmpUniqueId);

  return DM_OK;
} /* DM_GenerateUniqueHeaderID */

/**
 * @brief Function which remove the SOAP Header_ID from the global array
 *
 * @param pIDToRemove	Header ID to remove from the global array
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_RemoveHeaderIDFromTab(IN const char *pIDToRemove UNUSED) {
  // Return OK
  return DM_OK;
} /* DM_RemoveHeaderIDFromTab */

/**
 * @brief Function which add a Header_ID to a SOAP message
 *
 * @param pUniqueHeaderID Header ID to add to the soap XML message
 * @param pSoapMsg	  Pointer to the XML/SOAP object to update
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_AddSoapHeaderID(
  IN const char * pUniqueHeaderID,
  IN DM_SoapXml * pSoapMsg)
{
  DMRET nMainRet = DM_ERR;
  
  DBG("DM_AddSoapHeaderID pUniqueHeaderID = %s", pUniqueHeaderID);
	
  // -----------------------------------------------------------
  // Check parameters
  // -----------------------------------------------------------
  if ( (pUniqueHeaderID != NULL) && (pSoapMsg != NULL) && (pSoapMsg->pHeader != NULL) ){
	         
    GenericXmlNodePtr nodeHeaderId = xmlAddNode(pSoapMsg->pParser,  // XML Document
                                                pSoapMsg->pHeader,  // parent node
						HEADER_ID,          // new node tag
						NULL,               // Attribut Name
						NULL,               // Attribut Value
						pUniqueHeaderID);   // Node Value

    // ---------------------------------------------------------
    // Add attributes to the ID header element
    // ---------------------------------------------------------
    // ---------------------------------------------------------------
    // Add the MustUnderstarnd=1 attribute to the new inserted Element
    // ---------------------------------------------------------------
	  xmlAddNodeAttribut(nodeHeaderId, _MustUnderstandAttrName, HEADER_ATTR_VAL);   

    // ----------------------------------------------------------------------------------------
    // Add the type kind of the type used for this element (string) to the new inserted Element
    // ----------------------------------------------------------------------------------------
	  // xmlAddNodeAttribut(nodeHeaderId, ATTR_TYPE, XSD_STRING);   

    // --------------------------------------------------------------
    // Add the namespage for this element to the new inserted Element
    // --------------------------------------------------------------
    // xmlAddNodeAttribut(nodeHeaderId, NAMESPACE_ATTR, NAMESPACE_VAL);   

    nMainRet = DM_OK;
  } else {
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
  }
	
  return ( nMainRet );
} /* DM_AddSoapHeaderID */

/**
 * @brief Private _forceACSSessionToClose implementation
 *
 * @param void
 *
 * @Return None
 *
 * Note: This routine is used to close the ACS Session
 *       This routine is used when the normal closing case
 *       not occured.
 *       This routine lock and unlock the mutexAcsSession mutex
 *       to manipulate ACS Session Data.
 */
static void
_forceACSSessionToClose(void){
   WARN("Force ACS Session to Close");
  
   // To free the ACS Session, take the mutex first.
   DM_CMN_Thread_lockMutex(mutexAcsSession);
   _closeACSSession(false);
	DM_CMN_Thread_unlockMutex(mutexAcsSession);
  
}

/**
 * @brief Private _closeACSSession implementation
 *
 * @param success if the session is closed successfully or not
 *
 * @Return None
 *
 * Note: This routine is used to close the ACS Session
 *       This routine is used when the normal closing case occurs or when the timeout happens.
 *       The mutexAcsSession must be locked by the calling routine.
 */
static void
_closeACSSession(bool success){
  INFO("Close ACS Session");
  
  // No need to take the mutex. The mutex
  // is locked by the calling routine.
  g_DmComData.bSession            = false;
  g_DmComData.bSessionOpeningTime = 0;
  
  // Request the HTTP client to close the http session once all message are sent.
  DM_CloseHttpSession(NORMAL_CLOSE);
  
  ignoreHttpNoContentMsgFlag = true;
  
  cwmpSessionClosingFlag = false;
  
  DM_ENG_SessionClosed( DM_ENG_EntityType_ACS, success );
  g_DmComData.bIRTCInProgess = false;

  _freePrefixedTag();
}

/**
 * @brief Private Entry point for ACS Session Supervision Thread
 *
 * @param void
 *
 * @Return None
 *
 * Note: This routine is used to supervise the ACS Session 
 *       whitin a dedicated thread.
 */
void*
DM_COM_ACS_SESSION_SUPERVISOR(void* data UNUSED)
{
  int waitTime = ACSSESSIONSUPERVISOR;
  _AcsSessionSupervisorAlive = true;

  DBG("DM_COM_ACS_SESSION_SUPERVISOR - ACS Supervision thread: START...");

  while(_AcsSessionSupervisorAlive)
  {
  	 DM_CMN_Thread_sleep(waitTime);

    // DBG("DM_COM_ACS_SESSION_SUPERVISOR - Wake up...");
    DM_CMN_Thread_lockMutex(mutexAcsSession);
    
    // Check an ACS Session is opened.
    if (g_DmComData.bSession) {
    
      // Check the ACS Session Timeout is reached
      waitTime = ACSSESSIONTIMEOUT - (int)difftime(time(NULL), g_DmComData.bSessionOpeningTime);
      DBG("DM_COM_ACS_SESSION_SUPERVISOR - ACS Session Created for %d seconds", ACSSESSIONTIMEOUT - waitTime);
      if(waitTime <= 0) {
        // The Session must be closed.
	      WARN("ACS Session created for %d seconds - Close It!!!!", ACSSESSIONTIMEOUT - waitTime);
	      _closeACSSession(false);
         waitTime = ACSSESSIONSUPERVISOR;
	 
   
        // The ACS do not support Cwmp_1_1
        if(false == atLeastOneInformResponseReceivedWithThisAcsTerminated) {
          WARN("CWMP-1-1 Not supported by the ACS");
          acsSupportsCwmp_1_1 = false;
        }
	 
      }
      if (waitTime > ACSSESSIONSUPERVISOR) { waitTime = ACSSESSIONSUPERVISOR; }

    } else {
      // DBG("DM_COM_ACS_SESSION_SUPERVISOR - No ACS Session Created");
      waitTime = ACSSESSIONSUPERVISOR;
    }
    DM_CMN_Thread_unlockMutex(mutexAcsSession);
  }
  return 0;
}

/**
 * @brief This private routine is used to update the ACS SESSION TIMER.
 *        Each time a message is received from the ACS, this timer is updated to
 *        the current time.
 *
 * @param void
 *
 * @Return None
 *
 */
void 
_updateAcsSessionTimer(){

  DBG("_updateAcsSessionTimer - Begin");

  DM_CMN_Thread_lockMutex(mutexAcsSession);
    
  // Set the Time of the ACS Session creation.
  time(&g_DmComData.bSessionOpeningTime);
  
  DM_CMN_Thread_unlockMutex(mutexAcsSession);
  
  DBG("_updateAcsSessionTimer - End");
  
}

/**
 * @brief This private routine is used to retry the previous ACS HTTP Message
 *        The retry is performed on ACS Message reception with fault code set to 8005 (RetryRequest)
 *
 * @param void
 *
 * @Return None
 *
 */
void
_retryRequest(){

  DBG("_retryRequest - Send the previous CPE Request to the ACS");
 
  if(NULL != g_retryBuffer) {
    if ( DM_SendHttpMessage( g_retryBuffer ) == DM_OK ) {
      DBG( "Retry Request - Sending: OK" );
	  } else {
      WARN( "Retry Request - Sending: NOK" );
	  }
  }

}

/**
 * @brief Private routine. This method, is used to check if the parameter
 *                         is present more than one time in a single SET RPC Cmd.
 *
 * @param A pointer on the parameterList (last entry is NULL)
 *
 * @Return TRUE if the SET RPC Cmd is valid. False otherwise.
 *
 */
bool
_isValidRpcCommandList(DM_ENG_ParameterValueStruct * pParameterList[]) {
  const char * tmpStr;
  int    nbParam = 0;
  int    i = 0;
  int    j = 0;
  int    count = 0;
  
  // Compute the number of parameter to set
  while(pParameterList[nbParam] != NULL) {
    nbParam++;
  }
  
  for(i=0; i < nbParam; i++) {
    count = 0;
    tmpStr = pParameterList[i]->parameterName;
    
    // Make sure the parameterName appears only one time in the Set RPC Command List
    for(j = 0; j < nbParam; j++) {
      if(0 == strcmp(tmpStr, pParameterList[j]->parameterName)) {
        count++;
      }
    } // end for j
    
    // Check count value (must not be > 1)
    if(count > 1) {
      WARN("The %s parameter is set more than one time in a signle Set RPC Command", tmpStr);
      return false;
    }
    
  }  // end for i
  
  return true;
  
}

/**
 * @brief Private routine. This method, is used to check if the HTTP Server is started
 *
 * @param 
 *
 * @Return TRUE if the HTTP Server is started (false otherwise)
 *
 */
bool
_isHttpServerStarted() {
  return _httpServerStatus;

}

/* 
* Private routine used to build the Soap Message Skeleton
* Parameters: A pointer on the enveloppe XML Document
* @param allEnveloppeAttributs (true --> attr encoding, enveloppe, XMLSchema, XMLSchema-instance and 
                                          urn:dslforum-org:cwmp-1-0  ==> Inform and GetParameterValuesResponse
					  										 false --> attr encoding, enveloppe and urn:dslforum-org:cwmp-1-0 ==> All other cases )
* return true on success (false otherwise)
*/
bool 
_buildSoapMessageSkeleton(GenericXmlDocumentPtr   pParser,
                          bool                    allEnveloppeAttributs)
{
  GenericXmlNodePtr enveloppeNodePtr = NULL;
  // GenericXmlNodePtr headerNodePtr    = NULL;
  // GenericXmlNodePtr noMoreRequestsNodePtr    = NULL;
  
  if(NULL == pParser) {
    EXEC_ERROR("Invalid Parameter");
    return false;
  }
  
  enveloppeNodePtr = xmlGetFirstNodeWithTagName(pParser, _EnvelopeTagName); 
  if(NULL == enveloppeNodePtr) {
    EXEC_ERROR("Can not retrieve the Enveloppe Node");
    return false;
  }
  

  xmlAddNodeAttribut(enveloppeNodePtr, xmlns_soapenc_attribut, xmlns_soapenc_attribut_value);
  xmlAddNodeAttribut(enveloppeNodePtr, _SoapEnvAttrName,       xmlns_soapenv_attribut_value);
  if(true == allEnveloppeAttributs) {
    // Add extra attributes for Inform and GetParameterValuesResponse
    xmlAddNodeAttribut(enveloppeNodePtr, xmlns_xsd_attribut,             xmlns_xsd_attribut_value);  

  }
  xmlAddNodeAttribut(enveloppeNodePtr, xmlns_xsi_attribut,             xmlns_xsi_attribut_value);  // Added for ACS compatibility
  xmlAddNodeAttribut(enveloppeNodePtr, xmlns_cwmp_attribut,            _getCwmpVersionSupported());
 
  /*headerNodePtr =*/ xmlAddNode(pParser,
                             enveloppeNodePtr,
                             _HeaderTagName,
                             NULL,
                             NULL,
                             NULL);
			     
  xmlAddNode(pParser,
             enveloppeNodePtr,
             _BodyTagName,
             NULL,
             NULL,
             NULL);
			     
  return true;
}


/* 
* Private routine used to determine the CWMP Namespace Version to use
* The default version should be cwmp-1-1 but some ACS only support cwmp-1-0.
* Il the CPE uses cwmp-1-1 and the ACS only support cwmp-1-0, the CPE should
* automatically switch to cwmp-1-0.
* Parameters: None
* return A pointer on the string corresponding to the CWMP Namespace Version to use.
*/
static char * 
_getCwmpVersionSupported()
{
  if (DM_COM_IS_FORCED_CWMP_1_0_USAGE) {
    DBG("Force CWMP_1_0 Usage");
    return xmlns_cwmp_1_0_attribut_value;
  }

  if(true == acsSupportsCwmp_1_1) {
    WARN("CWMP-1-1 Supported by the ACS");
    return xmlns_cwmp_1_1_attribut_value;
  }
  
  WARN("CWMP-1-0 Supported by the ACS");
  return xmlns_cwmp_1_0_attribut_value;
}


/* END OF FILE */
