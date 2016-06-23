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
 * File        : DM_COM_HttpServerInterface.c
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
 * @file dm_com_http.c
 *
 * @brief http implementation of the http server
 *
 */

// System headers
#ifndef WIN32
#include <netdb.h>	  /* Needs for resolution network    */
#endif

// Enable the tracing support
#ifdef DEBUG
#define SHOW_TRACES 1
#define TRACE_PREFIX	"[DM-COM] %s:%s(),%d: ", __FILE__, __FUNCTION__, __LINE__
#endif

#include "DM_GlobalDefs.h"
#include "DM_COM_GenericHttpServerInterface.h"
#include "dm_com_digest.h"
#include "DM_COM_ConnectionSecurity.h"
#include "CMN_Trace.h"

// To retrieve g_DmComData data
extern dm_com_struct g_DmComData;


// // Private routine declaration
bool _sendAllMessage(int socketFd, const char * buf, int len);
bool _sendHttpResponse(char * httpCodeStr, char * httpString, int socketFd);
bool _checkHttpMessageContent(IN char *pSzBuf);



/**
 * @brief Launch the http server an infinite loop
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
void*
DM_HttpServer_Start(void* data UNUSED)
{
	struct hostent         * pExp  = NULL;
	struct sockaddr_in       AddrLocal;
	struct sockaddr_in       AdrExp;
	int                      nSocksExp  = 0;
	unsigned int             nLengthAddrExp;
	char                     SzBuf[SIZE_HTTP_MSG]; // No need to require a very large buffer.
	char                     szHostname[MAXHOSTNAMELEN];
   time_t                   connectionFreqArray[MAXCONNECTIONREQUESTARRAYSIZE];

#ifdef WIN32
  WSADATA WSAData;
  WSAStartup(MAKEWORD(2, 0), &WSAData);
#endif
			
	
	g_DmComData.ServerHttp.sockfd_serveur = socket( AF_INET, SOCK_STREAM, 0);
	if ( g_DmComData.ServerHttp.sockfd_serveur != -1 ){
	  DBG("dm_com - DM_HttpServer_Start");
	  
  // Init Connection Request Array
  initConnectionFreqArray(connectionFreqArray);
	  
  // -------------------------------------------------------------------
  // Set parameters of the http server's connexion
  // -------------------------------------------------------------------
  // AF_INET            = Internet protocol (not AF_UNIX)
  // NET_INTERFACE      = Don't specify a specific interface,
  //	  so this server will listen on each one
  // CPE_PORT           = Define on which port the HTTP server will listen
  // -------------------------------------------------------------------
  AddrLocal.sin_family      = AF_INET;
  AddrLocal.sin_addr.s_addr = htonl( NET_INTERFACE );
  AddrLocal.sin_port        = htons( CPE_PORT );
  
  // -------------------------------------------------------------------
  // Loop for binding the CPE's local address/port number.
  // If we cannot bind it maybe it is already in use by another program.
  // If we are rebooting, the TCP protocol need to wait a while before to retry.
  // See this page for more explanation about it :
  // NOTE : the while loop will work as long as the bind won't work
  //	  so it can become a problem in case another process use the
  //	  port defined for the tr-069's connexion request
  // -------------------------------------------------------------------
  INFO( "Http server is binding - Begin" );
  while ( bind( g_DmComData.ServerHttp.sockfd_serveur, (struct sockaddr*)&AddrLocal, sizeof(AddrLocal) ) != 0 );
  INFO( "Http server binded - End" );
  
  if ( listen( g_DmComData.ServerHttp.sockfd_serveur, MAX_BACKLOG_CONNECTION ) == 0 ) {
	  gethostname( szHostname, MAXHOSTNAMELEN );
	  INFO( "Listening the http server's socket : OK ( %s : %d )", szHostname,  CPE_PORT );
  } else {
	  EXEC_ERROR( "Listening the http server's socket : NOK (Errno.%d) ", errno );
	  return 0;
  }
  
  DBG("DM_COM Init End");
  // Notify DM_COM that the HTTP Server initialisation is done.
  DM_HttpServerStarted();
 
  // -------------------------------------------------------------------
  // Initialize the exit value of the main loop
  // then launch the http server's main loop
  // -------------------------------------------------------------------
  g_DmComData.ServerHttp.nExitHttpSvr = 0;
  while ( g_DmComData.ServerHttp.nExitHttpSvr != 1 )
  {
     int httpCode = 0;

	  // Wait for a client ...
	  nLengthAddrExp = sizeof( AdrExp );
     nSocksExp = accept( g_DmComData.ServerHttp.sockfd_serveur, (struct sockaddr*)&AdrExp, &nLengthAddrExp );
	  if ( nSocksExp == -1 )
     {
        EXEC_ERROR( "Bad connection established with the client (Err.%d) - Accept failed.", errno );
        continue;
     }

     INFO( "HTTP Server - Accept a new connection..." );

    // Check connection policy (max number of connection per minute 
    // and from device start up.
    if (true == maxConnectionPerPeriodReached(connectionFreqArray))
    {
       WARN( "Can not accept this connection (Policy Connection Refused)" );
       // Respond to that connection request with the HTTP 503 Status
          httpCode = HTTP_SERVICE_UNAVAILABLE;
    }
    else
    {
       // -------------------------------------------------------------------
       // Display several information about the client connected
       // -------------------------------------------------------------------
       pExp = gethostbyaddr( (char*)&AdrExp.sin_addr, sizeof(AdrExp.sin_addr), AF_INET );
       if ( pExp != NULL ) {
         INFO( "Connexion well established with the following client: %s", (char*)pExp->h_name );
       } else {
         WARN( "Connexion well established with an unknown client!!" );
       }

       // -------------------------------------------------------------------
       // Initialise the buffer then Get it
       // -------------------------------------------------------------------
       memset((void *) SzBuf, 0x00, SIZE_HTTP_MSG );
       // Wait a while after accept..
       DM_CMN_Thread_msleep(10);

       int res;
       int maxLoopCounter = 0;
       int maxLoopAllowed = 50;
       do {
#ifndef WIN32
         res = recv( nSocksExp, SzBuf, SIZE_HTTP_MSG, MSG_DONTWAIT );
#else
         res = recv( nSocksExp, SzBuf, SIZE_HTTP_MSG, 0 );
#endif

         if (res != 0 || errno != EAGAIN) {
            break;
	      }

	      DBG("Receive No Data or EAGAIN - Wait a while and try again later");
         DM_CMN_Thread_msleep(10);

	      // Increment maxLoopCounter (make sure to avoid infinite loop)
	      maxLoopCounter++;
       } while(maxLoopCounter<maxLoopAllowed);

       if ((-1 == res) || (maxLoopCounter == maxLoopAllowed))
       {
          EXEC_ERROR("ERROR while receiving message from socket");
       }
       else
       {
          // -------------------------------------------------------------------
          // Does a session is already in progress ?
          // -------------------------------------------------------------------
          if ( g_DmComData.bSession ) // A session is already created
          {
             // -------------------------------------------------------------------
             // Send a 503 HTTP message (busy)
             // -------------------------------------------------------------------
             EXEC_ERROR( "A session is already in process. Only one can be used at one time!!" );
             httpCode = HTTP_SERVICE_UNAVAILABLE;
          }
          else
          {

#ifndef WIN32 // NO AUTH ON WINDOWS
             // If this HTTP Message do not contain DIGEST Authentication data, send
             // an authentication request.
             if (false == _checkDigestAuthMessageContent(SzBuf)) {
                INFO( "HTTP Message do not contain Authentication Data." );
                httpCode = HTTP_UNAUTHORIZED;
             }
             else
             {
                INFO( "HTTP Message contain DIGEST Authentication data." );

                // The HTTP Message contains Digest Authentication data.
                // Perform the authentication (check certificat)
                char * tmpStr = DM_ENG_strndup(SzBuf, SIZE_HTTP_MSG-1);
                if (false == performClientDigestAuthentication(tmpStr)){
                   WARN( "DIGEST AUTHENTICATION: FAILED." );
                   httpCode = HTTP_UNAUTHORIZED;
                }
                else
                {
                   INFO( "DIGEST AUTHENTICATION: SUCCESS." );
                }
                DM_ENG_FREE(tmpStr);
             }
#endif

             if (httpCode == 0)
             {
                // As to analyse and valid the content received
   	          if ( true == _checkHttpMessageContent( SzBuf ) )
                {
   	             // If okay, ask to DM_Engine to open the session
                   if ( DM_ENG_RequestConnection( DM_ENG_EntityType_ACS ) == 0 )
                   {
   		             // Update the Request Connection Array
                      newConnectionRequestAllowed(connectionFreqArray);

                      httpCode = HTTP_OK;
                   }
   	             else
                   {
   	                // Can not perform request connection
                      httpCode = HTTP_SERVICE_UNAVAILABLE;
   	             }
   	          }
                else
                {
                   EXEC_ERROR( "Validation of the connexion request : NOK " );
                   DBG( "Sending an http message to the ACS because a authentification denied!!" );
                   httpCode = HTTP_UNAUTHORIZED;
                }
             }
          }
       }
    }

    if (httpCode == HTTP_UNAUTHORIZED)
    {
       INFO( "Request the ACS to provide Authentication Data." );
       _sendAuthenticationRequest(nSocksExp);
    }
    else if (httpCode == HTTP_SERVICE_UNAVAILABLE)
    {
       DBG( "Sending an http message to the ACS with the 503 code" );
       _sendHttpResponse(HTTP_CODE_SVR_BUSY, HTTP_STRING_SVR_BUSY, nSocksExp);
    }
    else if (httpCode == HTTP_OK)
    {
       // Send a HTTP response with either the code 200 or 204
       _sendHttpResponse(HTTP_CODE_OK, HTTP_STRING_OK, nSocksExp);
    }

    // Close the current socket used for the transaction
    close( nSocksExp );
  } // End while

  // --> The socket will be close in a DM_COM_Stop function
	} else {
  EXEC_ERROR( "[%d] Problem for creating the socket needs for the http server!!",
                       g_DmComData.ServerHttp.sockfd_serveur );
	}

#ifdef WIN32
  WSACleanup();
#endif
			
	return 0;
}


// ------------------------------------------------------------------
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// ------------------- PRIVATE ROUTINES ARE BELOW -------------------
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// ------------------------------------------------------------------

/**
 * @brief Send and HTTP Response on the given socket with the http Code and String
 *
 * @param httpCodeStr, httpString and socket to use.
 *
 * @return return true on success, false otherwise.
 *
 */
bool
_sendHttpResponse(
  char * httpCodeStr,
  char * httpString,
  int    socketFd)
{
  
  char                 pHTTP_Response[SIZE_HTTP_MSG];
  char                 pHTTP_Datetime[SIZE_HTTP_DATE];
  time_t               currentTime;
  
  
  if((NULL == httpCodeStr) || (NULL == httpString)) {
    EXEC_ERROR("Invalid Argument");
    return false;  
  }
	      
  DBG( "Sending an http response (Code: %s, String: %s)", httpCodeStr, httpString);
  
  currentTime = time( NULL );
  strftime( pHTTP_Datetime, sizeof(pHTTP_Datetime), HTTP_DATETIME, gmtime(&currentTime) );
  
  sprintf( pHTTP_Response, HTTP_RESPONSE, httpCodeStr, httpString );
  strcat( pHTTP_Response, pHTTP_Datetime );
  strcat( pHTTP_Response, HTTP_LENGTH    );
  DBG( "HTTP response to send to the ACS server:\n%s ", pHTTP_Response );
	      
  if(false == _sendAllMessage(socketFd, pHTTP_Response, strlen(pHTTP_Response))) {
    EXEC_ERROR("Error sending message on socket!!!");
    return false;
  } else {
    DBG("HTTP Message Sent");
  }      
  
  return true;
}

/*
* This routine is used to send a message in a socket. This routine ensure that 
* the complete message is sent.
*/
bool
_sendAllMessage(int          socketFd,
                const char * buf,
                int          len){

  int total = 0;        // How many byte we have sent
  int bytesleft = len;  // How many we have left to send
  int n;
  	      
  // Check Parameters
  if(NULL == buf){
    EXEC_ERROR( ERROR_INVALID_PARAMETERS );
    return false;
  }
	
  while(total < len) {
    n = send(socketFd, buf+total, bytesleft, 0);
    if(n == -1) {
      EXEC_ERROR( "Impossible to send the Http Message." );
      return false;
    }
    total     += n;
    bytesleft -= n;
  }
	
  return true;
	      
}

/**
 * @brief function which manage the a http connexion
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
bool
_checkHttpMessageContent(IN char *pSzBuf)
{
	char	    pBufferTemp[SIZE_HTTP_MSG]; // No need to require a very large buffer.
	bool	    nRet        = false;
	char	   *pMethod     = NULL;
	char	   *pPath       = NULL;
	char	   *pProtocol   = NULL;
	int       nLength     = 0;

	
	// Check parameter
	if ( pSzBuf != NULL ){
    // -------------------------------------------------------------------
    // Recopy the buffer into a temp one ; avoid the strcpy function against the buffer overflow
    // The recopy is needed becose of the usage of the strtok function ; this is not the same 
    // with the strtok_r function.
    // -------------------------------------------------------------------
    // bzero( pBufferTemp, sizeof(pBufferTemp) );
    memset((void *) pBufferTemp, 0x00, sizeof(pBufferTemp) );
    strncpy( pBufferTemp, pSzBuf, SIZE_HTTP_MSG );
	  
   
    // -------------------------------------------------------------------
    // Separate the following information from the http request :
    // GET /toto/ HTTP/1.1 --> <method> <path> <protocol/version>
    // and username
    // -------------------------------------------------------------------
    pMethod   = strtok( pBufferTemp, " " );
    pPath     = strtok( NULL,        " ");
    pProtocol = strtok( NULL,        "\r");

    if ( (NULL == pMethod) || (NULL == pPath) || (NULL == pProtocol) ) {
	    EXEC_ERROR( "Not all the information haven't been found as requested in the http message!!" );
	    return nRet;
    } else {
	    nLength        = sizeof(pSzBuf);
	    //int lenthfirst = sizeof(pMethod) + sizeof(pPath) + sizeof(pProtocol);
	  
	    // -------------------------------------------------------------------
	    // Result of the previous analyse. display the element sent in the http
	    // message and will be checked with the values expected.
	    // -------------------------------------------------------------------
	    DBG( "Summary of the previous analyse:" );
	    DBG( " - Protocol and version used = '%s' ", pProtocol );
	    DBG( " - Method                    = '%s' ", pMethod   );
	    DBG( " - Path requested in the URL = '%s' ", pPath     );
	    DBG( " - Data length               = %d   ", nLength   );
  
      // -------------------------------------------------------------------
      // Then compare the values
      // -------------------------------------------------------------------
      if ( (strcmp( pMethod,   CONNECTION_REQUESTION_METHOD   ) == 0) &&
           (strcmp( pProtocol, CONNECTION_REQUESTION_PROTOCOL ) == 0) )	{
	      INFO( " Message Content: OK" );
	      nRet = true;
      } else {
	      EXEC_ERROR( " Message Content: NOK" );
	    }
	  
    }
  
	} else {
  EXEC_ERROR( ERROR_INVALID_PARAMETERS );
	}
	
	return( nRet );
}



/* END OF FILE */

