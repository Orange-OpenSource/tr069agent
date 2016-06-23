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
 * File        : DM_DeviceAdapter.c
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
 * @file DM_DeviceAdapter.c
 *
 * This file implements routine declared into DM_ENG_Device.h header.
 *
 * @brief 
 *
 **/

#include "DM_ENG_Common.h"
#include "DM_ENG_Device.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_DiagnosticsLauncher.h"

#include "DM_GlobalDefs.h"
#include "CMN_Trace.h"
#include "DM_SystemCalls.h"
#include "DM_ComponentCalls.h"
#include "DM_ENG_ParameterBackInterface.h"
#include "DM_COM_GenericHttpClientInterface.h" // Required for HTTP GET FILE Example
#include "DM_CMN_Thread.h"

#ifndef WIN32
#include <linux/unistd.h> // To get the current working directory getcwd()
#endif

#include <ctype.h>
#include <signal.h>

/* -------- PRIVATE DEFINTIONS ----------- */

#ifndef _InternetGatewayDevice_

static const char* DM_ENG_LAN_IP_PING_DIAG_OBJECT_NAME = DM_PREFIX "LAN.IPPingDiagnostics.";
static const char* DM_ENG_LAN_IP_PING_DIAG_STATE_NAME  = DM_PREFIX "LAN.IPPingDiagnostics.DiagnosticsState";
static const char* DM_ENG_LAN_IP_PING_HOST_NAME        = DM_PREFIX "LAN.IPPingDiagnostics.Host";
static const char* DM_ENG_LAN_IP_PING_NB_REPET_NAME    = DM_PREFIX "LAN.IPPingDiagnostics.NumberOfRepetitions";
static const char* DM_ENG_LAN_IP_PING_TIMEOUT_NAME     = DM_PREFIX "LAN.IPPingDiagnostics.Timeout";
static const char* DM_ENG_LAN_IP_PING_DB_SIZE_NAME     = DM_PREFIX "LAN.IPPingDiagnostics.DataBlockSize";
static const char* DM_ENG_LAN_IP_PING_DSCP_NAME        = DM_PREFIX "LAN.IPPingDiagnostics.DSCP";
static const char* DM_ENG_LAN_IP_PING_SUCCESS_NAME     = DM_PREFIX "LAN.IPPingDiagnostics.SuccessCount";
static const char* DM_ENG_LAN_IP_PING_FAILURE_NAME     = DM_PREFIX "LAN.IPPingDiagnostics.FailureCount";
static const char* DM_ENG_LAN_IP_PING_AVERAGE_NAME     = DM_PREFIX "LAN.IPPingDiagnostics.AverageResponseTime";
static const char* DM_ENG_LAN_IP_PING_MIN_NAME         = DM_PREFIX "LAN.IPPingDiagnostics.MinimumResponseTime";
static const char* DM_ENG_LAN_IP_PING_MAX_NAME         = DM_PREFIX "LAN.IPPingDiagnostics.MaximumResponseTime";

#else

static const char* DM_ENG_LAN_IP_PING_DIAG_OBJECT_NAME = DM_PREFIX "IPPingDiagnostics.";
static const char* DM_ENG_LAN_IP_PING_DIAG_STATE_NAME  = DM_PREFIX "IPPingDiagnostics.DiagnosticsState";
static const char* DM_ENG_LAN_IP_PING_HOST_NAME        = DM_PREFIX "IPPingDiagnostics.Host";
static const char* DM_ENG_LAN_IP_PING_NB_REPET_NAME    = DM_PREFIX "IPPingDiagnostics.NumberOfRepetitions";
static const char* DM_ENG_LAN_IP_PING_TIMEOUT_NAME     = DM_PREFIX "IPPingDiagnostics.Timeout";
static const char* DM_ENG_LAN_IP_PING_DB_SIZE_NAME     = DM_PREFIX "IPPingDiagnostics.DataBlockSize";
static const char* DM_ENG_LAN_IP_PING_DSCP_NAME        = DM_PREFIX "IPPingDiagnostics.DSCP";
static const char* DM_ENG_LAN_IP_PING_SUCCESS_NAME     = DM_PREFIX "IPPingDiagnostics.SuccessCount";
static const char* DM_ENG_LAN_IP_PING_FAILURE_NAME     = DM_PREFIX "IPPingDiagnostics.FailureCount";
static const char* DM_ENG_LAN_IP_PING_AVERAGE_NAME     = DM_PREFIX "IPPingDiagnostics.AverageResponseTime";
static const char* DM_ENG_LAN_IP_PING_MIN_NAME         = DM_PREFIX "IPPingDiagnostics.MinimumResponseTime";
static const char* DM_ENG_LAN_IP_PING_MAX_NAME         = DM_PREFIX "IPPingDiagnostics.MaximumResponseTime";

#endif

static const char* DM_ENG_LAN_TRACEROUTE_DIAG_OBJECT_NAME   = DM_PREFIX "LAN.TraceRouteDiagnostics.";
static const char* DM_ENG_LAN_TRACEROUTE_DIAG_STATE_NAME    = DM_PREFIX "LAN.TraceRouteDiagnostics.DiagnosticsState";
static const char* DM_ENG_LAN_TRACEROUTE_HOST_NAME          = DM_PREFIX "LAN.TraceRouteDiagnostics.Host";
static const char* DM_ENG_LAN_TRACEROUTE_TIMEOUT_NAME       = DM_PREFIX "LAN.TraceRouteDiagnostics.Timeout";
static const char* DM_ENG_LAN_TRACEROUTE_DB_SIZE_NAME       = DM_PREFIX "LAN.TraceRouteDiagnostics.DataBlockSize";
static const char* DM_ENG_LAN_TRACEROUTE_MAXHOPCOUNT_NAME   = DM_PREFIX "LAN.TraceRouteDiagnostics.MaxHopCount";
// static const char* DM_ENG_LAN_TRACEROUTE_DSCP_NAME          = DM_PREFIX "LAN.TraceRouteDiagnostics.DSCP";
static const char* DM_ENG_LAN_TRACEROUTE_RESPONSETIME_NAME  = DM_PREFIX "LAN.TraceRouteDiagnostics.ResponseTime";
static const char* DM_ENG_LAN_TRACEROUTE_ROUTEHOPS_ROOTNAME = DM_PREFIX "LAN.TraceRouteDiagnostics.RouteHops.";

#define INTERFACE_FILE "DeviceInterfaceStubFile" /* The name of the file */
char INTERFACE_PATH_FILE[256];
/* -------- File format example ---------- */
// DeviceInfo.Manufacturer <myManufacturer>
// DeviceInfo.ManufacturerOUI <myOUI>
// DeviceInfo.ProductClass <myPC>
// DeviceInfo.SerialNumber <mySN>

extern char * g_randomCpeUrl;

#define TargetFileName "TargetFileName" // Target File Name to store the downloaded file (defined into DeviceInterfaceFile)
// Default Target File Name (if not provided in the download request or in the DeviceInterfaceStubFile);
#define DefaultTargetFileName "/tmp/cwmpGetFile"
 
#define FileNameToUpload "FileNameToUpload" //  File Name to upload (defined into DeviceInterfaceFile)
// Default Upload File Name (if not provided in the in the DeviceInterfaceStubFile)
#define DefaultFileNameToUpload "./parameters.csv"


// A mutex is required because 2 threads can access to the INTERFACE_FILE in read/write access.
static DM_CMN_Mutex_t _deviceInterfaceStubFileMutex = NULL;

static DM_CMN_Mutex_t _systemDataMutex = NULL;

static volatile bool _ready = false;

static bool _downloadCompletedSuccessfully = false; // Flag to indicate if the NextSoftwareVersion must be used.
#define NextSoftwareVersion    "NextSoftwareVersionAfterDownloadSuccess" // Software version to use after a successful download 
#define X_OUI_INFORM           "X_OUI_INFORM"   // To retrieve the OUI to send when an X Event Inform is requested by the end user
#define X_EVENT_INFORM         "X_EVENT_INFORM" // To retrieve the EVENT to send when an X Event Inform is requested by the end user
#define DEFAULT_X_OUI_INFORM   "001349"         // Default value to use when no value is retrieved
#define DEFAULT_X_EVENT_INFORM "GUI"            // Default value to use when no value is retrieved
#define WAN_IP_ADDRESS_KEY     "WAN.IPAddress"  // To retreive the WAN IP Address, if it is set in the DeviceInterfaceStubFile

typedef struct infoStruct {
char * paramNameStr;
char * paramValueStr;
struct infoStruct * next;
} infoStructType;

static infoStructType * rootInfoElementPtr = NULL;
static infoStructType * lastInfoElementPtr = NULL;

#define _DEVICE_LINE_MAX (1024)

/* Private routines declaration */
static int    _setValue(const char* name, char* val);
static int    _isCommentLine(char * lineStr);                      /* Used to check if it is a comment line */
static int    _getStringName(char * linePtr, char ** nameStr, char** contStr);  /* Used to retrieve the parameter name from the string */
static int    _getStringValue(char * linePtr, char ** valueStr);   /* Used to retrieve the parameter value from the string */
static char * _retrieveValueFromInfoList(const char* nameStr);          /* get a parameter value */
static bool   _setValueToInfoList(const char* nameStr, char* valueStr); /* set a parameter value */
static int    _freeInfoList();                                     /* Free the parameter name and value list */
static int    _buildInfoList();                                    /* Build the parameter name and value list */
static int    _saveInfoList();                                     /* Save the parameter name and value list */
static void * _downloadThread(void *data);                         /* Perform download stuff simulation */
static void * _uploadThread(void *data);                           /* Perform upload stuff simulation */
static const char* _extractParameterName(const char* parameterFullPath); /* Used to remove the device type tag from the parameter name */
static int    _pingTest(DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[]);
static int    _traceroute(DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[]);

#ifndef _WithoutUI_
static void * _deviceInterfaceSimulationThread(void* data);        /* Used to simulate the Parameter Set Value and DM_ENGINE value change notification */
static void   _testAction();
#endif

#define MAX_PARAMETER_NAME_VALUE_SIZE (256)

// Mutex used to protect download sequence.
// Only one download thread can download a file at a time (make sure the same file name is
// not used at the same time by two threads)
static DM_CMN_Mutex_t _mutexDownloadThread = NULL;
static bool      _downloadThreadInProgress = false; // Indicate if a download thread is running.

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
/* ------                IMPLEMENTATION OF DM_ENG_Device.h routines           ----- */
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/**
 * Performs the necessary initializations of the device adapter if any, when starting the DM Agent.
 */ 
void DM_ENG_Device_init()
{
   DBG("Call to DM_ENG_Device_init");

   DM_CMN_Thread_initMutex(&_deviceInterfaceStubFileMutex);
   DM_CMN_Thread_initMutex(&_systemDataMutex);
   DM_CMN_Thread_initMutex(&_mutexDownloadThread);

#ifndef _WithoutUI_
  /* Start a thread for User Interface */
   DM_CMN_ThreadId_t deviceSetParam_Thread_ID = 0;
   DM_CMN_Thread_create(_deviceInterfaceSimulationThread, NULL, false, &deviceSetParam_Thread_ID);
#endif

   _ready = true;
}

/**
 * Performs the necessary operations when stopping the DM Agent.
 */ 
void DM_ENG_Device_release()
{
   DBG("Call to DM_ENG_Device_release");
   _ready = false;
   DM_CMN_Thread_destroyMutex(_deviceInterfaceStubFileMutex); _deviceInterfaceStubFileMutex = NULL;
   DM_CMN_Thread_destroyMutex(_mutexDownloadThread); _mutexDownloadThread = NULL;
   DM_CMN_Thread_destroyMutex(_systemDataMutex); _systemDataMutex = NULL;
}

static int _MAX_WAIT_LOOP = 30; // on n'attendra pas plus de 30 * 1 secondes

static void _transferLock()
{
  // Make sure only one download (or upload) thread is active at a time.
  int i;
  for (i=0; i<=_MAX_WAIT_LOOP; i++)
  {
    bool readyToDownload = false;
    DM_CMN_Thread_lockMutex(_mutexDownloadThread);
    if (!_downloadThreadInProgress || (i==_MAX_WAIT_LOOP))
    {
      // No running download thread.
      // Set the _downloadThreadInProgress flag to true
      _downloadThreadInProgress = true;
      readyToDownload = true; // exit from the wait loop
    }
    DM_CMN_Thread_unlockMutex(_mutexDownloadThread);
    if (readyToDownload) break;
    DBG("Another download thread is running. Wait the other thread completes.");
    // Wait a while
    DM_CMN_Thread_sleep(1);
  }
}

static void _transferUnlock()
{
  // Set the _downloadThreadInProgress flag to false.
  DM_CMN_Thread_lockMutex(_mutexDownloadThread);
  _downloadThreadInProgress = false;
  DM_CMN_Thread_unlockMutex(_mutexDownloadThread);
}


/**
 * Notify the Device Adapter of the beginning of a session with the ACS. It allows to perform the necessary initializations.
 */ 
void DM_ENG_Device_openSession() {
  DBG("Call to DM_ENG_Device_openSession");

  if ((_deviceInterfaceStubFileMutex == NULL) || (_mutexDownloadThread == NULL)) return; // ressource non disponible

  _transferLock(); // exlusion mutuelle avec les téléchargements (pas obligatoire. A adapter à l'implémentation des download et upload)

  // Buil the info list (info list is build from the DeviceInterfaceStubFile file) and lock the mutex
  _buildInfoList();
  }



/**
 * Notify the Device Adapter of the end of the session with the ACS.
 */ 
void DM_ENG_Device_closeSession() {
  DBG("Call to DM_ENG_Device_closeSession");

  if ((_deviceInterfaceStubFileMutex == NULL) || (_mutexDownloadThread == NULL)) return; // ressource non disponible

  // Free the info list
  _freeInfoList();
  DM_SystemCalls_freeSessionData();
  DM_ComponentCalls_freeSessionData();

  _transferUnlock();

  return;
}

static bool _splitSourceAndData(const char* data, OUT int* pSrc,OUT char** pEndData)
{
   char* end = NULL;
   long src = 0;
   if (data != NULL)
   {
      errno = 0;
      src = strtol(data, &end, 10);
      if ((errno == ERANGE) || (src<0) || (src>INT_MAX)) { return false; }
      while ((end != NULL) && (isspace(*end))) { end++; }
      if ((end != NULL) && (*end == '#')) { end++; }
   }
   *pSrc = (int)src;
   *pEndData = end;
   return true;
}

/**
 * @brief Gets the parameter value from system level.
 *
 * @param name Name of the requested parameter according to the TR-069
 * @param val Pointer to a string (char*) for return of the value.\n
 * N.B. This function must only know the system level parameters.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_getValue(const char* name, const char* systemData, OUT char** pVal)
{
  const char* paramNameStr = _extractParameterName(name); // Parameter Name without the Device Type Tag
  char * ouiStr          = NULL;
  char * productClassStr = NULL;
  char * serialNumberStr = NULL;
  int    strSize = 0;
  char * nextSoftwareVersionStr = NULL;
 
  // Set default *pVal  
  *pVal = NULL;
  if(NULL == paramNameStr) {
    EXEC_ERROR("Invalid Parameter Name");
    return 9002;
  }

  DBG("Parameter Name to Retrieve: %s", paramNameStr);

  if(0 == strcmp(paramNameStr, "ManagementServer.ConnectionRequestUsername")) {
    DBG("BUILD ConnectionRequestUsername");

    // Build the connection request UserName OUI-ProductClass-SerialNumber
    ouiStr          = _retrieveValueFromInfoList("DeviceInfo.ManufacturerOUI");
    productClassStr = _retrieveValueFromInfoList("DeviceInfo.ProductClass");
    serialNumberStr = _retrieveValueFromInfoList("DeviceInfo.SerialNumber");

    // Check value
    if((NULL == ouiStr) || (NULL == productClassStr) || (NULL == serialNumberStr)) {
      // ERROR
    } else {
      strSize = strlen(ouiStr) + strlen(productClassStr) + strlen(serialNumberStr) + 4;
      *pVal = (char*)malloc(strSize);
      memset((void *) *pVal, 0x00, strSize);
      strcpy(*pVal, ouiStr);
      strcat(*pVal, "-");
      strcat(*pVal, productClassStr);
      strcat(*pVal, "-");
      strcat(*pVal, serialNumberStr);

    }
   
  } else if(0 == strcmp(paramNameStr, "ManagementServer.ConnectionRequestURL")) {
    DBG("BUILD ConnectionRequestURL");

    // Build the Connection Request URL http://IPAddress:50805/16random
    char* ipAddressStr = _retrieveValueFromInfoList(WAN_IP_ADDRESS_KEY);
    if (NULL == ipAddressStr)
    {
       ipAddressStr = DM_SystemCalls_getIPAddress();
    }
    if(NULL == ipAddressStr) {

    } else {
      char* sCpePort = DM_ENG_intToString(CPE_PORT);
      strSize = strlen("http://") + strlen(ipAddressStr) + strlen(sCpePort) + 20;
      *pVal = (char*)malloc(strSize);
      memset((void *) *pVal, 0x00, strSize);  
      strcpy(*pVal,  "http://");
      strcat(*pVal, ipAddressStr);
      strcat(*pVal, ":");
      strcat(*pVal, sCpePort);
      strcat(*pVal, "/");
      strcat(*pVal, g_randomCpeUrl);
      free(sCpePort);
    }

  } else if((0    == strcmp(paramNameStr, "DeviceInfo.SoftwareVersion")) && 
            (true == _downloadCompletedSuccessfully)) {
      // The FW File is downloaded. Send the next software version for TEST purpose.
      DBG("Next software Version\n");
      nextSoftwareVersionStr = _retrieveValueFromInfoList(NextSoftwareVersion);
      if(NULL == nextSoftwareVersionStr) {
       EXEC_ERROR("Please insert NextSoftwareVersionAfterDownloadSuccess \"newVersion\" DeviceInterfaceStubFile ");
        nextSoftwareVersionStr = _retrieveValueFromInfoList("DeviceInfo.SoftwareVersion");
      }
      *pVal = strdup(nextSoftwareVersionStr);

  } else if (0 == strcmp(paramNameStr, "LAN.IPAddress")) {

      char* adr = DM_SystemCalls_getIPAddress();
      *pVal = (adr==NULL ? NULL : strdup(adr));

  } else if (0 == strcmp(paramNameStr, "LAN.TraceRouteDiagnostics.DiagnosticsState")) {

      *pVal = strdup(DM_ENG_NONE_STATE);

  } else {

    // On recherche d'abord le paramètre dans DeviceInterfaceStubFile, fichier qui se place en coupure vis-à-vis de l'accès système
    char * tmpStr = _retrieveValueFromInfoList(paramNameStr);

    if (tmpStr != NULL)
    {
      *pVal = strdup(tmpStr);
    }
    else
    {
       char* end = NULL;
       int src = 0;
       if (_splitSourceAndData(systemData, &src, &end))
       {
          if (src == 0)
          {
             DM_CMN_Thread_lockMutex(_systemDataMutex);
             *pVal = DM_SystemCalls_getSystemData(paramNameStr, end);
             DM_CMN_Thread_unlockMutex(_systemDataMutex);
          }
          else
          {
             *pVal = DM_ComponentCalls_getComponentData(paramNameStr, end);
          }
       }
    }
  }

  if(NULL == *pVal) {
    return 9002;
  }
  return 0;
}


/**
 * @brief Sets the parameter values
 *
 * @param Array of parameters to set (parameterName and value)
 *        The last Array Entry must be set to NULL
 *        The IN Array memory is not free by this routine.
 *
 * Note: If one parameter can not be set, no parameter is updated.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_setValues(DM_ENG_SystemParameterValueStruct* parameterList[])
{
  int     size = 0;
  int     i;
  char ** backValueStr;
  bool    getCurrentValueFlagStatus = true;
  bool    setNewValueFlagStatus     = true;
  int     iError;
  int     rc = 0;

  DBG("NEW_DM_ENG_Device_setValues - Begin");
  // Check parameter
  if(parameterList == NULL) {
    EXEC_ERROR("Invalid Parameter");
    return DM_ENG_INTERNAL_ERROR;
  }

  // Compute the Array Size
  while (parameterList[size] != NULL) { size++; }
  
  // Perform Array parameter checking
   for (i=0; i<size; i++) {  
     if(NULL == parameterList[i]->parameterName) {
       EXEC_ERROR("Invalid Array Parameter Name");
       return DM_ENG_INVALID_PARAMETER_NAME;
     }
     if(NULL == parameterList[i]->value) {
        EXEC_ERROR("Invalid Array Parameter Value");
        return DM_ENG_INVALID_PARAMETER_VALUE;
     }
   }

  // Retrieve the current values to be able to set back values in case of error.
  backValueStr = (char**)calloc(size+1, sizeof(char*));

   for (i=0; i<size; i++) { 
     // Retrieve the current value
     if(0 != DM_ENG_Device_getValue(parameterList[i]->parameterName, parameterList[i]->systemData, &backValueStr[i])) {
       // Error while retrieving back value
       WARN("Can not retrieve value for %s", parameterList[i]->parameterName);
       getCurrentValueFlagStatus = false;
       break;
     }
     DBG("BACK VALUE: %s = %s", parameterList[i]->parameterName, backValueStr[i]);
   }

   // Set the new value
   if(true == getCurrentValueFlagStatus) {
     // Set new values
     for (i=0; i<size; i++) {
      if(0 != _setValue(parameterList[i]->parameterName, parameterList[i]->value)) {
        EXEC_ERROR("Can not set value %s for %s", parameterList[i]->value, parameterList[i]->parameterName);
        setNewValueFlagStatus = false;
         iError = i;
         break;
      } else {
        DBG("Set Value OK %s = %s",  parameterList[i]->parameterName, parameterList[i]->value);
      }
     } // end for

     if(false == setNewValueFlagStatus) {
       // Set back Value
       WARN ("Set back values");
       for (i=0; i<iError; i++) {
         // Set back value.
         _setValue(parameterList[i]->parameterName, backValueStr[i]);
       }
     }
   }

  // Free backValueStr
  for (i=0; i<size; i++) {
    // Retrieve all the current value.
    DM_ENG_FREE(backValueStr[i]);
  }   
   DM_ENG_FREE(backValueStr);

   // Compute the return code
   if((true != getCurrentValueFlagStatus) || (true != setNewValueFlagStatus)) {
     rc = DM_ENG_INTERNAL_ERROR;
   }

   DBG("NEW_DM_ENG_Device_setValue - End (rc = %d)", rc);

   return rc;
}

static unsigned int lastInstanceNumber = 0;

/**
 * @brief Adds an object as specified in the TR-069.
 *
 * @param objName A partial path name corresponding to an object of system level to be created.
 * @param instanceNumber Pointer to an integer for return of the new instance number.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_addObject(const char* objName, OUT unsigned int* pInstanceNumber)
{
   WARN("ADD OBJECT objName = %s", objName);
   if (objName == NULL) { return DM_ENG_INTERNAL_ERROR; }

   int res = 0;
   time_t currentTime = time(NULL);
   unsigned int mn = currentTime / 60;
   if (mn % 2 == 0)
   {
      lastInstanceNumber++;
      *pInstanceNumber = lastInstanceNumber;
   }
   else
   {
      res = DM_ENG_INTERNAL_ERROR;
   }
   return res;
}

/**
 * @brief Deletes an object as specified in the TR-069.
 *
 * @param objName A partial path name.
 * @param instanceNumber The instance number associated with the partial path name
 * corresponding to the object to be deleted.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_deleteObject(const char* objName, unsigned int instanceNumber)
{
   WARN("DELETE OBJECT objName = %s, instanceNumber = %d", objName, instanceNumber);
   if ((objName == NULL) || (instanceNumber == 0)) { return DM_ENG_INTERNAL_ERROR; }

   time_t currentTime = time(NULL);
   unsigned int mn = currentTime / 60;
   return (mn%2 == 0 ? 0 : DM_ENG_INTERNAL_ERROR);
}

/**
 * Commands the device to reboot.
 * 
 * @param factoryReset Indicates if or not the device must perform à factory reset.
 */
void DM_ENG_Device_reboot(bool factoryReset)
{
  WARN("REBOOT IMPLEMENTED");
  DM_SystemCalls_reboot(factoryReset);
  return;
  
}

/**
 * Commands the device to perform diagnostics (ping test, traceroute, ...).
 * 
 * @param objName Name of the TR-069 object refering to the diagnostics ("Xxx...ZzzDiagnostics.")
 * @param paramList Parameter list of the arguments
 * @param pResult Pointer to the result if it is provided synchronously
 * 
 * @return 0 if the diagnostics is carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_performDiagnostics(const char* objName, DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[])
{
   int res = 9003; // Diagnostic non supporté

   if (strcmp(objName, DM_ENG_LAN_IP_PING_DIAG_OBJECT_NAME) == 0)
   {
      res = _pingTest(paramList, pResult);
   }
   else if (strcmp(objName, DM_ENG_LAN_TRACEROUTE_DIAG_OBJECT_NAME) == 0)
   {
      res = _traceroute(paramList, pResult);
   }
   return res;
}

/**
 * Gets all the names and values of parameters of a sub-tree defined by the given object.
 * 
 * @param objName Name of an object
 * @param pParamVal Pointer to a null terminated array of DM_ENG_ParameterValueStruct* which returns the couples <name, value> 
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_getObject(const char* objName, const char* data, OUT DM_ENG_ParameterValueStruct** pParamVal[])
{
   int res = 9002;
   const char* shortname = _extractParameterName(objName); // Parameter Name without the Device Type Tag

   if (shortname == NULL)
   {
      EXEC_ERROR("Invalid Parameter Name");
   }
   else if (strcmp(shortname, "LAN.DHCPOption.")==0)
   {
      int nbParam = 6;
      *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      (*pParamVal)[0] = DM_ENG_newParameterValueStruct(".4.Request", DM_ENG_ParameterType_UNDEFINED, "0");
      (*pParamVal)[1] = DM_ENG_newParameterValueStruct(".4.Tag", DM_ENG_ParameterType_UNDEFINED, "121");
      (*pParamVal)[2] = DM_ENG_newParameterValueStruct(".4.Value", DM_ENG_ParameterType_UNDEFINED, "yyy");
      (*pParamVal)[3] = DM_ENG_newParameterValueStruct(".7.Request", DM_ENG_ParameterType_UNDEFINED, "0");
      (*pParamVal)[4] = DM_ENG_newParameterValueStruct(".7.Tag", DM_ENG_ParameterType_UNDEFINED, "122");
      (*pParamVal)[5] = DM_ENG_newParameterValueStruct(".7.Value", DM_ENG_ParameterType_UNDEFINED, "ttt");
      (*pParamVal)[6] = NULL;
      res = 0;
   }
   else if ((strcmp(shortname, "LAN.Stats.")==0) && (data == NULL)) // on utilise cette version bouchonnée seulement si data == NULL
   {
      time_t currentTime;
      time(&currentTime);
      unsigned int v1 = currentTime % 1000;
      char* sV1 = DM_ENG_uintToString(v1);
      char* sV2 = DM_ENG_uintToString(v1+1);
      int nbParam = 15;
      *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      (*pParamVal)[0] = DM_ENG_newParameterValueStruct("ConnectionUpTime", DM_ENG_ParameterType_UNDEFINED, sV1);
      (*pParamVal)[1] = DM_ENG_newParameterValueStruct("TotalBytesSent", DM_ENG_ParameterType_UNDEFINED, sV2);
      (*pParamVal)[2] = DM_ENG_newParameterValueStruct("TotalBytesReceived", DM_ENG_ParameterType_UNDEFINED, "333");
      (*pParamVal)[3] = DM_ENG_newParameterValueStruct("TotalPacketsSent", DM_ENG_ParameterType_UNDEFINED, "444");
      (*pParamVal)[4] = DM_ENG_newParameterValueStruct("TotalPacketsReceived", DM_ENG_ParameterType_UNDEFINED, "555");
      (*pParamVal)[5] = DM_ENG_newParameterValueStruct("CurrentDayInterval", DM_ENG_ParameterType_UNDEFINED, "666");
      (*pParamVal)[6] = DM_ENG_newParameterValueStruct("CurrentDayBytesSent", DM_ENG_ParameterType_UNDEFINED, "777");
      (*pParamVal)[7] = DM_ENG_newParameterValueStruct("CurrentDayBytesReceived", DM_ENG_ParameterType_UNDEFINED, "888");
      (*pParamVal)[8] = DM_ENG_newParameterValueStruct("CurrentDayPacketsSent", DM_ENG_ParameterType_UNDEFINED, "999");
      (*pParamVal)[9] = DM_ENG_newParameterValueStruct("CurrentDayPacketsReceived", DM_ENG_ParameterType_UNDEFINED, "1111");
      (*pParamVal)[10] = DM_ENG_newParameterValueStruct("QuarterHourInterval", DM_ENG_ParameterType_UNDEFINED, "2222");
      (*pParamVal)[11] = DM_ENG_newParameterValueStruct("QuarterHourBytesSent", DM_ENG_ParameterType_UNDEFINED, "4444");
      (*pParamVal)[12] = DM_ENG_newParameterValueStruct("QuarterHourBytesReceived", DM_ENG_ParameterType_UNDEFINED, "5555");
      (*pParamVal)[13] = DM_ENG_newParameterValueStruct("QuarterHourPacketsSent", DM_ENG_ParameterType_UNDEFINED, "6666");
      (*pParamVal)[14] = DM_ENG_newParameterValueStruct("QuarterHourPacketsReceived", DM_ENG_ParameterType_UNDEFINED, "7777");
      (*pParamVal)[15] = NULL;
      free(sV1);
      free(sV2);
      res = 0;
   }
   else
   {
      char* endData = NULL;
      int src = 0;
      if (_splitSourceAndData(data, &src, &endData))
      {
         bool resGetObject = false;
         if (src == 0)
         {
            DM_CMN_Thread_lockMutex(_systemDataMutex);
            resGetObject = DM_SystemCalls_getSystemObjectData(shortname, pParamVal);
            DM_CMN_Thread_unlockMutex(_systemDataMutex);
         }
         else
         {
            resGetObject = DM_ComponentCalls_getComponentObjectData(shortname, pParamVal);
         }
         if (resGetObject)
         {
            res = 0;
         }
         else
         {
            if (endData != NULL)
            {
               char* separ = strchr(endData, '#');
               if (separ != NULL)
               {
                  char* sysData = (char*)malloc(separ-endData+1);
                  strncpy(sysData, endData, separ-endData);
                  sysData[separ-endData] = '\0';
                  char* values = NULL;
                  if (src == 0)
                  {
                     DM_CMN_Thread_lockMutex(_systemDataMutex);
                     values = DM_SystemCalls_getSystemData(shortname, sysData);
                     DM_CMN_Thread_unlockMutex(_systemDataMutex);
                  }
                  else
                  {
                     values = DM_ComponentCalls_getComponentData(shortname, sysData);
                  }
                  free(sysData);
                  if (values != NULL)
                  {
                     separ++;
                     char* names = strdup(separ);
                     int nbParam = 1;
                     while (*separ != '\0')
                     {
                        if (*separ == ',') { nbParam++; }
                        separ++;
                     }
                     *pParamVal = (DM_ENG_ParameterValueStruct**)calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
                     char* name = names;
                     char* value = values;
                     int i;
                     for (i=0; i<nbParam; i++)
                     {
                        separ = name;
                        while ((*separ != ',') && (*separ != '\0')) { separ++; }
                        *separ = '\0';
                        char* separVal = value;
                        if (separVal != NULL)
                        {
                           while ((*separVal != ',') && (*separVal != '\0')) { separVal++; }
                           if (*separVal == '\0') { separVal = NULL; } else { *separVal = '\0'; separVal++; }
                        }
                        (*pParamVal)[i] = DM_ENG_newParameterValueStruct(name, DM_ENG_ParameterType_UNDEFINED, value);
                        value = separVal;
                        name = separ+1;
                     }
                     res = 0;
                     free(names);
                     free(values);
                  }
               }
            }
            if (res != 0)
            {
              WARN("GET OBJECT NOT IMPLEMENTED");
            }
         }
      }
   }
   return res;
}

/**
 * Gets all the names of parameters of a sub-tree defined by the given object.
 * 
 * @param objName Name of an object
 * @param pParamVal Pointer to a null terminated array of char* which gives the names
 * 
 * @return 0 if OK, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_getNames(const char* objName, const char* data, OUT char** pNames[])
{
   DM_ENG_ParameterValueStruct** paramVal = NULL;
   int res = 9002;
   const char* shortname = _extractParameterName(objName); // Parameter Name without the Device Type Tag

   if (shortname == NULL)
   {
      EXEC_ERROR("Invalid Parameter Name");
   }
   else if (strcmp(shortname, "LAN.DHCPOption.")==0)
   {
      int nbParam = 6;
      *pNames = (char**)calloc(nbParam+1, sizeof(char*));
      (*pNames)[0] = strdup(".4.Request");
      (*pNames)[1] = strdup(".4.Tag");
      (*pNames)[2] = strdup(".4.Value");
      (*pNames)[3] = strdup(".7.Request");
      (*pNames)[4] = strdup(".7.Tag");
      (*pNames)[5] = strdup(".7.Value");
      (*pNames)[6] = NULL;
      res = 0;
   }
   else
   {
      char* endData = NULL;
      int src = 0;
      if (_splitSourceAndData(data, &src, &endData))
      {
         bool resGetObject = false;
         if (src == 0)
         {
            DM_CMN_Thread_lockMutex(_systemDataMutex);
            resGetObject = DM_SystemCalls_getSystemObjectData(shortname, &paramVal);
            DM_CMN_Thread_unlockMutex(_systemDataMutex);
         }
         else
         {
            resGetObject = DM_ComponentCalls_getComponentObjectData(shortname, &paramVal);
         }
         if (resGetObject)
         {
            int nbParam = DM_ENG_tablen((void**)paramVal);
            *pNames = (char**)calloc(nbParam+1, sizeof(char*));
            int i;
            for (i=0; i<nbParam; i++)
            {
               (*pNames)[i] = strdup(paramVal[i]->parameterName);
            }
            (*pNames)[nbParam] = NULL;
            DM_ENG_deleteTabParameterValueStruct(paramVal);
            res = 0;
         }
         else
         {
            if (endData != NULL)
            {
               char* separ = strchr(endData, '#');
               if (separ != NULL)
               {
                  separ++;
                  char* names = strdup(separ);
                  int nbParam = 1;
                  while (*separ != '\0')
                  {
                     if (*separ == ',') { nbParam++; }
                     separ++;
                  }
                  *pNames = (char**)calloc(nbParam+1, sizeof(char*));
                  char* name = names;
                  int i;
                  for (i=0; i<nbParam; i++)
                  {
                     separ = name;
                     while ((*separ != ',') && (*separ != '\0')) { separ++; }
                     *separ = '\0';
                     char* longName = NULL;
                     if (*name == '.')
                     {
                        longName = (char*)malloc(strlen(objName)+strlen(name));
                        strcpy(longName, objName);
                        strcat(longName, name+1);
                     }
                     (*pNames)[i] = (longName == NULL ? strdup(name) : longName);
                     name = separ+1;
                  }
                  res = 0;
                  free(names);
               }
            }
            if (res != 0)
            {
              WARN("GET NAMES '%s' NOT IMPLEMENTED", objName);
            }
         }
      }
   }
   return res;
}

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
             char* successURL, char* failureURL, unsigned int transferId)
{
   WARN("DOWNLOAD REQUEST");

   DM_ENG_TransferRequest* data = DM_ENG_newTransferRequest(true, 2, 0, fileType, url, username, password, fileSize, targetFileName, successURL, failureURL, NULL, NULL);
   data->transferId = transferId;
  
   /* Start a thread to perform the download and notify once completed */
   DM_CMN_ThreadId_t download_Thread_ID = 0;
   DM_CMN_Thread_create(_downloadThread, data, false, &download_Thread_ID);

   return 1;
}

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
int DM_ENG_Device_upload(char* fileType, char* url, char* username, char* password, unsigned int transferId)
{
   WARN("UPLOAD REQUEST");
  
   DM_ENG_TransferRequest* data = DM_ENG_newTransferRequest(false, 2, 0, fileType, url, username, password, 0, NULL, NULL, NULL, NULL, NULL);
   data->transferId = transferId;

   /* Start a thread to perform the download and notify once completed */
   DM_CMN_ThreadId_t upload_Thread_ID = 0;
   DM_CMN_Thread_create(_uploadThread, data, false, &upload_Thread_ID);

   return 1;
}

/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */
/* --------------------- PRIVATE ROUTINES ----------------------- */
/* -------------------------------------------------------------- */
/* -------------------------------------------------------------- */

/* return true if it is a comment line */
static int _isCommentLine(char* lineStr)
{
   return ( (NULL == lineStr) || (*lineStr == '#') || (*lineStr == ' ') || (*lineStr == '\n') );
}

/* Return 0 on success - and the name of the parameter */
/* The string name must be free by the calling routine */
static int _getStringName(char* linePtr, char** nameStr, char** contStr)
{
  if((NULL == linePtr) || (NULL == nameStr)) {
     EXEC_ERROR("Invalid Parameters");
     return -1;  
  }

  *nameStr = NULL;
  *contStr = NULL;

  // skip spaces
  while (*linePtr == ' ') {
    linePtr++;
  }

  if (*linePtr == '\0') {
     EXEC_ERROR("Invalid Parameters");
     return -1;  
  }

  // Compute the string name size
  char * _linePtr = linePtr;
  while((*_linePtr != ' ') && (*_linePtr != '"') && (*_linePtr != '\0')) {
    _linePtr++;
  }

  int stringNameSize = _linePtr - linePtr;
  if (stringNameSize == 0) {
     EXEC_ERROR("Parameter Name Missing");
     return -1;  
  }

  *nameStr = (char*)malloc(stringNameSize+1);
  memset(*nameStr, '\0', stringNameSize + 1);
  strncpy(*nameStr, linePtr, stringNameSize);

  *contStr = _linePtr;
  return 0;
}

/* Return 0 on success - and the value of the parameter */
/* The string value must be free by the calling routine */
static int _getStringValue(char* linePtr, char** valueStr)
{
  if((NULL == linePtr) || (NULL == valueStr)) {
     EXEC_ERROR("Invalid Parameters");
     return -1;  
  }

  // Init *valueStr
  *valueStr = NULL;

  // skip spaces
  while (*linePtr == ' ') {
    linePtr++;
  }

  if (*linePtr == '\0') {
     EXEC_ERROR("Parameter Value Missing");
     return -1;  
  }

  if (*linePtr != '"') {
     EXEC_ERROR("Invalid Parameter Value");
     return -1;  
  }

  char* _linePtr = ++linePtr;
  while((*_linePtr != '"') && (*_linePtr != '\0')) {
    _linePtr++;
  }

  if (*_linePtr != '"') {
     EXEC_ERROR("Invalid Parameter Value");
     return -1;
  }

  int valueLen = _linePtr - linePtr;

  char* val = (char*)malloc(valueLen+1);
  memset(val, '\0', valueLen+1);
  strncpy(val, linePtr, valueLen);

  *valueStr = val;

  val = DM_ENG_decodeValue(*valueStr);
  if (val != NULL) // il y avait des car spéciaux dans la chaîne
  {
     free(*valueStr);
     *valueStr = val;
  }

  return 0;
}

/* Return a pointer on the value corresponding to the parameter name */
/* or NULL if no parameter is found */
static char* _retrieveValueFromInfoList(const char* nameStr)
{
  infoStructType * nextInfoPtr    = NULL;
  infoStructType * currentInfoPtr = NULL;
  
  if(NULL == nameStr) {
    return NULL;
  }
  
  if(NULL == rootInfoElementPtr) return NULL;

  currentInfoPtr = rootInfoElementPtr;
  do{
    nextInfoPtr = currentInfoPtr->next;
    if(false == _isCommentLine(currentInfoPtr->paramNameStr)) {
      if(0 == strcmp(nameStr, currentInfoPtr->paramNameStr)) {
          return  (currentInfoPtr->paramValueStr);
      } 
    }
    currentInfoPtr = nextInfoPtr;
  } while(NULL != currentInfoPtr);

  return NULL;
}

static bool _setValueToInfoList(const char* nameStr, char* valueStr)
{
  infoStructType * nextInfoPtr    = NULL;
  infoStructType * currentInfoPtr = NULL;
  
  if(NULL == nameStr) {
    return false;
  }
  
  if(NULL == rootInfoElementPtr) return false;
  
  currentInfoPtr = rootInfoElementPtr;
  do{
    nextInfoPtr = currentInfoPtr->next;
    if(NULL != strstr(nameStr, currentInfoPtr->paramNameStr)) {
   if(NULL != currentInfoPtr->paramValueStr) free(currentInfoPtr->paramValueStr);
   currentInfoPtr->paramValueStr = strdup(valueStr);
   return true;
    } 
    currentInfoPtr = nextInfoPtr;
  } while(NULL != currentInfoPtr);
  return false;
}

static int _freeInfoList()
{
  infoStructType * nextInfoPtr    = NULL;
  infoStructType * currentInfoPtr = NULL;
  
  
  if(NULL == rootInfoElementPtr) return true;
  
  currentInfoPtr = rootInfoElementPtr;
  do{
    nextInfoPtr = currentInfoPtr->next;
    if(NULL != currentInfoPtr->paramNameStr)  free(currentInfoPtr->paramNameStr);
    if(NULL != currentInfoPtr->paramValueStr) free(currentInfoPtr->paramValueStr);
    free(currentInfoPtr);
    currentInfoPtr = nextInfoPtr;
  } while(NULL != currentInfoPtr);
  
    
  lastInfoElementPtr = NULL;
  rootInfoElementPtr = NULL;
  
  // Unlock the mutex
  DM_CMN_Thread_unlockMutex(_deviceInterfaceStubFileMutex);
  
  return true;
}

static int _buildInfoList()
{
  FILE           * P_FICHIER = NULL; /* pointeur sur FILE */
  infoStructType * infoPtr = NULL;
  char             line[_DEVICE_LINE_MAX];
  char           * valueStr = NULL;
  char           * nameStr  = NULL;
  char           * contStr  = NULL;  
  
  // Build the Device Inetrface Path/file
  if(NULL != DATA_PATH) {
    strcpy(INTERFACE_PATH_FILE, DATA_PATH);
    strcat(INTERFACE_PATH_FILE, "/");
    strcat(INTERFACE_PATH_FILE, INTERFACE_FILE);
  }
 
  // Take the mutex (the mutex is unlocked on _freeInfoList call).
  DM_CMN_Thread_lockMutex(_deviceInterfaceStubFileMutex);
    
  // Make sure the list is not already built.
  if(NULL != rootInfoElementPtr) {
    return 0;
  }
   
  /* Ouverture du fichier en lecture */
  P_FICHIER = fopen(INTERFACE_PATH_FILE, "r");
  
  if(NULL == P_FICHIER) {
    EXEC_ERROR("No file");
    return -1;
  }
  
  /* Fin du fichier atteint ? */
  while (fgets(line, _DEVICE_LINE_MAX, P_FICHIER) != NULL) {
    nameStr  = NULL; 
    valueStr = NULL;
    if(false == _isCommentLine(line)) {
      if(0 != _getStringName(line, &nameStr, &contStr)) {    
        EXEC_ERROR("Invalid String for Name");
         nameStr = strdup("# INVALID NAME");
      }
      if(0 != _getStringValue(contStr, &valueStr)) {
        valueStr = NULL;
      }
    } else {
      nameStr = strdup(line);
    }
      
    infoPtr = (infoStructType *) malloc(sizeof(infoStructType ));
    infoPtr->paramNameStr  = nameStr;
    infoPtr->paramValueStr = valueStr;
    infoPtr->next          = NULL;
    if(NULL == rootInfoElementPtr) {
      // First element
      rootInfoElementPtr = infoPtr;
      lastInfoElementPtr = infoPtr;
    } else {
      // Add the element at the end of the list
      lastInfoElementPtr->next = infoPtr;
      lastInfoElementPtr       = infoPtr;
    }

  }  // end while
  
  /* Fermeture du fichier !!! A ne surtout pas oublier !!!*/
  fclose(P_FICHIER); 
  
  return true;
}

static int _saveInfoList()
{
  FILE           * P_FICHIER = NULL; /* pointeur sur FILE */
  char           * lineStr = NULL;
  int              lineStrSize = 0;
  infoStructType * nextInfoPtr    = NULL;
  infoStructType * currentInfoPtr = NULL;
  
  // Build the Device Inetrface Path/file
  if(NULL != DATA_PATH) {
    strcpy(INTERFACE_PATH_FILE, DATA_PATH);
    strcat(INTERFACE_PATH_FILE, "/");
    strcat(INTERFACE_PATH_FILE, INTERFACE_FILE);
  }
    
  /* Ouverture du fichier en lecture */
  remove(INTERFACE_PATH_FILE);
  
  /* Ouverture du fichier en ecriture */
  P_FICHIER = fopen(INTERFACE_PATH_FILE, "w+");
  
  if(NULL == rootInfoElementPtr) return true;
  
  currentInfoPtr = rootInfoElementPtr;
  do{
    nextInfoPtr = currentInfoPtr->next;
    if(true == _isCommentLine(currentInfoPtr->paramNameStr)) {
      // Allocate memory for lineStr
      lineStrSize = strlen(currentInfoPtr->paramNameStr) + 5;
      lineStr = (char *) malloc(lineStrSize);
      sprintf(lineStr, "%s", currentInfoPtr->paramNameStr);
    } else {
      // Allocate memory for lineStr
      lineStrSize = strlen(currentInfoPtr->paramNameStr) + 5;
      char* val = DM_ENG_encodeValue(currentInfoPtr->paramValueStr);
      if (val == NULL) { val = currentInfoPtr->paramValueStr; }
      if (val != NULL) { lineStrSize += strlen(val); }
      lineStr = (char *) malloc(lineStrSize);

      sprintf(lineStr, "%s \"%s\"\n", currentInfoPtr->paramNameStr, (val != NULL ? val : DM_ENG_EMPTY));
      if ((val != NULL) && (val != currentInfoPtr->paramValueStr)) { free(val); }
    }

    fputs(lineStr, P_FICHIER);

    currentInfoPtr = nextInfoPtr;

    if(NULL != lineStr) {
      free(lineStr);
      lineStr=NULL;
    }

  } while(NULL != currentInfoPtr);  

  /* Fermeture du fichier !!! A ne surtout pas oublier !!!*/
  fclose(P_FICHIER);
  return true;
}

/**
 * @brief Private routine used to retrieve the parameter name from a given full path
 *  
 * @param parameterFullPath The full path where to look for the parameter name 
 *
 * @return Returns a char* with the short parameter name.
 */
static const char* _extractParameterName(const char* parameterFullPath)
{
   return ((parameterFullPath != NULL) && (strncmp(parameterFullPath, DM_PREFIX, strlen(DM_PREFIX)) == 0) ? parameterFullPath+strlen(DM_PREFIX) : NULL);
}

static void *_downloadThread(void *data)
{
  time_t                   startTime;
  time_t                   endTime;
  DM_ENG_TransferRequest * dr = (DM_ENG_TransferRequest*)data;
  httpGetFileDataType    * httpGetFileDataPtr;
  int                      rc;
  char                   * tmpStr = NULL;

  if (_mutexDownloadThread == NULL) return NULL; // ressource non disponible

  _transferLock();

  httpGetFileDataPtr = (httpGetFileDataType * ) malloc(sizeof(httpGetFileDataType));
  
  httpGetFileDataPtr->fileURLStr            = strdup(dr->url);
  httpGetFileDataPtr->filSize               = dr->fileSize;
  
  if(NULL != dr->targetFileName) {
    httpGetFileDataPtr->targetFileNameStr = strdup(dr->targetFileName); 
  } else {
    // No Target File Name. Check into DeviceInterfaceStubFile file if the default target name is specified.
    // Build the Info List from the DeviceInterfaceStubFile (and lock the mutex)
    _buildInfoList();
      
    // Retrieve the value
    tmpStr   = _retrieveValueFromInfoList(TargetFileName);
    if(NULL == tmpStr) {
      tmpStr = DefaultTargetFileName;
    }

    DBG("Download Target File Name = %s", DefaultTargetFileName);
    
    // Set the Target File Name value.
    int tmpStrSize = strlen(tmpStr) + 1;
    httpGetFileDataPtr->targetFileNameStr = (char *) malloc(tmpStrSize); 
    memset((void *) httpGetFileDataPtr->targetFileNameStr, '\0', tmpStrSize);
    sprintf(httpGetFileDataPtr->targetFileNameStr, "%s", tmpStr);
    
     // Free the Info List (and unlock the mutex)
     _freeInfoList(); 
    
  }

  if(NULL != dr->username) {
    httpGetFileDataPtr->usernameStr = strdup(dr->username);
  } else {
    httpGetFileDataPtr->usernameStr = NULL;
  }
  if(NULL != dr->password) {
    httpGetFileDataPtr->passwordStr = strdup(dr->password);
  } else {
    httpGetFileDataPtr->passwordStr = NULL;
  } 
  
  time(&startTime);
  
  rc = DM_HttpGetFile(httpGetFileDataPtr);
  
  time(&endTime);
  
  // Free memory
  DM_ENG_FREE(httpGetFileDataPtr->fileURLStr);
  DM_ENG_FREE(httpGetFileDataPtr->targetFileNameStr);
  DM_ENG_FREE(httpGetFileDataPtr->usernameStr);
  DM_ENG_FREE(httpGetFileDataPtr->passwordStr);
  DM_ENG_FREE(httpGetFileDataPtr);
   
  
  // Transfer Complete Notification is done by DM_HttpGetFile
  if( 0 == rc ) {
    // Set the download flag to true.
    bool initialDownloadCompletedSuccessfully = _downloadCompletedSuccessfully;
    _downloadCompletedSuccessfully = true;
    DBG("Download completed with success");
    
    if(false == initialDownloadCompletedSuccessfully) {
      // Software upgrade simulation
      // Notify the DM_ENGINE
      
      char* softwareVersionStr = (char *) malloc(strlen(DM_PREFIX) + strlen("DeviceInfo.SoftwareVersion") + 1);
      strcpy(softwareVersionStr, DM_PREFIX);
      strcat(softwareVersionStr, "DeviceInfo.SoftwareVersion");
      INFO("Notify Software Version Change - %s", softwareVersionStr);
      DM_ENG_ParameterManager_dataValueChanged(softwareVersionStr);
      
      // Free allocated memory
      free(softwareVersionStr);
    }
  } else {
    DBG("Download completed with ERROR");
  }
  
  // Notify the DM_Engine
  DM_ENG_ParameterManager_transferComplete(dr->transferId, rc, startTime, endTime);
  
  DM_ENG_deleteTransferRequest( dr );

  // End of download sequence
  _transferUnlock();

  return NULL;
}

static void *_uploadThread(void *data)
{
  time_t                   startTime;
  time_t                   endTime;
  DM_ENG_TransferRequest * dr = (DM_ENG_TransferRequest*)data;
  httpPutFileDataType    * httpPutFileDataPtr;
  int                      rc;
  char                   * tmpStr = NULL;

  if (_mutexDownloadThread == NULL) return NULL; // ressource non disponible

  DBG("_uploadThread - Begin");
  _transferLock();

  httpPutFileDataPtr = (httpPutFileDataType * ) malloc(sizeof(httpPutFileDataType));
  memset((void *) httpPutFileDataPtr, 0x00, sizeof(httpPutFileDataType));
  
  // Destination URL
  httpPutFileDataPtr->destURLStr  = ((NULL != dr->url) ? strdup(dr->url) : NULL);
  
  // Username
  httpPutFileDataPtr->usernameStr = ((NULL != dr->username) ? strdup(dr->username) : NULL);
  
  // Password
  httpPutFileDataPtr->passwordStr = ((NULL != dr->password) ? strdup(dr->password) : NULL);
   
  // Local File Name - Computed thanks to the File Type
  // Try to retrieve the name of the file to download from the DeviceInterfaceStubFile
  // Build the Info List from the DeviceInterfaceStubFile (and take the mutex)
  _buildInfoList();
  tmpStr = _retrieveValueFromInfoList(FileNameToUpload);
  if(NULL == tmpStr) {
    DBG("Can not retrieve the FileNameToUpload from DeviceInterfaceStubFile. Use default value (%s)", DefaultFileNameToUpload);
    tmpStr = DefaultFileNameToUpload;
  }
  httpPutFileDataPtr->localFileNameStr = strdup(tmpStr);
  // Free the Info List (and unlock the mutex)
  _freeInfoList(); 
  
  time(&startTime);
  
  if(NULL == httpPutFileDataPtr->destURLStr) {
    // No URL
    rc = DM_ENG_UPLOAD_FAILURE;
  } else {
    DBG("********* destURLStr = %s ***********", httpPutFileDataPtr->destURLStr);
  
    if(0 == strncasecmp(httpPutFileDataPtr->destURLStr, "http", sizeof("http") - 1 )) {
      DBG("HTTP Type URL");
      rc = DM_HttpPutFile(httpPutFileDataPtr);
    } else if(0 == strncasecmp(httpPutFileDataPtr->destURLStr, "ftp", sizeof("ftp") - 1 )) {
      DBG("FTP Type URL");
      rc = DM_FtpPutFile(httpPutFileDataPtr);
    } else {
      // Unsupported protocol
      rc = DM_ENG_UNSUPPORTED_PROTOCOL;
    }
  }
  
  time(&endTime);
  
  // Free memory
  DM_ENG_FREE(httpPutFileDataPtr->destURLStr);
  DM_ENG_FREE(httpPutFileDataPtr->usernameStr);
  DM_ENG_FREE(httpPutFileDataPtr->passwordStr);
  DM_ENG_FREE(httpPutFileDataPtr->localFileNameStr);
  DM_ENG_FREE(httpPutFileDataPtr);
  
  // Transfer Complete Notofication is done by DM_HttpGetFile
  if( 0 == rc ) {
    DBG("Upload completed with success");
  } else {
    DBG("Upload completed with ERROR");
  }
  
  // Notify the DM_Engine
  DM_ENG_ParameterManager_transferComplete(dr->transferId, rc, startTime, endTime);
  
  DM_ENG_deleteTransferRequest( dr );
  
  DBG("_uploadThread - End");
  _transferUnlock();

  return NULL;
}

#ifndef _WithoutUI_

static void _flush(){
 int ch;
 do
 ch = getchar();
 while ( ch != EOF && ch != '\n' );
}

/**
 * @brief Main thread entry point to simulate the device interface (allow to set value
 *        and simulate the parameter value change)
 *
 */
static void * _deviceInterfaceSimulationThread(void* data UNUSED) {
  char   action;
  char   tmp[MAX_PARAMETER_NAME_VALUE_SIZE];
  char   parameterName[MAX_PARAMETER_NAME_VALUE_SIZE];
  char   parameterValue[MAX_PARAMETER_NAME_VALUE_SIZE];
  int    i;
  char * tmpStr = NULL;
  char   x_oui_Str[MAX_PARAMETER_NAME_VALUE_SIZE];
  char   x_event_Str[MAX_PARAMETER_NAME_VALUE_SIZE];
   
  printf("Enter your choice: \n");
  printf("  Quit application               --> 0\n");
  printf("  Notify Parameter Value Change  --> 1\n");
  printf("  Send End User Request Inform   --> 2\n"); 
  printf("  Send a Request Download        --> 3\n"); 
    
  while((action = getchar()) != '0')
  {  
    _flush();
 
    switch (action)
    {
       case '1':
       {
           DBG("Request to set a new parameter value");
           memset((void *) parameterName,  0x00, MAX_PARAMETER_NAME_VALUE_SIZE);
           memset((void *) parameterValue, 0x00, MAX_PARAMETER_NAME_VALUE_SIZE);

           printf("Parameter Name (%s...): ", DM_PREFIX);
           memset((void *) tmp, 0x00, MAX_PARAMETER_NAME_VALUE_SIZE);
           fgets(tmp, MAX_PARAMETER_NAME_VALUE_SIZE - 1, stdin);
           // Remove the '\n' at the end
           for (i = 0; (tmp[i] != '\n') && (i < MAX_PARAMETER_NAME_VALUE_SIZE); i++)
           {
              parameterName[i] = tmp[i];
           }

           printf("Parameter Value: ");
           memset((void *) tmp, 0x00, MAX_PARAMETER_NAME_VALUE_SIZE);
           fgets(tmp, MAX_PARAMETER_NAME_VALUE_SIZE - 1, stdin); 
           // Remove the '\n' at the end
           for (i = 0; (tmp[i] != '\n') && (i < MAX_PARAMETER_NAME_VALUE_SIZE); i++)
           {
              parameterValue[i] = tmp[i];
           }

           if ((0 == strlen(parameterName)) || (0 == strlen(parameterValue)))
           {
              printf("Invalid Parameter\n");
           }
           else
           { 
              // Build the Info List from the DeviceInterfaceStubFile (and lock the mutex)
              _buildInfoList();

              // set the new value
              _setValue(parameterName, parameterValue);

              // Free the Info List (and unlock the mutex)
              _freeInfoList(); 

              // Notify the DM_ENGINE     
              DM_ENG_ParameterManager_dataNewValue(parameterName, parameterValue);
              //DM_ENG_ParameterManager_dataValueChanged(parameterName);
           }
       }
       break;

       case '2':
       {
          DBG("Request to send a End User Requested Inform"); 

          // Build the Info List from the DeviceInterfaceStubFile (and take the mutex)
          _buildInfoList();

          // End user request Inform
          // Get the OUI string
          tmpStr = _retrieveValueFromInfoList(X_OUI_INFORM);
          if (NULL == tmpStr)
          {
             tmpStr = DEFAULT_X_OUI_INFORM;
          }
          strcpy(x_oui_Str, tmpStr);
          DBG("X OUI = %s", x_oui_Str);

          // Get the EVENT string
          tmpStr = NULL;
          tmpStr = _retrieveValueFromInfoList(X_EVENT_INFORM);
          if (NULL == tmpStr)
          {
             tmpStr = DEFAULT_X_EVENT_INFORM;
          }
          strcpy(x_event_Str, tmpStr);
          DBG("X EVENT = %s", x_event_Str);

          // Free the Info List (and unlock the mutex)
          _freeInfoList(); 

          DM_ENG_ParameterManager_vendorSpecificEvent(x_oui_Str, x_event_Str);
       }
       break;

       case '3':
       {
          DBG("Request to send a Request Download"); 

          DM_ENG_ArgStruct* args[4];
          args[0] = DM_ENG_newArgStruct("aa0", "v0");
          args[1] = DM_ENG_newArgStruct("a1", "vvvv1");
          args[2] = DM_ENG_newArgStruct("aaa2", "vv2");
          args[3] = NULL;
          DM_ENG_ParameterManager_requestDownload("Firmware Upgrade", args);
          DM_ENG_deleteArgStruct(args[0]);
          DM_ENG_deleteArgStruct(args[1]);
          DM_ENG_deleteArgStruct(args[2]);
       }
       break;

       case 't':
          _testAction();
          break;

       case '4':
       case '5':
       case '6':
       case '7':
       case '8':
       case '9':
       case ':':
          {
             DM_ComponentCalls_setComponentData(NULL, DM_ENG_intToString(action-'4'));
          }
          break;

       default:
          break;
    }

    DM_CMN_Thread_sleep(2);
    printf("Enter your choice: \n");
    printf("  Quit application               --> 0\n");
    printf("  Notify Parameter Value Change  --> 1\n");
    printf("  Send End User Request Inform   --> 2\n"); 
    printf("  Send a Request Download        --> 3\n"); 
    DM_CMN_Thread_sleep(3);
  }

  printf("STOP THE TR069 Agent (Send SIGTERM to PID %d)\n", (int)getpid());
#ifndef WIN32
  kill((int)getpid(), SIGTERM);
#else
  DM_Agent_Stop(0);
#endif

  return 0;
}

#endif

/**
 * @brief Sets the parameter value.
 *
 * @param name Name of the parameter according to the TR-069
 * @param val String value
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
static int _setValue(const char* name, char* val)
{
   bool res = false;
  const char* paramNameStr = _extractParameterName(name); // Parameter Name without the Device Type Tag
  
   if((paramNameStr != NULL) && (val != NULL))
   {
      // Traiter ci-dessous les cas particuliers
      if (0 == strcmp(paramNameStr, "LAN.Example"))
      {
         // ...
      }
      else if (_setValueToInfoList(paramNameStr, val))
      {
         _saveInfoList();
         res = true;
      }
      else
      {
         DM_CMN_Thread_lockMutex(_systemDataMutex);
         res = DM_SystemCalls_setSystemData(paramNameStr, val);
         DM_CMN_Thread_unlockMutex(_systemDataMutex);
      }
   }

   return (res ? 0 : 9002);
}

static int _pingTest(DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[])
{
   int res = 1; // par défaut, résultat asynchrone

   char* host = NULL;
   int numberOfRepetitions = 4;
   long timeout = 4000;
   int dataSize = 56;
   int dscp = 0;
   int i;
   for (i=0; paramList[i] != NULL; i++) // on récupère les valeurs qui peuvent être dans le désordre
   {
      if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_IP_PING_HOST_NAME)==0)
      {
         if (paramList[i]->value != NULL) { host = paramList[i]->value; }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_IP_PING_NB_REPET_NAME)==0)
      {
         if (paramList[i]->value != NULL) { numberOfRepetitions = atoi(paramList[i]->value); }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_IP_PING_TIMEOUT_NAME)==0)
      {
         if (paramList[i]->value != NULL) { timeout = atol(paramList[i]->value); }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_IP_PING_DB_SIZE_NAME)==0)
      {
         if (paramList[i]->value != NULL) { dataSize = atoi(paramList[i]->value); }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_IP_PING_DSCP_NAME)==0)
      {
         if (paramList[i]->value != NULL) { dscp = atoi(paramList[i]->value); }
      }
   }

   if (host == NULL) // sinon on laisse tomber le test
   {
      // DiagnosticsState positionné à "Error_Internal"
      *pResult = DM_ENG_newTabParameterValueStruct(1);
      (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_ERROR_INTERNAL_STATE);
      (*pResult)[1] =NULL;

      res = 0; // résultat fourni de façon synchrone
   }
   else
   {
      // Performing the ping test ...
      bool foundHost = false;
      int recus=0, perdus=0, moy=0, min=0, max=0;
      DM_SystemCalls_ping(host, numberOfRepetitions, timeout, dataSize, dscp, &foundHost, &recus, &perdus, &moy, &min, &max);

      if (!foundHost)
      {
         // DiagnosticsState positionné à "Error_CannotResolveHostName"
         *pResult = DM_ENG_newTabParameterValueStruct(1);
         (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_ERROR_CANNOT_RESOLVE_STATE);
         (*pResult)[1] =NULL;

         res = 0; // résultat fourni de façon synchrone
      }
      else
      {
         // Using and returning the results
         *pResult = DM_ENG_newTabParameterValueStruct(6);
         (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_COMPLETE_STATE);
         char* str = DM_ENG_intToString(recus);
         (*pResult)[1] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_SUCCESS_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         str = DM_ENG_intToString(perdus);
         (*pResult)[2] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_FAILURE_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         str = DM_ENG_longToString(moy);
         (*pResult)[3] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_AVERAGE_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         str = DM_ENG_longToString(min);
         (*pResult)[4] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_MIN_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         str = DM_ENG_longToString(max);
         (*pResult)[5] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_IP_PING_MAX_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         (*pResult)[6] = NULL;

         res = 0; // résultat fourni de façon synchrone
      }
   }

   return res;
}

static int _traceroute(DM_ENG_ParameterValueStruct* paramList[], OUT DM_ENG_ParameterValueStruct** pResult[])
{
   int res = 1; // par défaut, résultat asynchrone

   char* host = NULL;
   long timeout = 5000;
   int dataBlockSize = 40;
   int maxHopCount = 30;
   int i;
   for (i=0; paramList[i] != NULL; i++) // on récupère les valeurs qui peuvent être dans le désordre
   {
      if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_TRACEROUTE_HOST_NAME)==0)
      {
         if (paramList[i]->value != NULL) { host = paramList[i]->value; }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_TRACEROUTE_TIMEOUT_NAME)==0)
      {
         if (paramList[i]->value != NULL) { timeout = atol(paramList[i]->value); }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_TRACEROUTE_DB_SIZE_NAME)==0)
      {
         if (paramList[i]->value != NULL) { dataBlockSize = atoi(paramList[i]->value); }
      }
      else if (strcmp(paramList[i]->parameterName, DM_ENG_LAN_TRACEROUTE_MAXHOPCOUNT_NAME)==0)
      {
         if (paramList[i]->value != NULL) { maxHopCount = atoi(paramList[i]->value); }
      }
   }

   if (host == NULL) // sinon on laisse tomber le test
   {
      // DiagnosticsState positionné à "Error_Internal"
      *pResult = DM_ENG_newTabParameterValueStruct(1);
      (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_TRACEROUTE_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_ERROR_INTERNAL_STATE);
      (*pResult)[1] =NULL;

      res = 0; // résultat fourni de façon synchrone
   }
   else
   {
      // Performing the trace route ...
      bool foundHost = false;
      int responseTime = 0;
      char** routeHops = NULL;
      DM_SystemCalls_traceroute(host, timeout, dataBlockSize, maxHopCount, &foundHost, &responseTime, &routeHops);

      if (!foundHost)
      {
         // DiagnosticsState positionné à "Error_CannotResolveHostName"
         *pResult = DM_ENG_newTabParameterValueStruct(1);
         (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_TRACEROUTE_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_ERROR_CANNOT_RESOLVE_STATE);
         (*pResult)[1] =NULL;

         res = 0; // résultat fourni de façon synchrone
      }
      else
      {
         // Using and returning the results
         int nbHops = 0;
         while (routeHops[nbHops] != NULL) { nbHops++; }
         *pResult = DM_ENG_newTabParameterValueStruct(nbHops+3);
         (*pResult)[0] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_TRACEROUTE_DIAG_OBJECT_NAME, DM_ENG_ParameterType_UNDEFINED, NULL);
         (*pResult)[1] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_TRACEROUTE_DIAG_STATE_NAME, DM_ENG_ParameterType_UNDEFINED, DM_ENG_COMPLETE_STATE);
         char* str = DM_ENG_intToString(responseTime);
         (*pResult)[2] = DM_ENG_newParameterValueStruct(DM_ENG_LAN_TRACEROUTE_RESPONSETIME_NAME, DM_ENG_ParameterType_UNDEFINED, str);
         free(str);
         int i;
         char buf[100];
         for (i=0; routeHops[i] != NULL; i++)
         {
            sprintf(buf, "%s%d.HopHost", DM_ENG_LAN_TRACEROUTE_ROUTEHOPS_ROOTNAME, (i+1));
            (*pResult)[i+3] = DM_ENG_newParameterValueStruct(buf, DM_ENG_ParameterType_UNDEFINED, routeHops[i]);
            free(routeHops[i]);
         }
         free(routeHops);

         res = 0; // résultat fourni de façon synchrone
      }
   }

   return res;
}

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
int DM_ENG_Device_startSampling(const char* objName, time_t timeRef, unsigned int sampleInterval, const char* args)
{
   if (!_ready) return 1; // device not ready

   return DM_ENG_ComponentCalls_startSampling(objName, timeRef, sampleInterval, args);
}

/**
 * Stop the collection of statistics. 
 * Polling mode : polling function should no longer called.
 * Event mode : no more calls to the callback function.
 */
int DM_ENG_Device_stopSampling(const char* objName)
{
   return DM_ENG_ComponentCalls_stopSampling(objName);
}

/**
 * 
 * @param timeStampBefore Data for this time stamp are already passed. They should not be provided again.
 * If no new data are available, no new DM_ENG_SampleDataStruct is created. A non zero code (1) is returned.
 */
int DM_ENG_Device_getSampleData(const char* objName, time_t timeStampBefore, const char* data, OUT DM_ENG_SampleDataStruct** pSampleData)
{
   int res = 1; // objet non pris en charge
   char* endData = NULL;
   int src = 0;
   if (_splitSourceAndData(data, &src, &endData))
   {
      if (src == 0)
      {
         DM_ENG_ParameterValueStruct** parameterList = NULL;
         res = DM_ENG_Device_getObject(objName, data, &parameterList);
         if (res == 0)
         {
            *pSampleData = DM_ENG_newSampleDataStruct(objName, parameterList, time(NULL), true, false); // N.B. continued à true. Pourrait être positionné à !_starting, ou autre...
            DM_ENG_deleteTabParameterValueStruct(parameterList);
         }
         else
         {
            printf("Polling de données statistiques sur objet inconnu : %s\n", objName);
         }
      }
      else
      {
         res = DM_ENG_ComponentCalls_getSampleData(objName, timeStampBefore, endData, pSampleData);
      }
   }
   return res;
}

#ifndef _WithoutUI_

static char* _statObject = DM_PREFIX "Services.STBService.1.ServiceMonitoring.MainStream.1.RTPStats.";

static void _testAction()
{
   // notification spontanée de nouvelles données statistiques
   DM_ENG_SampleDataStruct* sampleData = NULL;
   if (DM_ENG_Device_getSampleData(_statObject, 0, "1", &sampleData) == 0)
   {
      DM_ENG_ParameterManager_sampleData(sampleData);
   }
}

#endif
