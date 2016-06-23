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
 * File        : DM_COM_HttpClientInterface.c
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
 * @file curlHttpClientInterface.c
 *
 * @brief Definition of the ELSE trace system
 *
 * This file implements the generic HTTP API with libcurl
 *
 */
 

#include "DM_COM_GenericHttpClientInterface.h"

#include "curl/curl.h"
#include "CMN_Trace.h"
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

extern char * PROXY_ADDRESS_STR;

#define DM_HTTP_THREAD_STACK_SIZE (64*1024*2) // Require larger stack size due to ixml recursive call involving stack overflow

// Pointer on the curl session
static CURL* _sessionHandle = NULL; // Non NULL value if and only if a session is in progress

// Used to store Private Data
static char * _acsURLPtr                    = NULL;
static char * _acsUsernameStr               = NULL;
static char * _acsPasswordStr               = NULL;
static char * _sslDeviceCertificatStr       = NULL;
static char * _sslDeviceRsaKey              = NULL;
static char * _sslAcsCertificationAuthority = NULL;

// Private routines declaration
static int    _curlGetHttpFile(httpGetFileDataType * httpGetDataPtr);
static int    _curlPutHttpFile(httpPutFileDataType * httpPutDataPtr);
static int    _curlPutFtpFile(httpPutFileDataType * httpPutDataPtr);
static int    _initHttpSession(); // HTTP Session inititalization is done using httpClientConfiguration
static size_t _curlCallbackClientData(void   *ptr, size_t  size, size_t  nmemb, void   *stream); // libcurl call back function
static size_t _curlCallbackClientHeader(void *ptr, size_t  size, size_t  nmemb, void   *stream); // libcurl call back function
static int    _getLastHttpResponseCode(OUT int * respCodePtr);
static void   _curlHandleCleanUp();
static void*  _sendHttpMessage();
static void   _addPendingHttpMessage(const char* httpMsg);
static char*  _getFirstPendingHttpMessage();
static void   _freePendingHttpMessages();
static int    _getFirmwareUpgradeErrorCodeFromHttpCode(int rc);
static int    _getUploadErrorCodeFromHttpCode(int httpRc);
static char * _stringToLower(const char * str);
static int    _isValidUrl(const char * urlStr);
static void   _CloseHttpSession(bool closeMode);

// -------------------------------------------------------------
// Definition about libcurl
// -------------------------------------------------------------
#define  CURL_TIMEOUT          (60)
#define  CURL_GET_FILE_TIMEOUT (300) // Maximum amount of time to download the File.
                                     // Attention, Ne doit pas être trop car peut être bloquant, ni trop court pour avoir assez de temps de téléchargement !!
#define  CURL_PUT_FILE_TIMEOUT (300) // Maximum amount of time to upload the File
#define  CURL_CONNECT_TIMEOUT  (30)
#define  CURL_NOSIGNAL         (1)

// HTTP ERROR CODE DEFINTION (2xx Success, 4xx and 5xx error)
#define HTTP_OK                (200)
#define HTTP_TRANSFER_COMPLETE (226)
#define HTTP_UNAUTHORIZED      (401)
#define HTTP_FORBIDDEN         (403)

// List used to define the HTTP Content-Type
static struct  curl_slist * _slist = NULL;
#define HttpXmlContentType    "Content-Type: text/xml; charset=\"utf-8\""
#define HttpEmptyContentType  "Content-type: "

/* -------------------------------------------------- */
typedef struct _PendingHttpMessage
{
  char* message;
  struct _PendingHttpMessage* next;

} __attribute((packed)) PendingHttpMessage;

static bool  _closeHttpSessionExpected          = false; // Flag to indicate if the HTTP Session must be closed
static char* _httpMessageBeingSent              = NULL;  // Message being sent
static PendingHttpMessage* _pendingHttpMessages = NULL;  // Message pending

#define DefaultFtpUploadFileName "UploadedFile"


pthread_mutex_t mutexHttpSendThreadControl = PTHREAD_MUTEX_INITIALIZER;


/* --------------------------------------------------- */

/*
* @brief Function used to configure HTTP Client Data Structures
*
* @param acs url, username and paswword
*        ssl Pub Cert, RSA Device Key and ACS CA.
*
* @return 0 on succes, -1 otherwise
*
*/
int DM_ConfigureHttpClient(const char * acsUrl,
                           const char * acsUsername,
                           const char * acsPassword,
                           const char * sslDevicePublicCertificatFile,
                           const char * sslDeviceRsaKeyFile,
                           const char * sslAcsCertificationAuthorityFile)
{
   DMRET nRet = DM_ERR;
   
   DBG("ACS URL      = %s", ((NULL != acsUrl)                           ? acsUrl                           : "None"));
   DBG("ACS Username = %s", ((NULL != acsUsername)                      ? acsUsername                      : "None"));
   DBG("ACS Password = %s", ((NULL != acsPassword)                      ? acsPassword                      : "None"));
   DBG("SSL Pub Cert = %s", ((NULL != sslDevicePublicCertificatFile)    ? sslDevicePublicCertificatFile    : "None"));
   DBG("SSL Priv Key = %s", ((NULL != sslDeviceRsaKeyFile)              ? sslDeviceRsaKeyFile              : "None"));
   DBG("SSL ACS CA   = %s", ((NULL != sslAcsCertificationAuthorityFile) ? sslAcsCertificationAuthorityFile : "None"));

  pthread_mutex_lock(&mutexHttpSendThreadControl);

  if(NULL == acsUrl) {
    EXEC_ERROR("No ACS URL");
  } else {
     if(NULL == _acsURLPtr) {
       _acsURLPtr = strdup(acsUrl);
     } else if(0 != strcmp(acsUrl, _acsURLPtr)) {
       // New value
         DM_ENG_FREE(_acsURLPtr);
         _acsURLPtr = strdup(acsUrl);
     }
     
     // Configure the Username
     if(NULL != acsUsername) {
       if(NULL == _acsUsernameStr) {
         _acsUsernameStr = strdup(acsUsername);
       } else if(0 != strcmp(_acsUsernameStr, acsUsername)) {
         // username update
         DM_ENG_FREE(_acsUsernameStr);
         _acsUsernameStr = strdup(acsUsername);        
       }
     } else {
       DM_ENG_FREE(_acsUsernameStr);
     }
     
     // Configure the Password
     if(NULL != acsPassword) {
       if(NULL == _acsPasswordStr) {
         _acsPasswordStr = strdup(acsPassword);
       } else if(0 != strcmp(_acsPasswordStr, acsPassword)) {
         // Password update
         DM_ENG_FREE(_acsPasswordStr);
         _acsPasswordStr = strdup(acsPassword);        
       }
     } else {
       DM_ENG_FREE(_acsPasswordStr);
     }
     
   
     if(( NULL == _sslDeviceCertificatStr ) && ( NULL != sslDevicePublicCertificatFile )) {
       _sslDeviceCertificatStr = strdup(sslDevicePublicCertificatFile);
     }
   
     if(( NULL == _sslDeviceRsaKey) && ( NULL != sslDeviceRsaKeyFile )){
       _sslDeviceRsaKey = strdup(sslDeviceRsaKeyFile);
     } 
     
     if(( NULL == _sslAcsCertificationAuthority ) && ( NULL != sslAcsCertificationAuthorityFile )) {
         _sslAcsCertificationAuthority = strdup(sslAcsCertificationAuthorityFile);
     }
     
     nRet = DM_OK;
  }

  pthread_mutex_unlock(&mutexHttpSendThreadControl);
  
  return nRet;
      
} // DM_ConfigureHttpClient


/*
* @brief Function used to close the HTTP Client Session
*
* @param None
*
* @return 0 on success (-1 otherwise)
*
*/
int DM_CloseHttpSession(bool closeMode)
{
  DBG("DM_CloseHttpSession - Begin");

  pthread_mutex_lock(&mutexHttpSendThreadControl);

  _CloseHttpSession(closeMode);

  pthread_mutex_unlock(&mutexHttpSendThreadControl);

  DBG("DM_CloseHttpSession - End");

  return DM_OK;

} // DM_CloseHttpSession


/*
* @brief Function used to free HTTP Client Data Strucutures and close the Http Session
*
* @param void
*
* @return void
*
*/
int DM_StopHttpClient(void)
{
  pthread_mutex_lock(&mutexHttpSendThreadControl);

  _CloseHttpSession(IMMEDIATE_CLOSE);

  // Free allocated memory
  DM_ENG_FREE(_acsURLPtr);
  DM_ENG_FREE(_sslDeviceCertificatStr);
  DM_ENG_FREE(_sslDeviceRsaKey);
  DM_ENG_FREE(_sslAcsCertificationAuthority);

   pthread_mutex_unlock(&mutexHttpSendThreadControl);

   return 0;

} // DM_StopHttpClient


/*
* @brief Function used to send an HTTP Message
*
* @param IN: ptr on the message to send
*
* @return 0 on success (-1 otherwise)
*
*/
int DM_SendHttpMessage(IN const char * msgToSendStr)
{
  pthread_attr_t   httpSendthread_Attribute;  
#ifndef WIN32
  pthread_t        httpSendthread_ID        = 0;
#else
  pthread_t        httpSendthread_ID;
#endif
   DMRET            nRet                     = DM_ERR;
   int              msgSize                  = 0;

   DBG("DM_SendHttpMessage - Begin");

   // Check parameter
   if ( msgToSendStr == NULL ) // Null pointer parameter
   {
      EXEC_ERROR( ERROR_INVALID_PARAMETERS );
   }
   else
   {
      pthread_mutex_lock(&mutexHttpSendThreadControl);

      if (_closeHttpSessionExpected)
      {
         WARN("Already a session in progress, retry later"); // We must not have 2 sessions for a single TCP connection
      }
      else if (_httpMessageBeingSent != NULL)
      {
         _addPendingHttpMessage(msgToSendStr);

          // Set the return code to OK
          nRet = DM_OK;
      }
      else if ((_sessionHandle == NULL) && (DM_OK != _initHttpSession()))
      {
         EXEC_ERROR("Can not set up the HTTP Session");
      }
      else
      {
         _httpMessageBeingSent = strdup(msgToSendStr);

         // -----------------------------------------------------------------------
         // Set the thread's parameters
         // -----------------------------------------------------------------------
         pthread_attr_init( &httpSendthread_Attribute );
         pthread_attr_setdetachstate(&httpSendthread_Attribute,PTHREAD_CREATE_DETACHED);
         pthread_attr_setstacksize(&httpSendthread_Attribute, DM_HTTP_THREAD_STACK_SIZE );

         // -----------------------------------------------------------------------
         // Launch the thread
         // -----------------------------------------------------------------------
         if ( pthread_create( &httpSendthread_ID,
                              &httpSendthread_Attribute,
                              (void*(*)(void*))_sendHttpMessage,
                              NULL ) != 0 )
         {
            DM_ENG_FREE(_httpMessageBeingSent);
            EXEC_ERROR( "Launching the '_sendHttpMessage' as a thread : NOK " );
         }
         else
         {
             // Update the retry buffer
             if(NULL != msgToSendStr) msgSize = strlen(msgToSendStr);
             DM_UpdateRetryBuffer(msgToSendStr, msgSize);

            pthread_detach(httpSendthread_ID);
            nRet = DM_OK;
         }

         // -----------------------------------------------------------------------
         // Free the pthread's attributes
         // -----------------------------------------------------------------------
         pthread_attr_destroy( &httpSendthread_Attribute );
      }

      pthread_mutex_unlock(&mutexHttpSendThreadControl);
   }

   DBG("DM_SendHttpMessage - End");

   return( nRet );

} // DM_SendHttpMessage


/*
* @brief Function used to get a file using HTTP GET.
*
* @param IN: char * fileURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: int    filSize
*        IN: char * targetFileNameStr 
*
* @return The FW Upgrade Error Code
*
*/
int DM_HttpGetFile(IN httpGetFileDataType * httpGetFileDataPtr){
   
   DMRET                 nRet = DM_ERR;
  httpGetFileDataType * _httpGetFileDataPtr = NULL;
  
  DBG("DM_HttpGetFile - Begin");
  

  // Check mandatory parameters
  if((NULL == httpGetFileDataPtr)                        ||
     (NULL == httpGetFileDataPtr->fileURLStr)            ||
     (NULL == httpGetFileDataPtr->targetFileNameStr)) {
     // Invalid parameters
     EXEC_ERROR("Invalid Parameters");
     return DM_ENG_INVALID_ARGUMENTS;
  }
  
  // Build httpGetFileDataPtr (Note: httpGetFileDataPtr must be free by the created thread below)
  _httpGetFileDataPtr = (httpGetFileDataType *) malloc(sizeof(httpGetFileDataType));
  memset((void *) _httpGetFileDataPtr, 0x00, sizeof(httpGetFileDataType));
  
  _httpGetFileDataPtr->fileURLStr = strdup(httpGetFileDataPtr->fileURLStr);
  
  _httpGetFileDataPtr->usernameStr = NULL;
  if(NULL != httpGetFileDataPtr->usernameStr) {
    _httpGetFileDataPtr->usernameStr = strdup(httpGetFileDataPtr->usernameStr);
  }
  
  _httpGetFileDataPtr->passwordStr = NULL;
  if(NULL != httpGetFileDataPtr->passwordStr) {
    _httpGetFileDataPtr->passwordStr = strdup(httpGetFileDataPtr->passwordStr);
  }
  
  _httpGetFileDataPtr->filSize = httpGetFileDataPtr->filSize;

  _httpGetFileDataPtr->targetFileNameStr = strdup(httpGetFileDataPtr->targetFileNameStr);
  
  // Perform the HTTP GET
  nRet = _curlGetHttpFile(_httpGetFileDataPtr);
  
  // Free allocated memory
  DM_ENG_FREE(_httpGetFileDataPtr->fileURLStr);
  DM_ENG_FREE(_httpGetFileDataPtr->usernameStr);
  DM_ENG_FREE(_httpGetFileDataPtr->passwordStr); 
  DM_ENG_FREE(_httpGetFileDataPtr->targetFileNameStr);
  DM_ENG_FREE(_httpGetFileDataPtr);

  return nRet; 

} // DM_HttpGetFile


/*
* @brief Function used to upload a file using HTTP PUT.
*
* @param IN: char * destURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: char * localFileNameStr 
*
* @return The FW Upload Error Code
*
*/
int DM_HttpPutFile(httpPutFileDataType * httpPutFileDataPtr)
{

  // Check mandatory parameters
  if((NULL == httpPutFileDataPtr) ||
     (NULL == httpPutFileDataPtr->destURLStr) ||
     (NULL == httpPutFileDataPtr->localFileNameStr)) {
     EXEC_ERROR("Invalid Parameter");
     return DM_ENG_INVALID_ARGUMENTS;
  }
  
  return (_curlPutHttpFile(httpPutFileDataPtr));

}

/*
* @brief Function used to upload a file using FTP PUT.
*
* @param IN: char * destURLStr
*        IN: char * usernameStr
*        IN: char * passwordStr
*        IN: char * localFileNameStr 
*
* @return The FW Upload Error Code
*
*/
int DM_FtpPutFile(httpPutFileDataType * httpPutFileDataPtr)
{

  // Check mandatory parameters
  if((NULL == httpPutFileDataPtr) ||
     (NULL == httpPutFileDataPtr->destURLStr) ||
     (NULL == httpPutFileDataPtr->localFileNameStr)) {
     EXEC_ERROR("Invalid Parameter");
     return DM_ENG_INVALID_ARGUMENTS;
  }
  
  return (_curlPutFtpFile(httpPutFileDataPtr));

}



// -----------------------------------------------------------------
// --------- Private curlHttpClient routine are below --------------
// -----------------------------------------------------------------


// -------------------------------------------------------------
// Management of the list of the HTPP messages pending
// -------------------------------------------------------------
static void _addPendingHttpMessage(const char* httpMsg)
{
   if (httpMsg == NULL) return;

   PendingHttpMessage* newMsg = (PendingHttpMessage*) malloc(sizeof(PendingHttpMessage));
   newMsg->message = strdup(httpMsg);
   newMsg->next = NULL;
   if (_pendingHttpMessages == NULL)
   {
      _pendingHttpMessages = newMsg;
   }
   else
   {
      PendingHttpMessage* curMsg = _pendingHttpMessages;
      while (curMsg->next != NULL) { curMsg = curMsg->next; }
      curMsg->next = newMsg;
   }
}

static char* _getFirstPendingHttpMessage()
{
   char* msgRet = NULL;
   if (_pendingHttpMessages != NULL)
   {
      PendingHttpMessage* first = _pendingHttpMessages;
      msgRet = first->message;
      _pendingHttpMessages = first->next;
      free(first);
   }
   return msgRet;
}

static void _freePendingHttpMessages()
{
   while (_pendingHttpMessages != NULL)
   {
      PendingHttpMessage* first = _pendingHttpMessages;
      _pendingHttpMessages = first->next;
      free(first->message);
      free(first);
   }
}



/*
* @brief Function used to initialize the HTTP Client
*        The initialization is done using httpClientConfiguration data
*
* @param IN: ptr on initHttpClientStructType structure
*
* @return 0 on success (-1 otherwise)
*
*/
static int _initHttpSession()
{
   DMRET                          nRet = DM_ERR;
  int                            usernamePasswordStrSize = 0;
  char                         * usernamePasswordStr     = NULL;
  
   DBG("DM_InitHttpClient - Begin");

  // Check parameters
  if(NULL == _acsURLPtr) {
    EXEC_ERROR("Invalid Parameters");
    return nRet;
  }

   // ------------------------------------------------------------
   // Global cCURL initialization
   // ------------------------------------------------------------
   nRet = curl_global_init( CURL_GLOBAL_ALL );
   if ( nRet != 0 ) {
    EXEC_ERROR( "Global CURL initialization : NOK (nRet = %d)", nRet );
    return( DM_ERR );
   }

   
   // ------------------------------------------------------------
   // Initializing the cURL interface
   // ------------------------------------------------------------
   _sessionHandle = curl_easy_init();
   curl_easy_reset(_sessionHandle);
   if ( _sessionHandle == NULL ) {
    EXEC_ERROR( "Initializing the CURL interface : NOK" );
    return( DM_ERR );
   } else {
    DBG( "Initializing the CURL interface : OK " );
   }

   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_NOSIGNAL, CURL_NOSIGNAL)) {
     EXEC_ERROR("Can not set CURLOPT_NOSIGNAL curl option");
   }
   
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_TIMEOUT, CURL_TIMEOUT)) {
     EXEC_ERROR("Can not set CURLOPT_TIMEOUT curl option");
   }
   
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT)) {
     EXEC_ERROR("Can not set CURLOPT_CONNECTTIMEOUT curl option");
   }
   
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_URL, _acsURLPtr)) {
     EXEC_ERROR("Can not set CURLOPT_URL curl option"); 
   }
   
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_NETRC, CURL_NETRC_IGNORED)) {
     EXEC_ERROR("Can not set CURLOPT_NETRC curl option");
   }
   
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1)) {
     EXEC_ERROR("Can not set CURLOPT_HTTP_VERSION curl option");
   }
   
   // Enable CURL Cookies Handling. The given file can not exist (this is not a failure)
   if(CURLE_OK != curl_easy_setopt( _sessionHandle, CURLOPT_COOKIEFILE, "/tmp/curlCookiesFile")) { 
     EXEC_ERROR("Can not set COOKIEFILE curl option");
   }  
   
   curl_easy_setopt( _sessionHandle, CURLOPT_COOKIESESSION, 1);

   // PROXY
   if(NULL != PROXY_ADDRESS_STR) {
     if(CURLE_OK != curl_easy_setopt(_sessionHandle, CURLOPT_PROXY, PROXY_ADDRESS_STR)) {
       EXEC_ERROR("Can not set PROXY curl option");
     }
   }

  // Set the CURL HTTP data call back function
  curl_easy_setopt( _sessionHandle, CURLOPT_WRITEDATA,     /* g_DmComData */ NULL );
  curl_easy_setopt( _sessionHandle, CURLOPT_WRITEFUNCTION, _curlCallbackClientData );

   /* SECURITY MANAGEMENT */
   // Check if the Cetificat Authority is provided to identify the ACS platform
  if(NULL != _sslAcsCertificationAuthority) {
    // ----- Set CA to identify the ACS platform -------
    INFO("ACS Certificat Authority: %s", _sslAcsCertificationAuthority);
    curl_easy_setopt(_sessionHandle, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(_sessionHandle, CURLOPT_CAINFO, _sslAcsCertificationAuthority );
    curl_easy_setopt(_sessionHandle, CURLOPT_SSL_VERIFYPEER, 0L);
    // Only check the Common Name (verify the CN in the Certificat match with the requested URL)
    curl_easy_setopt(_sessionHandle, CURLOPT_SSL_VERIFYHOST, 2L);   
  } else {
    // ------ No need to identify the ACS platform ----------
    INFO("No ACS Certificat Authority");
    curl_easy_setopt(_sessionHandle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(_sessionHandle, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  if((NULL != _sslDeviceCertificatStr) && (NULL != _sslDeviceRsaKey)) {
    INFO("Device Certificat: %s, Device Private Key = %s", _sslDeviceCertificatStr, _sslDeviceRsaKey);
    // ----  Set CURL Configuration to authentify the CLIENT ---- 
    curl_easy_setopt(_sessionHandle, CURLOPT_SSLKEYTYPE, "PEM");    
    curl_easy_setopt(_sessionHandle, CURLOPT_SSLCERT,    _sslDeviceCertificatStr);
    curl_easy_setopt(_sessionHandle, CURLOPT_SSLKEY,     _sslDeviceRsaKey);
  }
  
  if((NULL == _sslAcsCertificationAuthority) &&
     (NULL == _sslDeviceCertificatStr)       &&
     (NULL == _sslDeviceRsaKey)) {
    // No ACS or CPE authentication required
    INFO("No SSL security required - HTTP");
    
    // Check if ManagementServer.Username and Passwaord exist.

    if(NULL != _acsUsernameStr) {
      DBG("An ACS Username Authentication is required: %s", _acsUsernameStr);
      usernamePasswordStrSize = strlen(_acsUsernameStr) + 2; // For ":" separator
       if(NULL != _acsPasswordStr) {
         // A password exist;
          usernamePasswordStrSize += strlen(_acsPasswordStr);
       } 
       // Allocate memory for the username and password string
       usernamePasswordStr = (char *) malloc(usernamePasswordStrSize);  
       strcpy(usernamePasswordStr, _acsUsernameStr);
       strcat(usernamePasswordStr, ":");
       if(NULL != _acsPasswordStr) {
         DBG("An ACS Password Authentication is required: %s", _acsPasswordStr);
         // A password exist;
          strcat(usernamePasswordStr, _acsPasswordStr);
       }
             
       INFO("CURLOPT_USERPWD String: %s", usernamePasswordStr);
       curl_easy_setopt(_sessionHandle, CURLOPT_USERPWD, usernamePasswordStr);         
                  
    }
    
    // Allow digest authentication
    curl_easy_setopt( _sessionHandle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST );

     
  } else {
     // Authentication required. Set SSL Parameter
     
    curl_easy_setopt(_sessionHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
      curl_easy_setopt( _sessionHandle, CURLOPT_SSL_CIPHER_LIST, "DES-CBC3-SHA RC4-SHA");  
  }
  
  curl_easy_setopt(_sessionHandle, CURLOPT_POST, 1);
  
  // Set the CURL HTTP header call back function (when session is opened - Digest Auth Done)
  curl_easy_setopt( _sessionHandle, CURLOPT_WRITEHEADER,    /* g_DmComData */ NULL    );
  curl_easy_setopt( _sessionHandle, CURLOPT_HEADERFUNCTION, _curlCallbackClientHeader );

  // Free allocated memory
  DM_ENG_FREE(usernamePasswordStr);

   nRet = DM_OK;
   
   DBG("DM_InitHttpClient - End");

  return nRet;
} // _initHtcp tpSession

/**
 * @brief Function to get an http file using libcurl
 *
 * @param pParam  Message to send
 *
 * @Return The FW Upgrade Error Code
 *
 */    
static int _curlGetHttpFile(httpGetFileDataType * httpGetDataPtr)
{
  httpGetFileDataType * httpGetFileDataPtr = (httpGetFileDataType *) httpGetDataPtr;
  char                * fileToStoreStr = NULL;
  int                   fileToStoreStrSize = 0;
  FILE                * fp = NULL;
  CURL                * session = NULL;
  int                   userNamePasswordStrSize = 0;
  char                * userNamePasswordStr = NULL;
  int                   rc = DM_ERR; 
 
  // Build the file to store (path + \ + fileName)
  // Compute the size
  
  if(NULL == httpGetDataPtr) {
    EXEC_ERROR("Invalid Parameter");
    return DM_ENG_INVALID_ARGUMENTS;
  }
  
  // ---------- Display Trace ----------------
  DBG("GET File URL    : %s", httpGetDataPtr->fileURLStr);
  DBG("Target File Name: %s", httpGetDataPtr->targetFileNameStr);  
  if(NULL != httpGetDataPtr->usernameStr) {
    DBG("User Name       : %s", httpGetDataPtr->usernameStr);
  } else {
    DBG("User Name       : No Username");
  }
  if(NULL != httpGetDataPtr->passwordStr) {
    DBG("Password        : %s", httpGetDataPtr->passwordStr);
  } else {
    DBG("Password        : No Password");
  } 
  
  // Make sure it is a valid URL (HTTP type)
  if(0 != _isValidUrl(httpGetDataPtr->fileURLStr)) {
    WARN("Invalid HTTP URL (%s)", httpGetDataPtr->fileURLStr);
    return DM_ENG_UNSUPPORTED_PROTOCOL;
  }

  fileToStoreStrSize = strlen(httpGetFileDataPtr->targetFileNameStr) + 1;
  fileToStoreStr = (char *) malloc(fileToStoreStrSize);
  memset((void *) fileToStoreStr, 0x00, fileToStoreStrSize);

  strcpy(fileToStoreStr, httpGetFileDataPtr->targetFileNameStr);
  
   session = curl_easy_init(); 
  curl_easy_reset(session);
   curl_easy_setopt(session, CURLOPT_URL, httpGetFileDataPtr->fileURLStr);
   
   fp = fopen(fileToStoreStr, "w"); 
   if(NULL != fp) {
     if(0 != curl_easy_setopt(session,  CURLOPT_WRITEDATA, fp)) {
       EXEC_ERROR("Can not set CURLOPT_WRITEDATA");
     }
   
     if(0 != curl_easy_setopt(session,  CURLOPT_WRITEFUNCTION, fwrite)){
       EXEC_ERROR("Can not set CURLOPT_WRITEFUNCTION");
     }
   
     // PROXY
     if(NULL != PROXY_ADDRESS_STR) {
       DBG("Set Proxy Option");
       if(CURLE_OK != curl_easy_setopt(session, CURLOPT_PROXY, PROXY_ADDRESS_STR)) {
         EXEC_ERROR("Can not set PROXY curl option");
       }
     }
      
     if(CURLE_OK != curl_easy_setopt( session, CURLOPT_TIMEOUT, CURL_GET_FILE_TIMEOUT)) {
       EXEC_ERROR("Can not set CURLOPT_TIMEOUT curl option");
     }
   
     if(CURLE_OK != curl_easy_setopt( session, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT)) {
       EXEC_ERROR("Can not set CURLOPT_CONNECTTIMEOUT curl option");
     }
      
   
     // Usage of username and password (Check only the username. The password could be NULL)
     if(NULL != httpGetFileDataPtr->usernameStr) {
        userNamePasswordStrSize = strlen(httpGetFileDataPtr->usernameStr) + 2;
        // Add the password string size if exist.
        if(NULL != httpGetFileDataPtr->passwordStr) {
          userNamePasswordStrSize += strlen(httpGetFileDataPtr->passwordStr);
        }
        userNamePasswordStr = (char *) malloc(userNamePasswordStrSize);
        memset((void *) userNamePasswordStr, '\0', userNamePasswordStrSize);
        // Build the username:password string required by curl
        strcpy(userNamePasswordStr, httpGetFileDataPtr->usernameStr);
        strcat(userNamePasswordStr, ":");
        if(NULL != httpGetFileDataPtr->passwordStr) {
          strcat(userNamePasswordStr, httpGetFileDataPtr->passwordStr);
        }
        DBG("CURLOPT_USERPWD String: %s", userNamePasswordStr);
        // Set curl authentication option
        if(CURLE_OK != curl_easy_setopt(session, CURLOPT_USERPWD, userNamePasswordStr)) {
          EXEC_ERROR("Can not set USERPWD curl option");
        } else {
          DBG("USERPWD OPTION OK");
        }
        if(CURLE_OK != curl_easy_setopt(session, CURLOPT_HTTPAUTH, CURLAUTH_ANY)) {
          EXEC_ERROR("Can not set CURLAUTH_ANY curl option");
        } else {
          DBG("CURLAUTH_ANY OPTION OK");
        }
        
        // Allow Redirection
        if(CURLE_OK != curl_easy_setopt( session, CURLOPT_MAXREDIRS, -1)) {
          EXEC_ERROR("Can not set CURLOPT_MAXREDIRS curl option");
        } 
        if(CURLE_OK != curl_easy_setopt( session, CURLOPT_FOLLOWLOCATION, 1)) {
          EXEC_ERROR("Can not set CURLOPT_FOLLOWLOCATION curl option");
        }
        
     } else {
       DBG("WGET - No username or password");
     }  
     
     rc = curl_easy_perform(session);
     if(0 != rc) {
       WARN("Download ERROR - Can not download the file. Curl error code = %d", rc);
       if (rc == CURLE_LOGIN_DENIED) { rc = HTTP_UNAUTHORIZED; }
     } else {
       curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, &rc);
       DBG("Download completed  - curlResponseCode = %d", rc);
       if(HTTP_OK != rc) {
         WARN("Download Error");
       } else {
         DBG("Download OK");
       }
     }

     fclose(fp);
   
     curl_easy_cleanup(session);  
   } else {
     // if(NULL != fp)
     EXEC_ERROR("Can not store the file to download (%s)", fileToStoreStr);
   }

  // Free allocated memory if required
  DM_ENG_FREE(fileToStoreStr);
  DM_ENG_FREE(userNamePasswordStr);

   // Retrieve the FW Upgrade Error Code.
   rc = _getFirmwareUpgradeErrorCodeFromHttpCode(rc);

  // Quit the current thread
   return rc;
  
} // _curlGetHttpFile


// Private routine used by _curlPutHttpFile
static size_t read_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t retcode;
 
   /* in real-world cases, this would probably get this data differently
      as this fread() stuff is exactly what the library already would do
      by default internally */
   retcode = fread(ptr, size, nmemb, stream);
 
   DBG("We read %d bytes from file", retcode);
 
   return retcode;
 }

/**
 * @brief Function to put an http file using libcurl
 *
 * @param pParam  Message to send
 *
 * @Return The Upload Error Code (0 or 90xx)
 *
 */    
static int _curlPutHttpFile(httpPutFileDataType * httpPutDataPtr)
{
  int           rc = DM_ERR;
  CURL        * curl;
  FILE        * hd_src ;
  int           hd ;
  struct stat   file_info;
  int           userNamePasswordStrSize = 0;
  char        * userNamePasswordStr = NULL;

  DBG("****** _curlPutHttpFile ******");
  DBG("Destination URL: %s", httpPutDataPtr->destURLStr);

  DBG("Username:        %s", ((NULL != httpPutDataPtr->usernameStr)      ? httpPutDataPtr->usernameStr      : "None"));
  DBG("Password:        %s", ((NULL != httpPutDataPtr->passwordStr)      ? httpPutDataPtr->passwordStr      : "None"));
  DBG("Local File:      %s", ((NULL != httpPutDataPtr->localFileNameStr) ? httpPutDataPtr->localFileNameStr : "None"));    
      
  // Make sure it is a valid URL (HTTP type)
  if(0 != _isValidUrl(httpPutDataPtr->destURLStr)) {
    WARN("Invalid HTTP URL (%s)", httpPutDataPtr->destURLStr);
    return DM_ENG_UNSUPPORTED_PROTOCOL;
  }
  
  /* get the file size of the local file */
  hd = open(httpPutDataPtr->localFileNameStr, O_RDONLY);
  if(-1 == hd) {
    EXEC_ERROR("Can not open the file");
    return DM_ENG_UPLOAD_FAILURE;
  }
  fstat(hd, &file_info);
  close(hd);

  /* get a FILE * of the same file, could also be made with
    fdopen() from the previous descriptor, but hey this is just
    an example! */
  hd_src = fopen(httpPutDataPtr->localFileNameStr, "rb");
  if(NULL == hd_src) {
    EXEC_ERROR("Can not open the file");
    return DM_ENG_UPLOAD_FAILURE;
  }
  
  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);  
    
  /* get a curl handle */
  curl = curl_easy_init();
  if(curl) {
     // PROXY
     if(NULL != PROXY_ADDRESS_STR) {
       DBG("Set Proxy Option");
       if(CURLE_OK != curl_easy_setopt(curl, CURLOPT_PROXY, PROXY_ADDRESS_STR)) {
         EXEC_ERROR("Can not set PROXY curl option");
       }
     }
      
     if(CURLE_OK != curl_easy_setopt( curl, CURLOPT_TIMEOUT, CURL_PUT_FILE_TIMEOUT)) {
       EXEC_ERROR("Can not set CURLOPT_TIMEOUT curl option");
     }
   
     if(CURLE_OK != curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT)) {
       EXEC_ERROR("Can not set CURLOPT_CONNECTTIMEOUT curl option");
     }
  
  
    /* we want to use our own read function */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    
    curl_easy_setopt( curl, CURLOPT_TIMEOUT, 5);
    
    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* HTTP PUT please */
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    curl_easy_setopt(curl, CURLOPT_URL, httpPutDataPtr->destURLStr);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);

    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */
    DBG("Set CURLOPT_INFILESIZE_LARGE (%d)", (int) file_info.st_size);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);
           
     // Usage of username and password (Check only the username. The password could be NULL)
     if(NULL != httpPutDataPtr->usernameStr) {
        userNamePasswordStrSize = strlen(httpPutDataPtr->usernameStr) + 2;
        // Add the password string size if exist.
        if(NULL != httpPutDataPtr->passwordStr) {
          userNamePasswordStrSize += strlen(httpPutDataPtr->passwordStr);
        }
        userNamePasswordStr = (char *) malloc(userNamePasswordStrSize);
        memset((void *) userNamePasswordStr, '\0', userNamePasswordStrSize);
        // Build the username:password string required by curl
        strcpy(userNamePasswordStr, httpPutDataPtr->usernameStr);
        strcat(userNamePasswordStr, ":");
        if(NULL != httpPutDataPtr->passwordStr) {
          strcat(userNamePasswordStr, httpPutDataPtr->passwordStr);
        }
        DBG("CURLOPT_USERPWD String: %s", userNamePasswordStr);
        // Set curl authentication option
        if(CURLE_OK != curl_easy_setopt(curl, CURLOPT_USERPWD, userNamePasswordStr)) {
          EXEC_ERROR("Can not set USERPWD curl option");
        } else {
          DBG("USERPWD OPTION OK");
        }
        if(CURLE_OK != curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY)) {
          EXEC_ERROR("Can not set CURLAUTH_ANY curl option");
        } else {
          DBG("CURLAUTH_ANY OPTION OK");
        }
        
        // Allow Redirection
        if(CURLE_OK != curl_easy_setopt( curl, CURLOPT_MAXREDIRS, -1)) {
          EXEC_ERROR("Can not set CURLOPT_MAXREDIRS curl option");
        } 
        if(CURLE_OK != curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1)) {
          EXEC_ERROR("Can not set CURLOPT_FOLLOWLOCATION curl option");
        }
        
     } else {
       DBG("UPLOAD - No username or password");
     }       
           

    /* Now run off and do what you've been told! */
    DBG("Set curl_easy_perform");
    rc = curl_easy_perform(curl);

    DBG("Upload Result: %d", rc);

    // Retrieve the response code
    long curlRespCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &curlRespCode);
    DBG("CURLINFO_RESPONSE_CODE = %d", (int) curlRespCode);
    switch(curlRespCode) {
      case HTTP_OK:
      case HTTP_CREATED:
      case HTTP_ACCEPTED:
      case HTTP_TRANSFER_COMPLETE:
        rc = CURLE_OK;
       break;
      case HTTP_UNAUTHORIZED:
        rc = CURLE_REMOTE_ACCESS_DENIED;
      break;
      case HTTP_INTERNAL_SERVER_ERROR:
        rc = CURLE_COULDNT_CONNECT;
      break;
      default:
        rc = CURLE_COULDNT_CONNECT;
      break;
    }

    /* always cleanup */
    curl_easy_cleanup(curl);

    DM_ENG_FREE(userNamePasswordStr);

   }  else {
     // if(NULL != fp)
     EXEC_ERROR("Can not init curl handler");
   }

  fclose(hd_src); /* close the local file */

  curl_global_cleanup();

   // Retrieve the Upload Error Code.
   rc = _getUploadErrorCodeFromHttpCode(rc);

  // Quit the current thread
   return rc;
}


/**
 * @brief Function to FTP put a file using libcurl
 *
 * @param pParam  Message to send
 *
 * @Return The Upload Error Code (0 or 90xx)
 *
 */    
static int _curlPutFtpFile(httpPutFileDataType * httpPutDataPtr)
{
  int           rc = DM_ERR; 
  CURL        * curl;
  FILE        * hd_src;
  struct stat   file_info;
  char        * destUsernamePwdUrlStr     = NULL;
  int           destUsernamePwdUrlStrSize = 0;
  char        * tmpStr = NULL;

  DBG("****** _curlPutFtpFile ******");
  DBG("Destination URL: %s", httpPutDataPtr->destURLStr);

  DBG("Username:        %s", ((NULL != httpPutDataPtr->usernameStr)      ? httpPutDataPtr->usernameStr      : "None"));
  DBG("Password:        %s", ((NULL != httpPutDataPtr->passwordStr)      ? httpPutDataPtr->passwordStr      : "None"));
  DBG("Local File:      %s", ((NULL != httpPutDataPtr->localFileNameStr) ? httpPutDataPtr->localFileNameStr : "None"));
  
  // Make sure it is a valid URL (HTTP type)
  if(0 != _isValidUrl(httpPutDataPtr->destURLStr)) {
    WARN("Invalid HTTP URL (%s)", httpPutDataPtr->destURLStr);
    return DM_ENG_UNSUPPORTED_PROTOCOL;
  }

  /* get the file size of the file to send */
  if(stat(httpPutDataPtr->localFileNameStr, &file_info)) {
    DBG("Couldnt open '%s': %s\n", httpPutDataPtr->localFileNameStr, strerror(errno));
    return DM_ENG_UPLOAD_FAILURE;
  }
  
  // Build the dest URL (ftp://username:password@destUrl/DefaultFileName)
  // Compute the string size
  destUsernamePwdUrlStrSize = strlen(httpPutDataPtr->destURLStr) + strlen("@") + strlen("/") + strlen(DefaultFtpUploadFileName) + 2;
  if(NULL != httpPutDataPtr->usernameStr) destUsernamePwdUrlStrSize += (strlen(httpPutDataPtr->usernameStr) + strlen("@"));
  if(NULL != httpPutDataPtr->passwordStr) destUsernamePwdUrlStrSize += strlen(httpPutDataPtr->passwordStr);

  destUsernamePwdUrlStr = (char*)malloc(destUsernamePwdUrlStrSize);
  memset(destUsernamePwdUrlStr, 0x00, destUsernamePwdUrlStrSize);
  
  // Build the new URL String
  strcpy(destUsernamePwdUrlStr, "ftp://");
  if(NULL != httpPutDataPtr->usernameStr) {
    strcat(destUsernamePwdUrlStr, httpPutDataPtr->usernameStr);
    strcat(destUsernamePwdUrlStr, ":");
    if(NULL != httpPutDataPtr->passwordStr) {
      strcat(destUsernamePwdUrlStr, httpPutDataPtr->passwordStr);
    }
    strcat(destUsernamePwdUrlStr, "@");
  }

  size_t proLen = strlen("ftp://");
  if ((strncmp(httpPutDataPtr->destURLStr,"ftp://", proLen) == 0) || (strncmp(httpPutDataPtr->destURLStr,"FTP://", proLen) == 0))
  {
    tmpStr = httpPutDataPtr->destURLStr + proLen;
  }
  else
  {
    proLen = strlen("ftps://");
    if ((strncmp(httpPutDataPtr->destURLStr,"ftps://", proLen) == 0) || (strncmp(httpPutDataPtr->destURLStr,"FTPS://", proLen) == 0))
    {
      tmpStr = httpPutDataPtr->destURLStr + proLen;
    }
  }

  if(NULL != tmpStr) {
    strcat(destUsernamePwdUrlStr, tmpStr);
    if (*(tmpStr+strlen(tmpStr)-1) != '/') { strcat(destUsernamePwdUrlStr, "/"); }
    strcat(destUsernamePwdUrlStr, DefaultFtpUploadFileName);
    INFO("COMPUTED FTP URL: %s", destUsernamePwdUrlStr);
  }
 
  /* get a FILE * of the same file */
  hd_src = fopen(httpPutDataPtr->localFileNameStr, "rb");      
   
  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);
  
  /* get a curl handle */
  curl = curl_easy_init();

  if(curl) {
  
     // PROXY
     if(NULL != PROXY_ADDRESS_STR) {
       DBG("Set Proxy Option");
       curl_easy_setopt(curl, CURLOPT_PROXY, PROXY_ADDRESS_STR);
     }
      
      curl_easy_setopt( curl, CURLOPT_TIMEOUT, CURL_PUT_FILE_TIMEOUT);
   
      curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, CURL_CONNECT_TIMEOUT);
      
    /* we want to use our own read function */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  
    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);
  
    /* Set the size of the file to upload (optional).  If you give a *_LARGE
       option you MUST make sure that the type of the passed-in argument is a
       curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
       make sure that to pass in a type 'long' argument. */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);

    /* specify target */
    curl_easy_setopt(curl,CURLOPT_URL, destUsernamePwdUrlStr);
       
    /* Now run off and do what you've been told! */
    rc = curl_easy_perform(curl);
    
    DBG("Upload Result: res = %d\n", rc);
    
    // Retrieve the response code
    long curlRespCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &curlRespCode);
    DBG("CURLINFO_RESPONSE_CODE = %d", (int) curlRespCode);
    switch(curlRespCode) {
      case HTTP_OK:
      case HTTP_CREATED:
      case HTTP_ACCEPTED:
      case HTTP_TRANSFER_COMPLETE:
        rc = CURLE_OK;
       break;
      case HTTP_UNAUTHORIZED:
        rc = CURLE_REMOTE_ACCESS_DENIED;
      break;
      case HTTP_INTERNAL_SERVER_ERROR:
        rc = CURLE_COULDNT_CONNECT;
      break;
      default:
        rc = CURLE_COULDNT_CONNECT;
      break;
    
    }
   
    /* always cleanup */
    curl_easy_cleanup(curl);
  }
   
  fclose(hd_src); /* close the local file */
 
  curl_global_cleanup();        

  free(destUsernamePwdUrlStr);

   // Retrieve the Upload Error Code.
   rc = _getUploadErrorCodeFromHttpCode(rc);

  // Quit the current thread
   return rc;  
  
}


/**
 * @brief Call back Function called by the libcurl get the http content
 *
 * @param ptr  
 * @param size  
 * @param nmemb  
 * @param stream  Pointer to the buffer which contain the HTTP header   
 *
 * @return Size of the data received
 *
 */
static  size_t
_curlCallbackClientHeader(void   *ptr,
                        size_t  size,
                        size_t  nmemb,
                        void   *stream)
{
   int     lastHttpResponseCode;
   char  * httpMsgHeaderPtr = NULL;
   int     rc;

   if(NULL == ptr) {
     return (size * nmemb);
   }

   httpMsgHeaderPtr = (char *) malloc((size * nmemb) + 1);
   memcpy((void *) httpMsgHeaderPtr, (void *) ptr, size * nmemb);
   *(httpMsgHeaderPtr + size * nmemb) = '\0';

  if(NULL == stream) {
     // Not an error. Unused parameter. Test added to remove compil warning
   }

  // -------------------------------------------------------------------------
  // Read the response code of the HTTP message received
  // -------------------------------------------------------------------------
  _getLastHttpResponseCode(&lastHttpResponseCode);
  
  rc = DM_HttpCallbackClientHeader(httpMsgHeaderPtr, (size * nmemb), lastHttpResponseCode);

  DM_ENG_FREE(httpMsgHeaderPtr);
  
  return rc; 
}


/**
 * @brief Call back Function called by the libcurl get the http content
 *
 * @param ptr  
 * @param size  
 * @param nmemb  
 * @param stream  Pointer to the buffer which contain the HTTP content
 *
 * @return Size of the data received
 *
 */
static  size_t
_curlCallbackClientData(void   *ptr,
                        size_t  size,
                        size_t  nmemb,
                        void   *stream)
{
  int    rc;
  char * httpMsgDataPtr = NULL;
  
  if(NULL == ptr) {
    return (size * nmemb);
  }
  
  if(NULL == stream) {
     // Not an error. Unused parameter. Test added to remove compil warning
   }
   
   httpMsgDataPtr = (char *) malloc((size * nmemb) + 1);
   memcpy((void *) httpMsgDataPtr, (void *) ptr, size * nmemb);
   *(httpMsgDataPtr + size * nmemb) = '\0';
   
   rc =   DM_HttpCallbackClientData(httpMsgDataPtr, size * nmemb);
   
  DM_ENG_FREE(httpMsgDataPtr);   
  
  return rc;

}


/*
* @brief Function used to retrieve the last response code from an HTTP request
*
* @param OUT: The response code
*
* @return 0 on success (-1 otherwise)
*
*/
static int _getLastHttpResponseCode(OUT int * respCodePtr) {
  
  *respCodePtr = 0;
  curl_easy_getinfo( _sessionHandle, CURLINFO_RESPONSE_CODE, respCodePtr );
         
  if ( *respCodePtr == 0 ) {
    // Try with CURLINFO_HTTP_CONNECTCODE
     curl_easy_getinfo( _sessionHandle, CURLINFO_HTTP_CONNECTCODE, respCodePtr );
  }
  
  return DM_OK;

} // getLastHttpResponseCode

/**
 * @brief Fuction used to close the http session and clean up the curl handle
 *
 * @param None
 *
 * @return void
 *
 */
static void _curlHandleCleanUp() 
{

  DBG("_curlHandleCleanUp - Begin");

  if(NULL != _sessionHandle) {
    curl_easy_cleanup(_sessionHandle);
    _sessionHandle = NULL;
  }
  
  if(NULL != _slist) {
    curl_slist_free_all(_slist); /* free the list - List used to define the HTTP Content Type */
    _slist=NULL;
  }
  
  DBG("_curlHandleCleanUp - End");

}

static void * _sendHttpMessage()
{
   pthread_mutex_lock(&mutexHttpSendThreadControl);

  while (NULL != _httpMessageBeingSent)
  {
    // ---------------------------------------------------------------------------
    // Set the limit of the message to send
    // ---------------------------------------------------------------------------
    #ifdef X86
    curl_easy_setopt(_sessionHandle, CURLOPT_POSTFIELDSIZE_LARGE, strlen(_httpMessageBeingSent) );
    #else
    curl_easy_setopt(_sessionHandle, CURLOPT_POSTFIELDSIZE, strlen(_httpMessageBeingSent) );
    #endif
    
    // Set the relevant Content-type (No Content-type for 0 size data message)
    if(NULL != _slist) {
      // Free the list
      curl_slist_free_all(_slist); /* free the list - List used to define the HTTP Content Type */
      _slist=NULL;
    } 
    if(0 == strlen(_httpMessageBeingSent)) {
      DBG("Set empty HTTP Content-type");
      _slist = curl_slist_append(_slist, HttpEmptyContentType);
    } else {
      DBG("Set XML HTTP Content-type");
      _slist = curl_slist_append(_slist, HttpXmlContentType); 
    }
    curl_easy_setopt(_sessionHandle, CURLOPT_HTTPHEADER, _slist); 
    

    // ---------------------------------------------------------------------------
    // Fill the data to send
    // ---------------------------------------------------------------------------
    curl_easy_setopt( _sessionHandle, CURLOPT_POSTFIELDS, _httpMessageBeingSent );

    INFO("HTTP Message to send:\n%s", _httpMessageBeingSent);

    pthread_mutex_unlock(&mutexHttpSendThreadControl);

    if ( curl_easy_perform( _sessionHandle ) == 0 ) {
      INFO("HTTP Message sent");
    } else {
      WARN("HTTP Message Not Sent");
    }

    pthread_mutex_lock(&mutexHttpSendThreadControl);

    // Free and set to NULL the _httpMessageBeingSent
    DM_ENG_FREE(_httpMessageBeingSent);
    _httpMessageBeingSent = _getFirstPendingHttpMessage();

  }  // end if(NULL != _httpMessageBeingSent)

  if(_closeHttpSessionExpected)
  {
     _CloseHttpSession(NORMAL_CLOSE);
  }

  pthread_mutex_unlock(&mutexHttpSendThreadControl);

   // Quit the current thread
   pthread_exit(DM_OK);
}

/**
 * @brief Fuction used to retrieve the FW Upgrade ERROR Code 
 *        from the Curl Error Code
 *
 * @param None
 *
 * @return int - FW Upgrade Error Code
 *               0      - No Error
 *               9001   - Request Denied
 *               9002   - Internal Error
 *               9010   - Download Failure
 *               9012   - File Server Authentication Failure
 *               9013   - Unsupported protocol for file transfer
 *               9800   - Authentication check failure (invalid FW signature)
 *
 */
static int  _getFirmwareUpgradeErrorCodeFromHttpCode(int httpRc)
{

  int fwUpgradeErrorCode;
  
  DBG("HTTP Error code to convert: %d", httpRc);
  
  switch(httpRc) {
    case HTTP_OK: // No error
      fwUpgradeErrorCode = 0;
    break;
    case HTTP_TRANSFER_COMPLETE: // No error
      fwUpgradeErrorCode = 0;
    break;    
    case HTTP_UNAUTHORIZED:
      fwUpgradeErrorCode = DM_ENG_AUTHENTICATION_FAILURE;
    break;
    case HTTP_FORBIDDEN:
      fwUpgradeErrorCode = DM_ENG_REQUEST_DENIED;
    break;
    default:
      fwUpgradeErrorCode = DM_ENG_DOWNLOAD_FAILURE;
    break;
  };
  
  DBG("FW Upgrade Converted error code: %d", fwUpgradeErrorCode);
  
  return fwUpgradeErrorCode;

}

/**
 * @brief Fuction used to retrieve the FW Upgrade ERROR Code 
 *        from the Curl Error Code
 *
 * @param None
 *
 * @return int - FW Upgrade Error Code
 *               0      - No Error
 *               9001   - Request Denied
 *               9002   - Internal Error
 *               9010   - Download Failure
 *               9012   - File Server Authentication Failure
 *               9013   - Unsupported protocol for file transfer
 *               9800   - Authentication check failure (invalid FW signature)
 *
 */
static int  _getUploadErrorCodeFromHttpCode(int curlRc)
{

  int uploadErrorCode;
  
  DBG("CURL Error code to convert: %d", curlRc);
  
  switch(curlRc) {
    case CURLE_OK: // No error
      uploadErrorCode = 0;
    break;
    case CURLE_SSL_CACERT:
      uploadErrorCode = DM_ENG_AUTHENTICATION_FAILURE;
    break;
    default:
      uploadErrorCode = DM_ENG_UPLOAD_FAILURE;
    break;
  };
  
  DBG("Upload Converted error code: %d", uploadErrorCode);
  
  return uploadErrorCode;

}

/**
 * @brief Fuction used to retrieve convert a string into lower case string
 *
 * @param - The string to convert
 *
 * @return - lower case string (or NULL on error)
 *
 * This routine allocate memory. The caaling routine must free the memory.
 *
 */
static char * _stringToLower(const char * str){
  int    strLength = 0;
  int    i = 0;
  char * lowerStr  = NULL;
  
  if(NULL == str) return NULL;
  
  strLength = strlen(str);
  lowerStr = (char *) malloc(strLength + 1);
  memset((void *) lowerStr, '\0', strLength + 1);
  for(i = 0; i < strLength; i++) {
    lowerStr[i] = tolower(str[i]);  
  }
  
  return lowerStr;
}

/**
 * @brief Fuction used to check if the given string is a valid HTTP URL
 *
 * @param - The URL
 *
 * @return - 0 if it is a valid URL (-1 otherwise)
 *
 */
static int _isValidUrl(const char * urlStr) {
  char * lowerUrlStr = NULL;
  int    rc = -1;
  char   validHttpUrlToken[] = "http";
  char   validFtpUrlToken[]  = "ftp";
  
  if(NULL == urlStr) {
    WARN("Invalid Parameter");
    return rc;
  }
  
  if((strlen(urlStr) < strlen(validHttpUrlToken)) ||
     (strlen(urlStr) < strlen(validFtpUrlToken))) {
    WARN("Invalid Parameter - String size too short");
    return rc;  
  }
  
  lowerUrlStr = _stringToLower(urlStr);
  
  if((0 != strncmp(validHttpUrlToken, lowerUrlStr, strlen(validHttpUrlToken))) &&
     (0 != strncmp(validFtpUrlToken,  lowerUrlStr, strlen(validFtpUrlToken)))) {
    WARN("Invalid HTTP or FTP URL (%s)", lowerUrlStr);
  } else {
    rc = 0;
  }
  
  // Free the lower case url string
  DM_ENG_FREE(lowerUrlStr);
  
  return rc;

}


/*
Used to close the HTTP Session
*/
static void _CloseHttpSession(bool closeMode)
{
  _closeHttpSessionExpected = false;
  _freePendingHttpMessages();
  if (_httpMessageBeingSent != NULL)
  {
    if(IMMEDIATE_CLOSE == closeMode)
    {
       // Immediate close. Free the HTTP Message
       DM_ENG_FREE(_httpMessageBeingSent);
    }
    else
    {
       _closeHttpSessionExpected = true;
    }
  }

  if (!_closeHttpSessionExpected)
  {
     // Close the HTTP Session
     _curlHandleCleanUp();
  }
}
