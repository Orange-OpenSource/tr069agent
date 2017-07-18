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
 * Author      : Ying ZENG (Stage de fin d'etude Polytech Nantes 2017)
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
#include <stdbool.h>				/* Need for the bool type   */
#include <unistd.h>        /*for sleep function*/

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
#include "DM_GlobalDefs.h"
#include "CMN_Trace.h"
#include "DM_ENG_Parameter.h"

// extern variables
extern char * g_randomCpeUrl; // required for building ConnectionRequestURL
// --------------------------------------------------------------------
// GLOBAL VALUES
// --------------------------------------------------------------------
static char * remoteHost = NULL;  /*RemoteHost*/
static char  reservedPort [6] = "unset";  /*ExternalPort*/
static char * proto = "TCP";  /* PortMappingProtocol. Mandatory to put TCP. UDP is not working.*/
static char lanaddr[40] = "unset";	/* InternalClient */
static char inPort[6] = "50805";   /*InternalPort*/
static char leaseDuration[16]= "3600";  /*PortMappingLeaseDuration*/

// variables for upnpDiscoverIGD
static const char * multicastif = 0;
static const char * minissdpdpath = 0;
static int ipv6 = 0;
static unsigned char ttl = 2;
static int error = 0;
//struct for Discover results
static struct UPNPDev * devlist = 0;
static struct UPNPDev * dev;

// --------------------------------------------------------------------
// New definitions
// --------------------------------------------------------------------
#define IGD_SLEEP_TIME_ON_LeaseDuration 3600 /*Default value 3600*/  // not sure using const or define

// --------------------------------------------------------------------
// FUNCTIONS defined in the file
// --------------------------------------------------------------------

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
    DBG("g_randomCpeUrl is %s", g_randomCpeUrl);
    sleep(30);
  } while(true);
}
// --------------------------------------------------------------------
// Functions definitions
// --------------------------------------------------------------------


int DM_IGD_buildConnectionRequestUrl(char** pResult)
{
  // variables for GetExternalIPAddress()
  char extIpAddr[40] = "unset";
  // variables for AddAnyPortMapping
  int iextPort = 7547; // This port is recommoned by TR-069
  char extPort[6] = "unset";

  int strSize = 0;
  int ret;
  DBG("Build ConnectionRequestUrl Begin!");
  DBG("searching UPnP IGD devices ...");
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
                } while ((reten == UPNPCOMMAND_SUCCESS) && (strcmp(NewintClient, lanaddr)!=0) && (iextPort<65535));
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
            } while ((retam == 718) /*ConflictInMappingEntry*/ && (iextPort<65535));
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
          DBG("No valid device found by UPNP_GetValidIGD !!!");
          ret = -1;
      }
    } /*for(dev)*/
    freeUPNPDevlist(devlist); devlist=0;
  }/*if devlist*/ else {
    DBG("No UPnP IGD device found by upnpDiscoverIGD!!!");
    ret = -1;
  }
  return ret;
}

int refreshLeaseDuration(char* RemoteHost, char* ExternalPort, char* PortMappingProtocol, char* InternalClient){

}
