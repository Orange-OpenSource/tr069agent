/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C)  Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'.
 *
 *---------------------------------------------------------------------------
 * File        : dm_upnpigd.c
 *
 * Created     : 30/05/2017
 * Author      : Ying ZENG
 *
 *---------------------------------------------------------------------------
 *
 *  TODO :
 *
 *---------------------------------------------------------------------------
 */

/**
 * @file dm_upnpigd.c
 *
 * @brief upnpigd threads implementation
 *        UPnP Internet Gateway Device
 **/


// --------------------------------------------------------------------
// System headers
// --------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>  /* for IPPROTO_TCP / IPPROTO_UDP */
#include <ctype.h>
#include <sys/time.h>				/* */
#include <arpa/inet.h>				/* For inet_aton            */
#include <assert.h>				/* */
#include <stdbool.h>				/* Need for the bool type   */
#include <unistd.h>       /*for sleep function*/
// --------------------------------------------------------------------
// lib UPnP IGD's headers (miniupnp)
// --------------------------------------------------------------------
#include "miniwget.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"
#include "miniupnpcstrings.h"
#include "dm_upnpigd.h"

// --------------------------------------------------------------------
// DM_ENGINE's header
// --------------------------------------------------------------------
#include "DM_ENG_RPCInterface.h"  		/* DM_Engine module definition    */
#include "DM_GlobalDefs.h"			/* */
#include "DM_ENG_Parameter.h"			/* */
#include "dm_com_utils.h"			/* */
#include "DM_COM_ConnectionSecurity.h"		/* */
#include "CMN_Trace.h"
#include "DM_ENG_Device.h"

#if 0
#include "DM_ENG_Device.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_DiagnosticsLauncher.h"
#include "DM_SystemCalls.h"
#include "DM_ComponentCalls.h"
#include "DM_ENG_ParameterBackInterface.h"
#include "DM_COM_GenericHttpClientInterface.h" // Required for HTTP GET FILE Example
#include "DM_CMN_Thread.h"
#endif

// extern variables
extern char * g_randomCpeUrl; // required for building ConnectionRequestURL
// --------------------------------------------------------------------
// GLOBAL VALUES
// --------------------------------------------------------------------
// variables for upnpDiscoverIGD
static const char * multicastif = 0;
static const char * minissdpdpath = 0;
static int ipv6 = 0;
static unsigned char ttl = 2;
static int error = 0;
//struct for Discover results
static struct UPNPDev * devlist = 0;
static struct UPNPDev * dev;
// variables for UPNP_GetValidIGD()
static char lanaddr[40] = "unset";	/* ip address on the LAN */
// variables for GetExternalIPAddress()
static char extIpAddr[40] = "unset";
// variables for AddAnyPortMapping
static char  reservedPort [6] = "unset";
static char * remoteHost = NULL;
static int iextPort = 7547;
static char extPort[6] = "unset";
// char extPort[6] = DM_ENG_intToString(iextPort);
static char inPort[6] = "50805";
static char * proto = "TCP";
static char leaseDuration[16]= "3600";
/*parameter name for update value*/
static char* connectionrequesturl = "ManagementServer.ConnectionRequestURL";
// --------------------------------------------------------------------
// New definitions
// --------------------------------------------------------------------
#define IGD_SLEEP_TIME_ON_LeaseDuration (3600) /*Default value 3600*/  // not sure using const or define

// --------------------------------------------------------------------
// FUNCTIONS defined in the file
// --------------------------------------------------------------------
// __stun thread functions__

// __general functions__

// __DB dialog functions__
static int DM_IGD_buildConnectionRequestUrl(char** pResult);
static bool DM_IGD_updateParameterValue(char* parameterName, DM_ENG_ParameterType type, char* value);
bool DM_IGD_getParameterValue(char* parameterName, char** value);
/**
 * @brief the dm_upnpigd thread
 *
 *
 * @param none
 *
 * @return pthread_exit
 *
 */

void*
DM_IGD_upnpigdThread()
{
  do {
    char* pResult = NULL;
    DM_IGD_getParameterValue(connectionrequesturl, &pResult);
    DBG("connectionrequesturl VALUE IS %s\n", pResult);
    free(pResult);
    DBG("Sleep for leaseDuration time then rebuild ConnectionRequestUrl/refresh leaseDuration\n");
    /*int ileaseDuration = atoi(leaseDuration);
    sleep(ileaseDuration);*/
    sleep(60);
  } while(true);
}
// --------------------------------------------------------------------
// Functions definitions
// --------------------------------------------------------------------

bool
DM_IGD_getParameterValue(char* parameterName, char** value)
{
	DM_ENG_ParameterValueStruct**	pResult = NULL;
	char*				pParameterName[2];
	int				  err;
	bool				bRet    = true;

	DBG("UPnP IGD - DM_IGD_getParameterValue - parameterName = %s", parameterName);
  *value = NULL;

	// -------------------------------------------------------------------------------
	// Initialize the array for the DM_ENGINE call with a NULL at the end
	// -------------------------------------------------------------------------------
	pParameterName[0]=parameterName;
	pParameterName[1]=NULL;

	// -------------------------------------------------------------------------------
	// Request the value of the parameter name given from the DM_Engine
	// -------------------------------------------------------------------------------
	err = DM_ENG_GetParameterValues( DM_ENG_EntityType_ANY, (char**)pParameterName, &pResult );

	if ( (err == 0) && (pResult != NULL) && (strcmp(parameterName, (char*)pResult[0]->parameterName) == 0 ) )
	{
		DBG( "Parameter name : %s = '%s' (type = %d)", (char*)pResult[0]->parameterName, (char*)pResult[0]->value, pResult[0]->type );
		if(NULL != (char*)pResult[0]->value ) {
		  *value= strdup( (char*)pResult[0]->value );
		}
		DM_ENG_deleteTabParameterValueStruct(pResult);
	} else {
		EXEC_ERROR( "error code = %d ", err);

		bRet = false;
	} // IF

	return( bRet );
} // DM_STUN_getParameterValue

/**
 * @brief Update a value from the DB
 *
 * @param parameterName (R)
 * @param DM_ENG_ParameterType (R)
 * @param value (R)
 *
 * @return Return true if okay else false
 *
 */
bool
DM_IGD_updateParameterValue(char* parameterName, DM_ENG_ParameterType type, char* value)
{
	bool bRet = false;
	DM_ENG_ParameterValueStruct*		pParameterList[2];
	DM_ENG_ParameterStatus			    status;
	DM_ENG_SetParameterValuesFault**	faults = NULL;

	pParameterList[0] = DM_ENG_newParameterValueStruct(parameterName,type, value);
	pParameterList[1] = NULL;

	DBG( "updating %s parameter with %s value", parameterName, value );
	if ( DM_ENG_SetParameterValues( DM_ENG_EntityType_SYSTEM, pParameterList, "", &status, &faults) != 0 )
	{
		EXEC_ERROR( "update not performed" );
		unsigned char i = 0;
		while ( NULL != faults[i] )
		{
			EXEC_ERROR( "Error %u while updating the '%s' parameter.", faults[i]->faultCode, faults[i]->parameterName );
			i++;
		} // WHILE
		ASSERT( FALSE );
	} else {
		bRet = true;
		DBG( "Updating the '%s'='%s' parameter : OK ", parameterName, value );
	} // IF

	// Free the memory previously allocated
	DM_ENG_deleteParameterValueStruct( pParameterList[0] );
	if ( NULL != faults )
	{
		DM_ENG_deleteTabSetParameterValuesFault( faults );
	}

	return( bRet );
} // DM_STUN_updateParameterValue


int DM_IGD_buildConnectionRequestUrl(char** pResult)
{
  int strSize = 0;
  int ret;
  DBG("Build ConnectionRequestUrl Begin!\n");
  DBG("searching UPnP IGD devices, especially WANIPConnection and WANPPPConnection\n");
	devlist = upnpDiscoverIGD(2000, multicastif, minissdpdpath,
													 0/*localport*/, ipv6, ttl, &error);
	if (devlist) {
    int i, reten, retam;
		for(dev = devlist, i = 1; dev != NULL; dev = dev->pNext, i++) {
			DBG("Found device %3d: %-48s\n", i, dev->st);
      // variables for UPNP_GetValidIGD()
      struct UPNPUrls urls;
      struct IGDdatas data;
			int state = UPNP_GetValidIGD(dev, &urls, &data, lanaddr, sizeof(lanaddr));
			if (state == 1) {
				DBG("local LAN IP Address is %s\n", lanaddr);
				int ext = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, extIpAddr);
				if (ext == 0){
					DBG("External IP Address of this network is %s\n", extIpAddr);
          /* for WANIPConnection:2*/
          if (0 == strcmp(dev->st, "urn:schemas-upnp-org:service:WANIPConnection:2")){
            sprintf(extPort, "%d", iextPort);
            retam = UPNP_AddAnyPortMapping(urls.controlURL /*controlURL*/, data.first.servicetype /*servicetype*/,
                                 extPort /*extPort*/,
                                 inPort /*controlURL*/,
                                 lanaddr /*intClient*/,
                                 "WANIPConnection:2 addanyportmapping" /*desc*/,
                                 proto /*proto UPPERCASE*/,
                                 remoteHost /*remoteHost*/,
                                 leaseDuration /*leaseDuration*/,
                                 reservedPort /*reservedPort*/);
            if (retam == UPNPCOMMAND_SUCCESS){
              DBG("AddAnyPortMapping Success! externalPort: %s, internalPort: %s, leaseDuration: %s\n",
                                                                reservedPort, inPort, leaseDuration);
              strSize = strlen("http://") + strlen(extIpAddr) + strlen(reservedPort) + 20;
              *pResult = (char*)malloc(strSize);
              memset((void *) *pResult, 0x00, strSize);
              strcpy(*pResult, "http://");
              strcat(*pResult, extIpAddr);
              strcat(*pResult, ":");
              strcat(*pResult, reservedPort);
              strcat(*pResult, "/");
              strcat(*pResult, g_randomCpeUrl);
              ret = 0;
            } else {
              DBG("AddAnyPortMapping Error!!! Error code: %d, %s\n", retam, strupnperror(retam));
              ret = -1;
            }
          } /*else: WANIPConnection:1 and WANPPPConnection:1*/
          else {
            do {
              char NewintClient[40], NewintPort[6], NewLeaseDuration[16];
              /*while port mapping exists and this port mapping entry is used by another client, increment extPort by 1 */
              do {
                  sprintf(extPort, "%d", iextPort);
                  iextPort++;
                  reten = UPNP_GetSpecificPortMappingEntry(urls.controlURL, data.first.servicetype,
                                          extPort, proto, NULL/*remoteHost*/,
                                          NewintClient, NewintPort, NULL/*desc*/,
                                          NULL/*enabled*/, NewLeaseDuration);
                  DBG("GetSpecificPortMappingEntry result %d %s\n", reten, strupnperror(reten));
                } while (reten == UPNPCOMMAND_SUCCESS && 0 != strcmp(NewintClient, lanaddr));
              /*if port mapping exists, print port mapping information*/
              if (reten == UPNPCOMMAND_SUCCESS){
                DBG("externalPort %s is redirected to %s:%s, leaseDuration: %s\n", extPort, NewintClient, NewintPort, NewLeaseDuration);
              }
              /*if port mapping exists and is used by this client or no such port mapping exists
                invoke a UPNP_AddPortMapping method */
              if ((reten == UPNPCOMMAND_SUCCESS && 0 == strcmp(NewintClient, lanaddr))
                  || (reten == 714 /*NoSuchEntryInArray*/)){
                retam = UPNP_AddPortMapping(urls.controlURL /*controlURL*/, data.first.servicetype /*servicetype*/,
                                     extPort /*extPort*/,
                                     inPort /*inPort*/,
                                     lanaddr /*intClient*/,
                                     "WANConnection:1 addportmapping" /*desc*/,
                                     proto /*proto UPPERCASE*/,
                                     remoteHost /*remoteHost*/,
                                     leaseDuration /*leaseDuration*/);
                if (retam == UPNPCOMMAND_SUCCESS) {
                  strcpy(reservedPort,extPort);
                  DBG("AddPortMapping Success! externalPort: %s, internalPort: %s, leaseDuration: %s\n",
                                                                     reservedPort, inPort, leaseDuration);
                  strSize = strlen("http://") + strlen(extIpAddr) + strlen(reservedPort) + 20;
                  *pResult = (char*)malloc(strSize);
                  memset((void *) *pResult, 0x00, strSize);
                  strcpy(*pResult, "http://");
                  strcat(*pResult, extIpAddr);
                  strcat(*pResult, ":");
                  strcat(*pResult, reservedPort);
                  strcat(*pResult, "/");
                  strcat(*pResult, g_randomCpeUrl);
                  ret = 0;
                }
              }
            } while (retam == 718 /*ConflictInMappingEntry*/);
            if (retam != UPNPCOMMAND_SUCCESS && retam != 718) {
                  DBG("AddPortMapping Error!!! Error code %d, %s\n", retam, strupnperror(retam));
                  ret = -1;
                }

            if (reten != UPNPCOMMAND_SUCCESS && reten != 714) {
                DBG("GetSpecificPortMappingEntry Error!!! Error code %d, %s\n", reten, strupnperror(reten));
                ret = -1;
              }
          }
        } else {
          DBG("Error when GetExternalIPAddress, error code %d, %s\n", ext, strupnperror(ext));
          ret = -1;
        }
        FreeUPNPUrls(&urls);
      }/*if state*/else {
          DBG("%s\n", "No valid device found by UPNP_GetValidIGD !!!");
          ret = -1;
      }
    } /*for(dev)*/
    freeUPNPDevlist(devlist); devlist=0;
  }/*if devlist*/ else {
    DBG("%s\n", "No UPnP IGD device found by upnpDiscoverIGD!!!");
    ret = -1;
  }
  return ret;
}
