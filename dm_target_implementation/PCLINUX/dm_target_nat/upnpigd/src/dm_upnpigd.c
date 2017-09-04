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
 * To enable this thread, add "IGD_ENABLE=Y" at compile time
 * e.g. make Target=PCLINUX TRACE_LEVEL=7 IGD_ENABLE=Y
 *---------------------------------------------------------------------------
 *
 *  TODO : Implement UPnP subscription mechanism (Event notification mechanism)
 *         Get notification message from gateway to check if external IP address has changed
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
// lib UPnP IGD's headers (pupnp)
// --------------------------------------------------------------------
#include "httpreadwrite.h"
#include "statcodes.h"
#include "upnpapi.h"
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
static char lanaddr[32] = "unset";	/* InternalClient */
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
#define IGD_SLEEP_TIME_ON_LeaseDuration (3500) /*Default value 3600, sleep time slightly smaller than 3600*/

/*local functions declaration*/
static int refreshLeaseDuration(char* ExternalPort, char* InternalClient);
static int gena_subscribe(IN const UpnpString *url, INOUT int *timeout, IN const UpnpString *renewal_sid, OUT UpnpString *sid);

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
  UpnpString *ActualSID = UpnpString_new();
  while (true) {
    sleep(IGD_SLEEP_TIME_ON_LeaseDuration);
    refreshLeaseDuration(reservedPort, lanaddr);
  }
}

/**
 * @brief create port mapping and build ConnectionRequestUrl using external ip address and external port
 *  this function is invoked in DM_ENG_Device_getNames in DM_DeviceAdapter.c every time ConnectionRequestUrl is needed
 *
 * @param pResult: string ConnectionRequestUrl
 *
 * @return 0 if success, -1 if fail
 *
 */
int DM_IGD_buildConnectionRequestUrl(char** pResult)
{
  // variables for GetExternalIPAddress()
  char extIpAddr[40] = "unset";
  // variables for AddAnyPortMapping
  int iextPort = 7547; // This port is recommoned by TR-069
  char extPort[6] = "unset";

  int strSize = 0;
  int ret = -1;
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
        UpnpString *ActualSID = UpnpString_new();
        UpnpString *PublisherURL = UpnpString_new();
        char *eventsuburl = build_absolute_url(data.urlbase, dev->descURL, data.first.eventsuburl, dev->scope_id);
        DBG("eventsuburl is %s", eventsuburl);
        UpnpString_set_String(PublisherURL, eventsuburl);
        int timeout = 3600;
        int *TimeOut = &timeout;
        int return_code = gena_subscribe(PublisherURL, TimeOut, NULL, ActualSID);
        DBG("return code of subscribe is %d", return_code);
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

/**
 * @brief activate port mappping periodicly by using the same RemoteHost(always NULL), ExternalPort, PortMappingProtocol(always TCP), InternalClient to refresh leaseDuration
 *         (keep this port mapping active)
 *
 * @param ExternalPort: the external port used for port mapping
 * @param InternalClient: ip address of internal client
 * @return 0 if success, -1 if fail
 *
 */
static int refreshLeaseDuration(char* ExternalPort, char* InternalClient){
  int ret = -1;
  if ((ExternalPort != NULL) && (InternalClient != NULL)) {

    DBG("Activate this port mapping to refresh LeaseDuration");
  	devlist = upnpDiscoverIGD(2000, multicastif, minissdpdpath,
  													 0/*localport*/, ipv6, ttl, &error);
  	if (devlist) {
      int i, reten, retam;
  		for(dev = devlist, i = 1; dev != NULL; dev = dev->pNext, i++) {
        // variables for UPNP_GetValidIGD()
        struct UPNPUrls urls;
        struct IGDdatas data;
  			int state = UPNP_GetValidIGD(dev, &urls, &data, lanaddr, sizeof(lanaddr));
  			if (state == 1) {

  				if (strcmp(lanaddr, InternalClient) == 0)/*if same InternalClient*/{
            /* for WANIPConnection:2*/
            if (0 == strcmp(dev->st, "urn:schemas-upnp-org:service:WANIPConnection:2")){

              retam = UPNP_AddAnyPortMapping(urls.controlURL /*controlURL*/, data.first.servicetype /*servicetype*/,
                                   ExternalPort /*extPort*/,
                                   inPort /*controlURL*/,
                                   InternalClient /*intClient*/,
                                   "Refresh leaseDuration" /*desc*/,
                                   proto /*proto UPPERCASE*/,
                                   remoteHost /*remoteHost*/,
                                   leaseDuration /*leaseDuration*/,
                                   reservedPort /*reservedPort*/);
              if (retam == UPNPCOMMAND_SUCCESS){
                DBG("Refresh AddAnyPortMapping Success! externalPort: %s, internalPort: %s, leaseDuration: %s\n",
                                                                  reservedPort, inPort, leaseDuration);
                ret = 0;
              } else {
                DBG("Refresh AddAnyPortMapping Error!!! Error code: %d, %s\n", retam, strupnperror(retam));
                ret = -1;
              }
            } /*else: WANIPConnection:1 and WANPPPConnection:1*/
            else {

              retam = UPNP_AddPortMapping(urls.controlURL /*controlURL*/, data.first.servicetype /*servicetype*/,
                                   ExternalPort /*extPort*/,
                                   inPort /*inPort*/,
                                   InternalClient /*intClient*/,
                                   "Refresh leaseDuration" /*desc*/,
                                   proto /*proto UPPERCASE*/,
                                   remoteHost /*remoteHost*/,
                                   leaseDuration /*leaseDuration*/);
              if (retam == UPNPCOMMAND_SUCCESS) {
                 DBG("Refresh AddPortMapping Success! externalPort: %s, internalPort: %s, leaseDuration: %s\n",
                                                                    ExternalPort, inPort, leaseDuration);
                 ret = 0;
               } else {
                 DBG("Refresh AddAnyPortMapping Error!!! Error code: %d, %s\n", retam, strupnperror(retam));
                 ret = -1;
               }
             }
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
  }
  return ret;
}

/*!
 * \brief Subscribes or renew subscription.
 *
 * \return 0 if successful, otherwise returns the appropriate error code.
 */
static int gena_subscribe(
	/*! [in] URL of service to subscribe. */
	IN const UpnpString *url,
	/*! [in,out] Subscription time desired (in secs). */
	INOUT int *timeout,
	/*! [in] for renewal, this contains a currently held subscription SID.
	 * For first time subscription, this must be NULL. */
	IN const UpnpString *renewal_sid,
	/*! [out] SID returned by the subscription or renew msg. */
	OUT UpnpString *sid)
{
	int return_code;
	int parse_ret = 0;
	int local_timeout = CP_MINIMUM_SUBSCRIPTION_TIME;
	memptr sid_hdr;
	memptr timeout_hdr;
	char timeout_str[25];
	membuffer request;
	uri_type dest_url;
	http_parser_t response;
	int rc = 0;


	UpnpString_clear(sid);

	/* request timeout to string */
	if (timeout == NULL) {
		timeout = &local_timeout;
	}
	if (*timeout < 0) {
		memset(timeout_str, 0, sizeof(timeout_str));
		strncpy(timeout_str, "infinite", sizeof(timeout_str) - 1);
	} else if(*timeout < CP_MINIMUM_SUBSCRIPTION_TIME) {
		rc = snprintf(timeout_str, sizeof(timeout_str),
			"%d", CP_MINIMUM_SUBSCRIPTION_TIME);
	} else {
		rc = snprintf(timeout_str, sizeof(timeout_str), "%d", *timeout);
	}
	if (rc < 0 || (unsigned int) rc >= sizeof(timeout_str))
		return UPNP_E_OUTOF_MEMORY;

	/* parse url */
	return_code = http_FixStrUrl(
		UpnpString_get_String(url),
		UpnpString_get_Length(url),
		&dest_url);
	if (return_code != 0) {
		return return_code;
	}

	/* make request msg */
	membuffer_init(&request);
	request.size_inc = 30;
	if (renewal_sid) {
		/* renew subscription */
		return_code = http_MakeMessage(
			&request, 1, 1,
			"q" "ssc" "sscc",
			HTTPMETHOD_SUBSCRIBE, &dest_url,
			"SID: ", UpnpString_get_String(renewal_sid),
			"TIMEOUT: Second-", timeout_str );
	} else {
		/* subscribe */
		if (dest_url.hostport.IPaddress.ss_family == AF_INET6) {
			struct sockaddr_in6* DestAddr6 = (struct sockaddr_in6*)&dest_url.hostport.IPaddress;
			return_code = http_MakeMessage(
				&request, 1, 1,
				"q" "sssdsc" "sc" "sscc",
				HTTPMETHOD_SUBSCRIBE, &dest_url,
				"CALLBACK: <http://[",
				(IN6_IS_ADDR_LINKLOCAL(&DestAddr6->sin6_addr) || strlen(gIF_IPV6_ULA_GUA) == 0) ?
					gIF_IPV6 : gIF_IPV6_ULA_GUA,
				"]:", LOCAL_PORT_V6, "/>",
				"NT: upnp:event",
				"TIMEOUT: Second-", timeout_str);
		} else {
			return_code = http_MakeMessage(
				&request, 1, 1,
				"q" "sssdsc" "sc" "sscc",
				HTTPMETHOD_SUBSCRIBE, &dest_url,
				"CALLBACK: <http://", lanaddr, ":", _CPE_PORT, "/>",
				"NT: upnp:event",
				"TIMEOUT: Second-", timeout_str);
		}
	}
	if (return_code != 0) {
		return return_code;
	}

	/* send request and get reply */
	return_code = http_RequestAndResponse(&dest_url, request.buf,
		request.length,
		HTTPMETHOD_SUBSCRIBE,
		HTTP_DEFAULT_TIMEOUT,
		&response);
	membuffer_destroy(&request);

	if (return_code != 0) {
		httpmsg_destroy(&response.msg);

		return return_code;
	}
	if (response.msg.status_code != HTTP_OK) {
		httpmsg_destroy(&response.msg);

		return UPNP_E_SUBSCRIBE_UNACCEPTED;
	}

	/* get SID and TIMEOUT */
	if (httpmsg_find_hdr(&response.msg, HDR_SID, &sid_hdr) == NULL ||
	    sid_hdr.length == 0 ||
	    httpmsg_find_hdr( &response.msg, HDR_TIMEOUT, &timeout_hdr ) == NULL ||
	    timeout_hdr.length == 0 ) {
		httpmsg_destroy( &response.msg );

		return UPNP_E_BAD_RESPONSE;
	}

	/* save timeout */
	parse_ret = matchstr(timeout_hdr.buf, timeout_hdr.length, "%iSecond-%d%0", timeout);
	if (parse_ret == PARSE_OK) {
		/* nothing to do */
	} else if (memptr_cmp_nocase(&timeout_hdr, "Second-infinite") == 0) {
		*timeout = -1;
	} else {
		httpmsg_destroy( &response.msg );

		return UPNP_E_BAD_RESPONSE;
	}
  /* save SID */
	UpnpString_set_StringN(sid, sid_hdr.buf, sid_hdr.length);
	if (UpnpString_get_String(sid) == NULL) {
		httpmsg_destroy(&response.msg);

		return UPNP_E_OUTOF_MEMORY;
	}
	httpmsg_destroy(&response.msg);

	return UPNP_E_SUCCESS;
}